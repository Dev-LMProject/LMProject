#include "LMCombat/LMAbilityTaskWeaponHitTrace.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"
#include "LMCombat/LMAnimNotifyStateAttackHitWindow.h"
#include "LMCombat/LMAttackData.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "Templates/UnrealTemplate.h"

/** 입력만 보관하고 실제 검증과 이벤트 연결은 Activate에서 수행한다. */
ULMAbilityTaskWeaponHitTrace* ULMAbilityTaskWeaponHitTrace::CreateWeaponHitTraceTask(UGameplayAbility* OwningAbility, UAnimMontage* InAttackMontage, ULMAttackData* InAttackData)
{
    if (!IsValid(OwningAbility))
    {
        return nullptr;
    }

    ULMAbilityTaskWeaponHitTrace* Task = NewAbilityTask<ULMAbilityTaskWeaponHitTrace>(OwningAbility);
    Task->AttackMontage = InAttackMontage;
    Task->AttackData = InAttackData;

    return Task;
}

/** 몽타주의 타격 구간들을 수집하고 이번 공격 실행의 알림을 구독한다. */
void ULMAbilityTaskWeaponHitTrace::Activate()
{
    Super::Activate();

    AttackerCharacter = IsValid(Ability.Get()) ? Cast<ALMBaseCharacter>(Ability->GetAvatarActorFromActorInfo()) : nullptr;
    AttackerMesh = AttackerCharacter.IsValid() ? AttackerCharacter->GetMesh() : nullptr;
    AttackerAnimInstance = AttackerMesh.IsValid() ? AttackerMesh->GetAnimInstance() : nullptr;

    if (!AttackerCharacter.IsValid() || !AttackerMesh.IsValid() || !AttackerAnimInstance.IsValid() || !IsValid(AttackMontage.Get()) || !IsValid(AttackData.Get()))
    {
        StopTraceWithWarning(TEXT("Missing attacker, mesh, animation instance, montage or default attack data."));
        return;
    }

    for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
    {
        ULMAnimNotifyStateAttackHitWindow* WindowNotify = Cast<ULMAnimNotifyStateAttackHitWindow>(NotifyEvent.NotifyStateClass.Get());

        if (!IsValid(WindowNotify))
        {
            continue;
        }

        // 해당 타격에 별도 데이터가 없으면 GA에 지정한 기본 데이터를 사용한다.
        ULMAttackData* WindowData = IsValid(WindowNotify->AttackDataOverride.Get()) ? WindowNotify->AttackDataOverride.Get() : AttackData.Get();

        if (!IsValid(WindowData) || !FMath::IsFinite(WindowData->BaseDamage) || WindowData->BaseDamage < 0.0f || !FMath::IsFinite(WindowData->TraceRadius) || WindowData->TraceRadius <= 0.0f || WindowData->TraceChannel >= ECC_MAX || WindowData->TraceChannel == ECC_OverlapAll_Deprecated || WindowData->TraceStartSocket == WindowData->TraceEndSocket || !AttackerMesh->DoesSocketExist(WindowData->TraceStartSocket) || !AttackerMesh->DoesSocketExist(WindowData->TraceEndSocket))
        {
            StopTraceWithWarning(TEXT("Check attack data, trace channel, positive radius and both blade sockets."));
            return;
        }

        const float StartTime = NotifyEvent.GetTriggerTime();
        const float EndTime = NotifyEvent.GetEndTriggerTime();

        if (!FMath::IsFinite(StartTime) || !FMath::IsFinite(EndTime) || EndTime <= StartTime || NotifyEvent.MontageTickType != EMontageNotifyTickType::BranchingPoint)
        {
            StopTraceWithWarning(TEXT("Each hit window needs a positive duration and Branching Point timing."));
            return;
        }

        const int32 SectionIndex = AttackMontage->GetSectionIndexFromPosition(StartTime);

        if (SectionIndex == INDEX_NONE)
        {
            StopTraceWithWarning(TEXT("A hit window is outside the montage sections."));
            return;
        }

        float SectionStart = 0.0f;
        float SectionEnd = 0.0f;
        AttackMontage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);

        // 섹션 시작·끝에 걸친 타격창은 점프나 반복 경계와 충돌하므로 허용하지 않는다.
        if (StartTime <= SectionStart + UE_KINDA_SMALL_NUMBER || EndTime >= SectionEnd - UE_KINDA_SMALL_NUMBER)
        {
            StopTraceWithWarning(TEXT("Place each hit window strictly inside one montage section."));
            return;
        }

        for (const FLMWeaponHitWindowState& ExistingWindow : HitWindows)
        {
            // 현재 검 하나의 타격창은 서로 겹치거나 같은 경계 시간을 공유하지 않게 한다.
            if (ExistingWindow.Notify.Get() == WindowNotify || (StartTime <= ExistingWindow.EndTime + UE_KINDA_SMALL_NUMBER && EndTime >= ExistingWindow.StartTime - UE_KINDA_SMALL_NUMBER))
            {
                StopTraceWithWarning(TEXT("Separate hit windows and use a distinct NotifyState placement for each hit."));
                return;
            }
        }

        FLMWeaponHitWindowState& Window = HitWindows.AddDefaulted_GetRef();
        Window.Notify = WindowNotify;
        Window.AttackData = WindowData;
        Window.StartTime = StartTime;
        Window.EndTime = EndTime;
    }

    if (HitWindows.IsEmpty())
    {
        StopTraceWithWarning(TEXT("Add LM Attack Hit Window to this attack montage."));
        return;
    }

    FAnimMontageInstance* Instance = AttackerAnimInstance->GetActiveInstanceForMontage(AttackMontage.Get());
    MontageInstanceID = Instance ? Instance->GetInstanceID() : INDEX_NONE;

    // Notify 이벤트의 float 값에 담긴 실행 ID와 정확하게 비교할 수 있는 범위다.
    if (MontageInstanceID < 0 || MontageInstanceID > 16777216 || !GetValidAttackInstance())
    {
        StopTraceWithWarning(TEXT("The attack montage execution or attacker combat attributes are not ready."));
        return;
    }

    HitWindowEventTags.AddTag(LMGameplayTags::Event_Combat_HitWindow_Begin);
    HitWindowEventTags.AddTag(LMGameplayTags::Event_Combat_HitWindow_End);
    HitWindowEventTags.AddTag(LMGameplayTags::Event_Combat_HitWindow_Cancel);

    // Notify에서는 경계만 받고, 실제 소켓 위치는 포즈 갱신 완료 후 읽는다.
    GameplayEventHandle = AbilitySystemComponent->AddGameplayEventTagContainerDelegate(HitWindowEventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ULMAbilityTaskWeaponHitTrace::HandleHitWindowEvent));
    PoseFinalizedHandle = AttackerMesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ULMAbilityTaskWeaponHitTrace::HandlePoseFinalized));

    if (!GameplayEventHandle.IsValid() || !PoseFinalizedHandle.IsValid())
    {
        StopTraceWithWarning(TEXT("Failed to register hit-window or pose callbacks."));
    }
}

