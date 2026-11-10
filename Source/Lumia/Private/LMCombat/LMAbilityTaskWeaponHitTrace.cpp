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
#include "LMCharacter/LMBaseCharacter.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"
#include "LMCombat/LMAnimNotifyStateAttackHitWindow.h"
#include "LMCombat/LMAttackData.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMGameplayTags/LMGameplayTags.h"

/** 이번 공격에서 사용할 몽타주와 데이터를 저장한다. */
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

/** 단일 공격 설정을 확인하고 이번 실행에 필요한 두 종류의 알림을 구독한다. */
void ULMAbilityTaskWeaponHitTrace::Activate()
{
    Super::Activate();

    AttackerCharacter = IsValid(Ability.Get()) ? Cast<ALMBaseCharacter>(Ability->GetAvatarActorFromActorInfo()) : nullptr;
    AttackerMesh = AttackerCharacter.IsValid() ? AttackerCharacter->GetMesh() : nullptr;
    AttackerAnimInstance = AttackerMesh.IsValid() ? AttackerMesh->GetAnimInstance() : nullptr;

    if (!AttackerCharacter.IsValid() || !AttackerAnimInstance.IsValid() || !IsValid(AttackMontage.Get()) || !IsValid(AttackData.Get()))
    {
        StopTraceWithWarning(TEXT("Missing attacker, animation instance, montage or attack data."));
        return;
    }

    // shortcut: 첫 단일 공격용이다. 콤보의 섹션 이동·반복·속도 변경을 연결할 때 실행 정책을 확장한다.
    if (AttackMontage->GetNumSections() != 1 || !AttackMontage->CompositeSections[0].NextSectionName.IsNone() || !AttackMontage->bEnableAutoBlendOut || !FMath::IsNearlyEqual(AttackMontage->RateScale, 1.0f))
    {
        StopTraceWithWarning(TEXT("Use one non-looping section, Auto Blend Out and Rate Scale 1."));
        return;
    }

    // 공유 공격 데이터를 읽기만 하며 잘못된 숫자와 소켓 설정은 실행 전에 거부한다.
    if (!FMath::IsFinite(AttackData->BaseDamage) || AttackData->BaseDamage < 0.0f || !FMath::IsFinite(AttackData->TraceRadius) || AttackData->TraceRadius <= 0.0f || AttackData->TraceChannel >= ECC_MAX || AttackData->TraceChannel == ECC_OverlapAll_Deprecated || AttackData->TraceStartSocket == AttackData->TraceEndSocket || !AttackerMesh->DoesSocketExist(AttackData->TraceStartSocket) || !AttackerMesh->DoesSocketExist(AttackData->TraceEndSocket))
    {
        StopTraceWithWarning(TEXT("Check damage, trace channel, positive radius and both body-mesh sockets."));
        return;
    }

    int32 WindowCount = 0;

    for (const FAnimNotifyEvent& Notify : AttackMontage->Notifies)
    {
        ULMAnimNotifyStateAttackHitWindow* Window = Cast<ULMAnimNotifyStateAttackHitWindow>(Notify.NotifyStateClass.Get());

        if (!IsValid(Window))
        {
            continue;
        }

        ++WindowCount;

        if (Notify.MontageTickType != EMontageNotifyTickType::BranchingPoint)
        {
            StopTraceWithWarning(TEXT("Set LM Attack Hit Window to Branching Point."));
            return;
        }

        HitWindowNotify = Window;
        HitWindowStartTime = Notify.GetTriggerTime();
        HitWindowEndTime = Notify.GetEndTriggerTime();
    }

    const float BlendOutLead = AttackMontage->BlendOutTriggerTime >= 0.0f ? AttackMontage->BlendOutTriggerTime : AttackMontage->GetDefaultBlendOutTime();
    const float BlendOutStart = AttackMontage->GetPlayLength() - BlendOutLead;

    // 준비 포즈와 마지막 완료 포즈를 얻을 수 있도록 타격창 양쪽에 여유를 둔다.
    if (WindowCount != 1 || !FMath::IsFinite(HitWindowStartTime) || !FMath::IsFinite(HitWindowEndTime) || !FMath::IsFinite(BlendOutLead) || !FMath::IsFinite(BlendOutStart) || BlendOutLead < 0.0f || HitWindowStartTime <= 0.0f || HitWindowEndTime <= HitWindowStartTime || HitWindowEndTime + 0.1f >= BlendOutStart)
    {
        StopTraceWithWarning(TEXT("Use one positive hit window ending more than 0.1 seconds before Blend Out."));
        return;
    }

    FAnimMontageInstance* Instance = AttackerAnimInstance->GetActiveInstanceForMontage(AttackMontage.Get());
    MontageInstanceID = Instance ? Instance->GetInstanceID() : INDEX_NONE;

    // 이벤트에 담은 재생 ID와 정확하게 비교할 수 있는 범위만 사용한다.
    if (MontageInstanceID < 0 || MontageInstanceID > 16777216 || !GetValidAttackInstance())
    {
        StopTraceWithWarning(TEXT("Attack execution or combat attributes are not ready."));
        return;
    }

    HitWindowEventTags.AddTag(LMGameplayTags::Event_Combat_HitWindow_Begin);
    HitWindowEventTags.AddTag(LMGameplayTags::Event_Combat_HitWindow_End);
    HitWindowEventTags.AddTag(LMGameplayTags::Event_Combat_HitWindow_Cancel);

    // 이벤트는 판정 상태만 바꾸고 실제 검 위치는 완료 포즈 알림에서 읽는다.
    GameplayEventHandle = AbilitySystemComponent->AddGameplayEventTagContainerDelegate(HitWindowEventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ULMAbilityTaskWeaponHitTrace::HandleHitWindowEvent));
    PoseFinalizedHandle = AttackerMesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ULMAbilityTaskWeaponHitTrace::HandlePoseFinalized));

    if (!ensureMsgf(GameplayEventHandle.IsValid() && PoseFinalizedHandle.IsValid(), TEXT("Failed to subscribe weapon trace callbacks.")))
    {
        EndTask();
    }
}