/** 같은 공격 실행인지 검사하되 특수 공격의 Montage Pause 상태는 유지한다. */
FAnimMontageInstance* ULMAbilityTaskWeaponHitTrace::GetValidAttackInstance() const
{
    ALMBaseCharacter* Attacker = AttackerCharacter.Get();
    UAbilitySystemComponent* AttackerASC = AbilitySystemComponent.Get();

    if (bStopped || !IsValid(Ability.Get()) || !Ability->IsActive() || !IsValid(Attacker) || Attacker->IsActorBeingDestroyed() || !Attacker->HasAuthority() || !IsValid(AttackerASC) || !AttackerMesh.IsValid() || !AttackerAnimInstance.IsValid() || !IsValid(AttackMontage.Get()) || !IsValid(AttackData.Get()))
    {
        return nullptr;
    }

    // PlayerState가 ASC를 소유해도 실제 공격 몸체인 Avatar는 일치해야 한다.
    if (Ability->GetAvatarActorFromActorInfo() != Attacker || Attacker->GetAbilitySystemComponent() != AttackerASC || AttackerASC->GetAvatarActor() != Attacker || AttackerASC->GetAnimatingAbility() != Ability.Get() || Ability->GetCurrentMontage() != AttackMontage.Get() || Attacker->GetMesh() != AttackerMesh.Get() || AttackerMesh->GetAnimInstance() != AttackerAnimInstance.Get())
    {
        return nullptr;
    }

    const ULMCombatAttributeSet* AttackerAttributes = AttackerASC->GetSet<ULMCombatAttributeSet>();

    if (!AttackerAttributes || !FMath::IsFinite(AttackerAttributes->GetHealth()) || AttackerAttributes->GetHealth() <= 0.0f || AttackerASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead) || AttackerASC->HasMatchingGameplayTag(LMGameplayTags::State_Stunned))
    {
        return nullptr;
    }

    FAnimMontageInstance* Instance = AttackerAnimInstance->GetActiveInstanceForMontage(AttackMontage.Get());

    // IsPlaying은 Pause 때 false가 되므로 여기서는 종료 조건으로 사용하지 않는다.
    if (!Instance || Instance->GetInstanceID() != MontageInstanceID || Instance->IsStopped() || !FMath::IsFinite(Instance->GetPlayRate()) || Instance->GetPlayRate() <= 0.0f || !FMath::IsFinite(AttackMontage->RateScale) || AttackMontage->RateScale <= 0.0f)
    {
        return nullptr;
    }

    return Instance;
}