/** 취소되거나 다른 몸체·몽타주로 교체된 공격에서 피해가 남지 않도록 확인한다. */
FAnimMontageInstance* ULMAbilityTaskWeaponHitTrace::GetValidAttackInstance() const
{
    ALMBaseCharacter* Attacker = AttackerCharacter.Get();
    UAbilitySystemComponent* AttackerASC = AbilitySystemComponent.Get();

    if (bStopped || !IsValid(Ability.Get()) || !Ability->IsActive() || !IsValid(Attacker) || Attacker->IsActorBeingDestroyed() || !Attacker->HasAuthority() || !IsValid(AttackerASC) || !AttackerMesh.IsValid() || !AttackerAnimInstance.IsValid() || !IsValid(AttackMontage.Get()) || !IsValid(AttackData.Get()))
    {
        return nullptr;
    }

    // PlayerState가 ASC를 소유해도 Avatar와 현재 공격 몸체는 일치해야 한다.
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

    if (!Instance || Instance->GetInstanceID() != MontageInstanceID || !Instance->IsPlaying() || Instance->IsStopped() || !FMath::IsNearlyEqual(Instance->GetPlayRate(), 1.0f) || !FMath::IsNearlyEqual(AttackMontage->RateScale, 1.0f) || Instance->GetNextSectionID(0) != INDEX_NONE)
    {
        return nullptr;
    }

    return Instance;
}

/** 몽타주 애셋뿐 아니라 재생 ID와 Notify 객체까지 일치하는 이벤트만 처리한다. */
void ULMAbilityTaskWeaponHitTrace::HandleHitWindowEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
    if (!Payload || !GetValidAttackInstance())
    {
        return;
    }

    if (Payload->Instigator.Get() != AttackerCharacter.Get() || Payload->OptionalObject.Get() != AttackMontage.Get() || Payload->OptionalObject2.Get() != HitWindowNotify.Get() || !FMath::IsFinite(Payload->EventMagnitude) || Payload->EventMagnitude != static_cast<float>(MontageInstanceID))
    {
        return;
    }

    if (EventTag == LMGameplayTags::Event_Combat_HitWindow_Begin)
    {
        // 같은 시작 알림이 중복되어도 이미 처리한 대상 기록을 초기화하지 않는다.
        if (!bHitWindowStarted)
        {
            bHitWindowStarted = true;
            bHitWindowOpen = true;
        }
    }
    else if (EventTag == LMGameplayTags::Event_Combat_HitWindow_End)
    {
        if (bHitWindowStarted)
        {
            // 이 시점의 소켓을 바로 읽지 않고 이번 프레임의 완료 포즈를 기다린다.
            bHitWindowOpen = false;
            bFinishAfterPose = true;
        }
    }
    else if (EventTag == LMGameplayTags::Event_Combat_HitWindow_Cancel)
    {
        // 취소에는 마지막 타격을 보충하지 않는다.
        EndTask();
    }
}