/** 이벤트의 공격자·몽타주·재생 ID·Notify 객체를 확인한 뒤 해당 타격창만 변경한다. */
void ULMAbilityTaskWeaponHitTrace::HandleHitWindowEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
    FAnimMontageInstance* Instance = GetValidAttackInstance();

    if (!Payload || !Instance)
    {
        return;
    }

    if (Payload->Instigator.Get() != AttackerCharacter.Get() || Payload->OptionalObject.Get() != AttackMontage.Get() || !FMath::IsFinite(Payload->EventMagnitude) || Payload->EventMagnitude != static_cast<float>(MontageInstanceID))
    {
        return;
    }

    for (FLMWeaponHitWindowState& Window : HitWindows)
    {
        if (Window.Notify.Get() != Payload->OptionalObject2.Get())
        {
            continue;
        }

        if (EventTag == LMGameplayTags::Event_Combat_HitWindow_Begin)
        {
            const float Position = Instance->GetPosition();

            // 중복 Begin으로 이미 맞힌 대상 목록이 지워지는 것을 막는다.
            if (Window.bOpen || Position < Window.StartTime - UE_KINDA_SMALL_NUMBER || Position > Window.EndTime + UE_KINDA_SMALL_NUMBER)
            {
                return;
            }

            Window.ProcessedReceivers.Reset();
            Window.bOpen = true;
            Window.bPendingLastPose = false;
            ++TraceRevision;
        }
        else if (EventTag == LMGameplayTags::Event_Combat_HitWindow_End && Window.bOpen)
        {
            // End 알림 시점에는 뼈 갱신 전일 수 있으므로 마지막 완료 포즈를 한 번 기다린다.
            Window.bOpen = false;
            Window.bPendingLastPose = true;
            ++TraceRevision;
        }
        else if (EventTag == LMGameplayTags::Event_Combat_HitWindow_Cancel && (Window.bOpen || Window.bPendingLastPose))
        {
            // 취소된 타격은 남은 경로를 보충해서 피해를 주지 않는다.
            Window.bOpen = false;
            Window.bPendingLastPose = false;
            ++TraceRevision;
        }

        return;
    }
}

/** 연속된 포즈의 검 경로를 타격창 시간으로 자르고 일시정지·점프 구간을 제외한다. */
void ULMAbilityTaskWeaponHitTrace::HandlePoseFinalized()
{
    if (bStopped || bProcessingPose)
    {
        return;
    }

    // 피해 콜백에서 포즈 갱신이 재진입해도 같은 샘플을 중첩 처리하지 않는다.
    TGuardValue<bool> ProcessingGuard(bProcessingPose, true);

    FAnimMontageInstance* Instance = GetValidAttackInstance();
    UWorld* World = GetWorld();

    if (!Instance || !World)
    {
        EndTask();
        return;
    }

    const float Position = Instance->GetPosition();
    const double WorldTime = World->GetTimeSeconds();

    if (!FMath::IsFinite(Position) || !FMath::IsFinite(WorldTime))
    {
        StopTraceWithWarning(TEXT("Invalid montage position or world time."));
        return;
    }

    const float PositionDelta = Position - PreviousMontagePosition;
    const double TimeDelta = WorldTime - PreviousWorldTime;

    if (bHasPreviousPose && FMath::IsNearlyZero(PositionDelta) && FMath::IsNearlyZero(TimeDelta))
    {
        return;
    }

    const UCharacterMovementComponent* Movement = AttackerCharacter->GetCharacterMovement();
    const bool bMontagePositionChanged = !bHasPreviousPose || !FMath::IsNearlyZero(PositionDelta);

    // 엔진이 실제로 진행한 시간과 비교하여 섹션 점프를 정상 재생으로 오인하지 않는다.
    // shortcut: 0.1초를 넘는 포즈 공백은 연결하지 않는다. 저프레임 보상이 필요하면 별도 세분화가 필요하다.
    const bool bContinuousPose = bHasPreviousPose && TimeDelta > 0.0 && TimeDelta <= 0.1 && PositionDelta > 0.0f && FMath::IsNearlyEqual(Instance->GetPreviousPosition(), PreviousMontagePosition, 0.002f) && FMath::IsNearlyEqual(Instance->GetDeltaMoved(), PositionDelta, 0.002f) && (!Movement || !Movement->bJustTeleported);

    for (FLMWeaponHitWindowState& Window : HitWindows)
    {
        const ULMAttackData* WindowData = Window.AttackData.Get();

        if (!IsValid(WindowData))
        {
            StopTraceWithWarning(TEXT("A hit window lost its attack data."));
            return;
        }

        const FVector BladeStart = AttackerMesh->GetSocketLocation(WindowData->TraceStartSocket);
        const FVector BladeEnd = AttackerMesh->GetSocketLocation(WindowData->TraceEndSocket);

        if (BladeStart.ContainsNaN() || BladeEnd.ContainsNaN())
        {
            StopTraceWithWarning(TEXT("Invalid blade socket position."));
            return;
        }

        if (Window.bOpen || Window.bPendingLastPose)
        {
            if (bContinuousPose)
            {
                const float From = FMath::Max(PreviousMontagePosition, Window.StartTime);
                const float To = FMath::Min(Position, Window.EndTime);

                if (From < To)
                {
                    // 한 프레임이 타격창 경계를 지나도 허용 시간에 해당하는 부분만 검사한다.
                    const float FromAlpha = FMath::Clamp((From - PreviousMontagePosition) / PositionDelta, 0.0f, 1.0f);
                    const float ToAlpha = FMath::Clamp((To - PreviousMontagePosition) / PositionDelta, 0.0f, 1.0f);

                    if (!TraceWeaponMovement(Window, FMath::Lerp(Window.PreviousBladeStart, BladeStart, FromAlpha), FMath::Lerp(Window.PreviousBladeEnd, BladeEnd, FromAlpha), FMath::Lerp(Window.PreviousBladeStart, BladeStart, ToAlpha), FMath::Lerp(Window.PreviousBladeEnd, BladeEnd, ToAlpha)))
                    {
                        bHasPreviousPose = false;
                        return;
                    }
                }
            }
            else if (bMontagePositionChanged && Instance->IsPlaying() && Position >= Window.StartTime && Position < Window.EndTime)
            {
                // 점프 후 첫 샘플은 현재 검 위치만 검사하고 이전 위치까지 연결하지 않는다.
                if (!TraceWeaponMovement(Window, BladeStart, BladeEnd, BladeStart, BladeEnd))
                {
                    bHasPreviousPose = false;
                    return;
                }
            }

            Window.bPendingLastPose = false;

            if (Position < Window.StartTime || Position >= Window.EndTime)
            {
                Window.bOpen = false;
            }
        }

        // 닫힌 타격창도 준비 포즈를 보관하여 다음 Begin 직후의 경로를 계산할 수 있게 한다.
        Window.PreviousBladeStart = BladeStart;
        Window.PreviousBladeEnd = BladeEnd;
    }

    PreviousMontagePosition = Position;
    PreviousWorldTime = WorldTime;
    bHasPreviousPose = true;
}