/** 이전 포즈와 현재 포즈 사이의 경로를 타격창 시간에 맞춰 잘라 검사한다. */
void ULMAbilityTaskWeaponHitTrace::HandlePoseFinalized()
{
    FAnimMontageInstance* Instance = GetValidAttackInstance();

    if (!Instance)
    {
        EndTask();
        return;
    }

    const float Position = Instance->GetPosition();
    const double WorldTime = GetWorld()->GetTimeSeconds();
    const FVector BladeStart = AttackerMesh->GetSocketLocation(AttackData->TraceStartSocket);
    const FVector BladeEnd = AttackerMesh->GetSocketLocation(AttackData->TraceEndSocket);
    const FVector AttackerLocation = AttackerCharacter->GetActorLocation();

    if (!FMath::IsFinite(Position) || BladeStart.ContainsNaN() || BladeEnd.ContainsNaN() || AttackerLocation.ContainsNaN())
    {
        StopTraceWithWarning(TEXT("Invalid completed pose."));
        return;
    }

    if (!bHasPreviousPose)
    {
        // 시작 전 포즈가 없으면 알 수 없는 검 경로를 임의로 만들어 피해를 주지 않는다.
        if (Position >= HitWindowStartTime)
        {
            StopTraceWithWarning(TEXT("Place the hit window after a completed preparation pose."));
            return;
        }

        bHasPreviousPose = true;
    }
    else
    {
        const double TimeDelta = WorldTime - PreviousWorldTime;
        const float PositionDelta = Position - PreviousMontagePosition;

        if (FMath::IsNearlyZero(TimeDelta) && FMath::IsNearlyZero(PositionDelta))
        {
            return;
        }

        // shortcut: 큰 프레임 지연·순간이동·시간 배율 변경은 중단한다. 해당 공격을 지원할 때 샘플 정책을 확장한다.
        if (TimeDelta <= 0.0 || TimeDelta > 0.1 || PositionDelta <= 0.0f || FMath::Abs(PositionDelta - TimeDelta) > 0.005 || FVector::Dist(AttackerLocation, PreviousAttackerLocation) > 100.0f || FVector::Dist(BladeStart, PreviousBladeStart) > 250.0f || FVector::Dist(BladeEnd, PreviousBladeEnd) > 250.0f)
        {
            StopTraceWithWarning(TEXT("Discontinuous pose or time sample; remaining trace stopped."));
            return;
        }

        const float From = FMath::Max(PreviousMontagePosition, HitWindowStartTime);
        const float To = FMath::Min(Position, HitWindowEndTime);

        if ((bHitWindowOpen || bFinishAfterPose) && From <= To && Position >= HitWindowStartTime && PreviousMontagePosition < HitWindowEndTime)
        {
            // 한 프레임이 타격 시작·끝을 넘어가도 허용 시간에 해당하는 이동만 근사한다.
            const float FromAlpha = FMath::Clamp((From - PreviousMontagePosition) / PositionDelta, 0.0f, 1.0f);
            const float ToAlpha = FMath::Clamp((To - PreviousMontagePosition) / PositionDelta, 0.0f, 1.0f);

            if (!TraceWeaponMovement(FMath::Lerp(PreviousBladeStart, BladeStart, FromAlpha), FMath::Lerp(PreviousBladeEnd, BladeEnd, FromAlpha), FMath::Lerp(PreviousBladeStart, BladeStart, ToAlpha), FMath::Lerp(PreviousBladeEnd, BladeEnd, ToAlpha)))
            {
                return;
            }
        }

        if (Position >= HitWindowEndTime)
        {
            if (!bHitWindowStarted)
            {
                StopTraceWithWarning(TEXT("Hit-window Begin event was not received."));
                return;
            }

            EndTask();
            return;
        }
    }

    // 다음 완료 포즈와 비교할 기준을 이번 공격의 Task에 보관한다.
    PreviousMontagePosition = Position;
    PreviousWorldTime = WorldTime;
    PreviousBladeStart = BladeStart;
    PreviousBladeEnd = BladeEnd;
    PreviousAttackerLocation = AttackerLocation;
}

/** 검 길이를 여러 구체로 덮고 각 구체의 이동 경로에서 일반 몬스터를 찾는다. */
bool ULMAbilityTaskWeaponHitTrace::TraceWeaponMovement(const FVector& PreviousStart, const FVector& PreviousEnd, const FVector& CurrentStart, const FVector& CurrentEnd)
{
    const float Radius = AttackData->TraceRadius;
    const float BladeLength = FMath::Max(FVector::Dist(PreviousStart, PreviousEnd), FVector::Dist(CurrentStart, CurrentEnd));
    const float RequiredSegments = BladeLength / Radius;

    // 잘못된 소켓 거리나 반경 때문에 무제한으로 충돌 검사를 실행하지 않게 한다.
    if (!FMath::IsFinite(RequiredSegments) || RequiredSegments > 15.0f)
    {
        StopTraceWithWarning(TEXT("Blade needs more than 16 samples; check socket range or radius."));
        return false;
    }

    const int32 Segments = FMath::Max(1, FMath::CeilToInt(RequiredSegments));
    UWorld* World = GetWorld();
    ALMBaseCharacter* Attacker = AttackerCharacter.Get();

    FCollisionQueryParams Query(SCENE_QUERY_STAT(LMWeaponHitTrace), false, Attacker);
    Query.bFindInitialOverlaps = true;

    // 공격자에게 붙은 별도 Actor도 자기 공격의 장애물이 되지 않게 한다.
    TArray<AActor*> AttachedActors;
    Attacker->GetAttachedActors(AttachedActors, true, true);
    Query.AddIgnoredActors(AttachedActors);

    for (int32 Index = 0; Index <= Segments; ++Index)
    {
        if (!GetValidAttackInstance())
        {
            EndTask();
            return false;
        }

        const float Alpha = static_cast<float>(Index) / Segments;
        const FVector SweepStart = FMath::Lerp(PreviousStart, PreviousEnd, Alpha);
        const FVector SweepEnd = FMath::Lerp(CurrentStart, CurrentEnd, Alpha);

        TArray<FHitResult> Hits;

        // 반환 bool은 Block 여부이므로 Overlap 후보는 결과 배열에서 직접 확인한다.
        World->SweepMultiByChannel(Hits, SweepStart, SweepEnd, FQuat::Identity, AttackData->TraceChannel, FCollisionShape::MakeSphere(Radius), Query);

        if (AttackData->bDrawDebug)
        {
            DrawDebugLine(World, SweepStart, SweepEnd, FColor::Cyan, false, 0.1f, 0, 1.0f);
            DrawDebugSphere(World, SweepEnd, Radius, 12, FColor::Cyan, false, 0.1f);
        }

        for (const FHitResult& Hit : Hits)
        {
            // 앞 대상의 피격 콜백이 공격을 취소했을 수 있으므로 후보마다 다시 확인한다.
            if (!GetValidAttackInstance())
            {
                EndTask();
                return false;
            }

            // shortcut: 첫 검증의 표적은 일반 몬스터다. 보스·아군이 생기면 공통 표적 정책으로 확장한다.
            ALMFieldMonsterCharacter* Receiver = Cast<ALMFieldMonsterCharacter>(Hit.GetActor());

            if (!IsValid(Receiver) || Receiver == Attacker || Receiver->IsActorBeingDestroyed() || Hit.GetComponent() != Receiver->GetCapsuleComponent() || ProcessedReceivers.Contains(TWeakObjectPtr<AActor>(Receiver)))
            {
                continue;
            }

            UAbilitySystemComponent* ReceiverASC = Receiver->GetAbilitySystemComponent();
            const ULMCombatAttributeSet* ReceiverAttributes = IsValid(ReceiverASC) ? ReceiverASC->GetSet<ULMCombatAttributeSet>() : nullptr;

            if (!ReceiverAttributes || ReceiverASC->GetAvatarActor() != Receiver || !FMath::IsFinite(ReceiverAttributes->GetHealth()) || ReceiverAttributes->GetHealth() <= 0.0f || ReceiverASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead))
            {
                continue;
            }

            // 두 몸체 사이에 벽이 있으면 피해 요청을 막는다.
            FCollisionQueryParams WallQuery = Query;
            WallQuery.AddIgnoredActor(Receiver);

            AttachedActors.Reset();
            Receiver->GetAttachedActors(AttachedActors, true, true);
            WallQuery.AddIgnoredActors(AttachedActors);

            if (World->LineTraceTestByChannel(Attacker->GetActorLocation(), Receiver->GetActorLocation(), AttackData->TraceChannel, WallQuery))
            {
                continue;
            }

            // 피해 콜백이 재진입해도 중복 처리하지 않도록 호출보다 먼저 기록한다.
            ProcessedReceivers.Add(TWeakObjectPtr<AActor>(Receiver));

            // HP는 직접 수정하지 않고 피격자의 기존 공통 피해 경로에 맡긴다.
            Receiver->ReceiveCombatDamage(Attacker, AttackData.Get(), Hit);
        }
    }

    return !bStopped;
}

/** 복원하기 어려운 설정·샘플 오류를 알리고 이번 판정을 종료한다. */
void ULMAbilityTaskWeaponHitTrace::StopTraceWithWarning(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Warning, TEXT("[LMWeaponHitTrace] %s: %s"), *GetNameSafe(AttackerCharacter.Get()), Reason);
    EndTask();
}

/** 종료 상태를 먼저 확정하여 남아 있는 콜백이 피해를 적용하지 못하게 한다. */
void ULMAbilityTaskWeaponHitTrace::OnDestroy(bool bInOwnerFinished)
{
    bStopped = true;
    bHitWindowOpen = false;
    bFinishAfterPose = false;

    // 해제 도중 다른 알림이 발생하더라도 같은 핸들을 다시 해제하지 않게 한다.
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

    ProcessedReceivers.Reset();

    Super::OnDestroy(bInOwnerFinished);
}