/** 검 전체를 덮는 구체 궤적으로 일반 몬스터를 찾고 타격창마다 한 번 피해를 요청한다. */
bool ULMAbilityTaskWeaponHitTrace::TraceWeaponMovement(FLMWeaponHitWindowState& Window, const FVector& PreviousStart, const FVector& PreviousEnd, const FVector& CurrentStart, const FVector& CurrentEnd)
{
    const ULMAttackData* WindowData = Window.AttackData.Get();
    UWorld* World = GetWorld();
    ALMBaseCharacter* Attacker = AttackerCharacter.Get();

    if (!World || !GetValidAttackInstance())
    {
        EndTask();
        return false;
    }

    if (!IsValid(WindowData) || !FMath::IsFinite(WindowData->TraceRadius) || WindowData->TraceRadius <= 0.0f)
    {
        StopTraceWithWarning(TEXT("Invalid trace radius or attack data."));
        return false;
    }

    const uint32 InvocationRevision = TraceRevision;
    const float Radius = WindowData->TraceRadius;
    const float BladeLength = FMath::Max(FVector::Dist(PreviousStart, PreviousEnd), FVector::Dist(CurrentStart, CurrentEnd));
    const float RequiredSegments = BladeLength / Radius;

    // 설정 오류로 검사 횟수가 무제한 증가하거나 정수 변환이 넘치는 것을 막는다.
    if (!FMath::IsFinite(RequiredSegments) || RequiredSegments > 15.0f)
    {
        StopTraceWithWarning(TEXT("Blade needs more than 16 samples; check socket range or radius."));
        return false;
    }

    const int32 Segments = FMath::Max(1, FMath::CeilToInt(RequiredSegments));

    FCollisionQueryParams Query(SCENE_QUERY_STAT(LMWeaponHitTrace), false, Attacker);
    Query.bFindInitialOverlaps = true;

    // 공격자에게 붙은 별도 Actor도 자신의 공격을 막지 않도록 제외한다.
    TArray<AActor*> AttachedActors;
    Attacker->GetAttachedActors(AttachedActors, true, true);
    Query.AddIgnoredActors(AttachedActors);

    for (int32 Index = 0; Index <= Segments; ++Index)
    {
        // 피해 콜백에서 섹션이 바뀌었다면 이전 구간의 나머지 후보를 처리하지 않는다.
        if (TraceRevision != InvocationRevision)
        {
            return false;
        }

        if (!GetValidAttackInstance())
        {
            EndTask();
            return false;
        }

        const float Alpha = static_cast<float>(Index) / Segments;
        const FVector SweepStart = FMath::Lerp(PreviousStart, PreviousEnd, Alpha);
        const FVector SweepEnd = FMath::Lerp(CurrentStart, CurrentEnd, Alpha);

        TArray<FHitResult> Hits;

        // 반환 bool은 Block 여부다. Overlap 대상도 결과 배열에서 확인한다.
        World->SweepMultiByChannel(Hits, SweepStart, SweepEnd, FQuat::Identity, WindowData->TraceChannel, FCollisionShape::MakeSphere(Radius), Query);

        if (WindowData->bDrawDebug)
        {
            DrawDebugLine(World, SweepStart, SweepEnd, FColor::Cyan, false, 0.1f, 0, 1.0f);
            DrawDebugSphere(World, SweepEnd, Radius, 12, FColor::Cyan, false, 0.1f);
        }

        for (const FHitResult& Hit : Hits)
        {
            // 직전 대상의 피격 처리에서 섹션 이동이나 공격 종료가 발생했을 수 있다.
            if (TraceRevision != InvocationRevision)
            {
                return false;
            }

            if (!GetValidAttackInstance())
            {
                EndTask();
                return false;
            }

            // shortcut: 현재 표적은 일반 몬스터다. 보스·아군까지 확장할 때 공통 표적 정책을 추가한다.
            ALMFieldMonsterCharacter* Receiver = Cast<ALMFieldMonsterCharacter>(Hit.GetActor());

            if (!IsValid(Receiver) || Receiver == Attacker || Receiver->IsActorBeingDestroyed() || Window.ProcessedReceivers.Contains(TWeakObjectPtr<AActor>(Receiver)))
            {
                continue;
            }

            // 피격자의 ASC와 실제 몸체가 연결되어 있고 살아 있을 때만 피해를 요청한다.
            UAbilitySystemComponent* ReceiverASC = Receiver->GetAbilitySystemComponent();
            const ULMCombatAttributeSet* ReceiverAttributes = IsValid(ReceiverASC) ? ReceiverASC->GetSet<ULMCombatAttributeSet>() : nullptr;

            if (!ReceiverAttributes || ReceiverASC->GetAvatarActor() != Receiver || !FMath::IsFinite(ReceiverAttributes->GetHealth()) || ReceiverAttributes->GetHealth() <= 0.0f || ReceiverASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead))
            {
                continue;
            }

            // 검 궤적 검사와 별도로 두 몸체 사이의 벽을 확인하여 벽 너머 피해를 막는다.
            FCollisionQueryParams WallQuery = Query;
            WallQuery.AddIgnoredActor(Receiver);

            AttachedActors.Reset();
            Receiver->GetAttachedActors(AttachedActors, true, true);
            WallQuery.AddIgnoredActors(AttachedActors);

            if (World->LineTraceTestByChannel(Attacker->GetActorLocation(), Receiver->GetActorLocation(), WindowData->TraceChannel, WallQuery))
            {
                continue;
            }

            // 피격 콜백이 재진입해도 같은 대상을 다시 처리하지 않도록 먼저 기록한다.
            Window.ProcessedReceivers.Add(TWeakObjectPtr<AActor>(Receiver));

            // HP를 직접 변경하지 않고 피격자의 공통 GAS 피해 처리에 맡긴다.
            Receiver->ReceiveCombatDamage(Attacker, WindowData, Hit);
        }
    }

    // 마지막 피해 콜백에서도 공격 상태가 바뀔 수 있으므로 반환 전에 다시 확인한다.
    return TraceRevision == InvocationRevision && GetValidAttackInstance() != nullptr;
}

/** 섹션 점프 전에 이전 타격창과 검 위치 기록을 버려 허공을 연결하는 판정을 막는다. */
void ULMAbilityTaskWeaponHitTrace::ResetForMontageJump()
{
    // 실행 중인 Trace가 이전 구간의 나머지 대상을 처리하지 못하게 변경 번호부터 갱신한다.
    ++TraceRevision;
    bHasPreviousPose = false;

    for (FLMWeaponHitWindowState& Window : HitWindows)
    {
        Window.bOpen = false;
        Window.bPendingLastPose = false;
        Window.ProcessedReceivers.Reset();
    }
}

/** 잘못된 설정이나 복원할 수 없는 상태를 알리고 이번 판정 Task를 종료한다. */
void ULMAbilityTaskWeaponHitTrace::StopTraceWithWarning(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Warning, TEXT("[LMWeaponHitTrace] %s: %s"), *GetNameSafe(AttackerCharacter.Get()), Reason);
    EndTask();
}

/** 종료 상태를 먼저 확정하고 판정 기록과 직접 등록한 알림을 정리한다. */
void ULMAbilityTaskWeaponHitTrace::OnDestroy(bool bInOwnerFinished)
{
    // 구독 해제 중 다른 콜백이 발생해도 종료된 공격에서 피해가 나가지 않게 한다.
    bStopped = true;
    ResetForMontageJump();

    // 같은 핸들을 중복 해제하지 않도록 보관 중인 핸들을 먼저 비운다.
    const FDelegateHandle PreviousEventHandle = GameplayEventHandle;
    const FDelegateHandle PreviousPoseHandle = PoseFinalizedHandle;
    GameplayEventHandle.Reset();
    PoseFinalizedHandle.Reset();

    if (AbilitySystemComponent.IsValid() && PreviousEventHandle.IsValid())
    {
        AbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(HitWindowEventTags, PreviousEventHandle);
    }

    if (AttackerMesh.IsValid() && PreviousPoseHandle.IsValid())
    {
        AttackerMesh->UnregisterOnBoneTransformsFinalizedDelegate(PreviousPoseHandle);
    }

    // 피해 콜백에서 종료될 수 있으므로 현재 참조 중일 수 있는 HitWindows 배열은 여기서 제거하지 않는다.
    Super::OnDestroy(bInOwnerFinished);
}