#include "LMCombat/LMAttackAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMCombat/LMAbilityTaskWeaponHitTrace.h"
#include "LMCombat/LMAttackData.h"
#include "LMCharacter/LMPlayerMeleeMovementComponent.h"
#include "LMPlayer/LMPlayerController.h"

/** 공격 능력의 공통 실행 정책과 상태 태그를 설정한다. */
ULMAttackAbility::ULMAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	bRetriggerInstancedAbility = false;
	bReplicateInputDirectly = false;

	FGameplayTagContainer AttackTags;
	AttackTags.AddTag(LMGameplayTags::Ability_Attack);
	SetAssetTags(AttackTags);

	ActivationOwnedTags.AddTag(LMGameplayTags::State_Action_Attacking);
	ActivationOwnedTags.AddTag(LMGameplayTags::State_Movement_External);

	ActivationBlockedTags.AddTag(LMGameplayTags::State_Action_Attacking);
	ActivationBlockedTags.AddTag(LMGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(LMGameplayTags::State_Stunned);
}

/** 기본 GAS 조건에 더해 지상 / 공중 실행 조건을 검사한다. */
bool ULMAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	
	if (!Movement)
	{
		return false;
	}

	if (MovementRequirement == ELMAttackMovementRequirement::Grounded && !Movement->IsMovingOnGround())
	{
		return false;
	}
	
	if (MovementRequirement == ELMAttackMovementRequirement::Airborne && !Movement->IsFalling())
	{
		return false;
	}
	
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

/** 능력을 실제로 수행하는 Avatar 캐릭터를 반환한다. */
ACharacter* ULMAttackAbility::GetAttackCharacter() const
{
	return Cast<ACharacter>(GetAvatarActorFromActorInfo());
}

/** 몽타주를 시작하고 완료 / 중단 / 실패를 이 능력의 종료와 연결한다. */
bool ULMAttackAbility::PlayAttackMontage(UAnimMontage* Montage, FName StartSection, float RootMotionTranslationScale)
{
	if (!IsActive())
	{
		return false;
	}
	
	const bool bValidMontage = IsValid(Montage) && (StartSection.IsNone() || Montage->GetSectionIndex(StartSection) != INDEX_NONE);
	
	if (!ensureMsgf(bValidMontage, TEXT("Invalid attack montage or section: %s / %s"), *GetNameSafe(Montage), *StartSection.ToString()))
	{
		HandleAttackMontageAborted();
		return false;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.0f, StartSection, true, RootMotionTranslationScale, 0.0f, true);

	if (!ensure(Task))
	{
		HandleAttackMontageAborted();
		return false;
	}

	// 지상 공격에 직전 걷기 속도가 섞이지 않도록 먼저 정리한다.
	if (ACharacter* Character = GetAttackCharacter())
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (Movement->IsMovingOnGround())
			{
				Movement->StopMovementImmediately();
			}
		}
	}

	Task->OnCompleted.AddDynamic(this, &ThisClass::HandleAttackMontageCompleted);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::HandleAttackMontageAborted);
	Task->OnCancelled.AddDynamic(this, &ThisClass::HandleAttackMontageAborted);

	// 같은 능력이 다른 몽타주를 시작하면 이전 판정을 먼저 닫는다.
	StopWeaponHitTrace();

	// 몽타주가 시작되기 전에 이번 평타의 접근 위치와 방향을 등록한다.
	PrepareMeleeApproach(Montage, StartSection);

	Task->ReadyForActivation();

	// 몽타주 재생 ID가 생긴 뒤 이번 실행을 관찰하는 Task를 만든다.
	if (IsActive() && IsValid(AttackData.Get()))
	{
		WeaponHitTraceTask = ULMAbilityTaskWeaponHitTrace::CreateWeaponHitTraceTask(this, Montage, AttackData.Get());

		if (!ensure(IsValid(WeaponHitTraceTask.Get())))
		{
			HandleAttackMontageAborted();
			return false;
		}

		WeaponHitTraceTask->ReadyForActivation();
	}

	// 몽타주 시작 실패 시 기존 콜백에서 능력이 종료되었을 수 있다.
	return IsActive();
}

/** 몽타주가 끝까지 재생되면 공격을 정상 종료한다. */
void ULMAttackAbility::HandleAttackMontageCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

/** 몽타주 실행 실패나 중단을 취소된 공격 종료로 처리한다. */
void ULMAttackAbility::HandleAttackMontageAborted()
{
	if (IsActive())
	{
		// 취소 알림 안에서 CancelAbility를 다시 호출하여 알림이 재진입하지 않게 한다.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

/** 정상 종료와 취소 모두에서 회복 상태와 루트 모션 배율을 정리한다. */
void ULMAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	if (ScopeLockCount > 0)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	// 몽타주 종료와 외부 콜백보다 먼저 판정을 닫아 취소 후 피해가 남지 않게 한다.
	StopWeaponHitTrace();
	
	// 정상 종료와 취소 모두에서 이 공격이 등록한 접근 타깃과 접촉 정지를 해제한다.
	FinishMeleeApproach();
	
	SetAttackRecovery(false);

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// 다른 능력이 이미 몽타주를 이어받았다면 그 능력의 배율은 변경하지 않는다.
	if (ASC && ASC->GetAnimatingAbility() == this)
	{
		if (ACharacter* Character = GetAttackCharacter())
		{
			Character->SetAnimRootMotionTranslationScale(1.0f);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/** 회복 관련 사건을 먼저 구독하고 기존 BP의 ActivateAbility 실행을 이어 준다. */
void ULMAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 같은 인스턴스를 재사용하므로 새 실행 번호와 착지 피해 기록을 먼저 준비한다.
	++LandingDamageRun;
	bLandingDamageApplied = false;

	// 같은 능력 인스턴스를 다시 사용할 때 이전 실행의 접근 상태를 남기지 않는다.
	FinishMeleeApproach();

	SetAttackRecovery(false);

    UAbilityTask_WaitGameplayEvent* RecoveryTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, LMGameplayTags::Event_Combat_Recovery_Begin, nullptr, false, true);
    UAbilityTask_WaitGameplayEvent* CancelTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, LMGameplayTags::Event_Action_CancelRecovery, nullptr, false, true);

    if (!ensure(RecoveryTask && CancelTask))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    RecoveryTask->EventReceived.AddDynamic(this, &ThisClass::HandleRecoveryBegin);
    CancelTask->EventReceived.AddDynamic(this, &ThisClass::HandleRecoveryCancel);

    RecoveryTask->ReadyForActivation();
    CancelTask->ReadyForActivation();

    // BP에서 몽타주를 시작하기 전에 Notify를 받을 준비를 완료한다.
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

/** 이 공격이 소유한 회복 상태를 변경하고 공통 상태 태그에 반영한다. */
void ULMAttackAbility::SetAttackRecovery(bool bNewRecovery)
{
    if (bInAttackRecovery == bNewRecovery)
    {
        return;
    }

    if (bNewRecovery && (!IsActive() || bIsAbilityEnding))
    {
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!ASC)
    {
        bInAttackRecovery = false;
        return;
    }

    // 태그 변경 알림이 발생하기 전에 이 능력의 내부 상태를 먼저 맞춘다.
    bInAttackRecovery = bNewRecovery;

	if (bNewRecovery)
	{
		// 태그 알림으로 다음 공격이 시작되기 전에 현재 공격의 접근 상태를 정리한다.
		FinishMeleeApproach();

		ASC->AddLooseGameplayTag(LMGameplayTags::State_Action_Recovery);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(LMGameplayTags::State_Action_Recovery);
	}
}

/** 현재 공격의 마무리 동작을 취소할 수 있는지 반환한다. */
bool ULMAttackAbility::IsInAttackRecovery() const
{
    return bInAttackRecovery;
}

/** 현재 이 능력이 재생하는 몽타주의 회복 시작 알림만 받아들인다. */
void ULMAttackAbility::HandleRecoveryBegin(FGameplayEventData Payload)
{
    if (!IsActive() || bIsAbilityEnding)
    {
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    UAnimMontage* Montage = GetCurrentMontage();

    if (ASC && ASC->GetAnimatingAbility() == this && Montage && Payload.OptionalObject.Get() == Montage)
    {
        SetAttackRecovery(true);
    }
}

/** 공격 본동작은 유지하고 회복 구간에서만 표준 GAS 취소 경로를 실행한다. */
void ULMAttackAbility::HandleRecoveryCancel(FGameplayEventData Payload)
{
    if (IsActive() && !bIsAbilityEnding && bInAttackRecovery)
    {
        K2_CancelAbility();
    }
}

/** 종료 콜백이 재진입해도 이전 Task를 다시 정리하지 않도록 참조부터 비운다. */
void ULMAttackAbility::StopWeaponHitTrace()
{
	ULMAbilityTaskWeaponHitTrace* TaskToStop = WeaponHitTraceTask.Get();
	WeaponHitTraceTask = nullptr;

	if (IsValid(TaskToStop))
	{
		TaskToStop->EndTask();
	}
}

/** 섹션을 건너뛸 때 실제로 재생하지 않은 공간에 검 궤적이 생기지 않도록 처리한다. */
void ULMAttackAbility::JumpAttackMontageSection(FName SectionName)
{
	UAbilitySystemComponent* AttackerASC = GetAbilitySystemComponentFromActorInfo();
	UAnimMontage* Montage = GetCurrentMontage();

	// 다른 능력이 재생 중인 몽타주나 존재하지 않는 섹션에는 영향을 주지 않는다.
	if (!IsActive() || !IsValid(AttackerASC) || AttackerASC->GetAnimatingAbility() != this || !IsValid(Montage) || Montage->GetSectionIndex(SectionName) == INDEX_NONE)
	{
		return;
	}

	// 점프 후 발생할 Notify가 새 타격 기록을 시작할 수 있도록 이전 상태를 먼저 비운다.
	if (IsValid(WeaponHitTraceTask.Get()))
	{
		WeaponHitTraceTask->ResetForMontageJump();
	}

	// 다음 평타는 새 목적지를 준비하고, 특수공격 섹션이면 기존 접근을 정리한다.
	PrepareMeleeApproach(Montage, SectionName);

	MontageJumpToSection(SectionName);
}

// 평타 한 타가 시작될 때 현재 표식 대상을 읽고, 이동 보정과 공통 공격 대상에 전달한다.
void ULMAttackAbility::PrepareMeleeApproach(UAnimMontage* Montage, FName SectionName)
{
	// 같은 GA의 직전 타가 남긴 접근·대상 보관을 먼저 정리한다.
	FinishMeleeApproach();

	ACharacter* Character = GetAttackCharacter();
	ULMPlayerMeleeMovementComponent* Movement = Character ? Cast<ULMPlayerMeleeMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	ALMPlayerController* Controller = Character ? Cast<ALMPlayerController>(Character->GetController()) : nullptr;

	// 현재 등록된 지상 평타 섹션에만 적용한다.
	if (!Movement || !IsValid(Montage) || !MeleeApproachSections.Contains(SectionName) || !Movement->IsMovingOnGround())
	{
		return;
	}

	AActor* FocusActor = Controller && Controller->CanUpdateFocus() ? Controller->GetFocusTarget() : nullptr;
	ALMBaseCharacter* Target = Cast<ALMBaseCharacter>(FocusActor);

	// 이동 컴포넌트는 전달된 대상만 검사하며, 실패해도 다른 몬스터를 대신 찾지 않는다.
	const bool bHasTarget = Movement->BeginMeleeApproach(this, Montage, Target);

	// 실제로 받아들인 대상만 카메라·표식과 공유한다.
	// 대상이 없어도 이번 타가 끝날 때까지 중간 재선정은 막는다.
	if (Controller)
		Controller->BeginAttackFocus(this, bHasTarget ? Target : nullptr);
}

// 회복·종료·취소에서 자신이 시작한 접근과 공통 공격 대상 보관을 함께 해제한다.
void ULMAttackAbility::FinishMeleeApproach()
{
	ACharacter* Character = GetAttackCharacter();

	if (ULMPlayerMeleeMovementComponent* Movement = Character ? Cast<ULMPlayerMeleeMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		Movement->EndMeleeApproach(this);

	if (ALMPlayerController* Controller = Character ? Cast<ALMPlayerController>(Character->GetController()) : nullptr)
		Controller->EndAttackFocus(this);
}

/** 실제 착지점 주변의 몸체를 검사하고 가려지지 않은 적에게 한 번씩 피해를 요청한다. */
void ULMAttackAbility::ApplyLandingAreaDamage(const FGameplayEventData& LandingEvent)
{
    // 종료되었거나 이미 처리한 공격에서 피해가 다시 발생하지 않게 한다.
    if (!IsActive() || bIsAbilityEnding || bLandingDamageApplied)
    {
        return;
    }

    ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(GetAvatarActorFromActorInfo());

    // 기존 GAS 피해 경로와 같이 권한이 있는 현재 몸체에서만 피해를 요청한다.
    if (!IsValid(Attacker) || Attacker->IsActorBeingDestroyed() || !Attacker->HasAuthority())
    {
        return;
    }

    if (LandingEvent.EventTag != LMGameplayTags::Event_Movement_Landed || LandingEvent.Instigator.Get() != Attacker || LandingEvent.Target.Get() != Attacker)
    {
        return;
    }

    UWorld* World = Attacker->GetWorld();
    const ULMAttackData* LandingData = LandingAttackData.Get();
    const FHitResult* LandingHit = LandingEvent.ContextHandle.GetHitResult();

    // 착지 정보가 없을 때 캡슐 중심으로 임의 대체하면 다른 높이에 피해가 생긴다.
    if (!World || !LandingHit || !LandingHit->IsValidBlockingHit() || LandingHit->ImpactPoint.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMLandingAreaDamage] Missing valid landing hit."));
        return;
    }

    if (!IsValid(LandingData) || !FMath::IsFinite(LandingData->BaseDamage) || LandingData->BaseDamage < 0.0f || !FMath::IsFinite(LandingDamageRadius) || LandingDamageRadius <= 0.0f || !FMath::IsFinite(LandingDamageHalfHeight) || LandingDamageHalfHeight <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMLandingAreaDamage] Check landing attack data, radius and height."));
        return;
    }

    const ECollisionChannel WallChannel = LandingData->TraceChannel;
    if (WallChannel >= ECC_MAX || WallChannel == ECC_OverlapAll_Deprecated)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMLandingAreaDamage] Invalid wall trace channel."));
        return;
    }

    // 피해 콜백의 재진입보다 먼저 기록하고 현재 실행을 식별한다.
    bLandingDamageApplied = true;
    const uint32 CurrentRun = LandingDamageRun;
    const FVector Origin = LandingHit->ImpactPoint;
    const float Radius = LandingDamageRadius;
    const float HalfHeight = LandingDamageHalfHeight;

    FCollisionObjectQueryParams ObjectQuery;
    ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LMLandingAreaDamage), false, Attacker);

    // 자신에게 부착된 무기가 자신의 충격파를 가리지 않게 한다.
    TArray<AActor*> AttachedActors;
    Attacker->GetAttachedActors(AttachedActors, true, true);
    Query.AddIgnoredActors(AttachedActors);

    // 원기둥을 감싸는 박스로 후보를 모으고 아래에서 모서리의 후보를 제외한다.
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectQuery, FCollisionShape::MakeBox(FVector(Radius, Radius, HalfHeight)), Query);

    if (LandingData->bDrawDebug)
        DrawDebugCylinder(World, Origin - FVector::UpVector * HalfHeight, Origin + FVector::UpVector * HalfHeight, Radius, 48, FColor::Cyan, false, 1.0f, 0, 2.0f);

    TSet<TWeakObjectPtr<AActor>> ProcessedReceivers;

    // 바닥 자체에 선이 막히지 않도록 접점보다 조금 높은 위치에서 검사한다.
    const float SightHeight = FMath::Min(20.0f, HalfHeight * 0.5f);
    const FVector VisibilityStart = Origin + FVector(0.0f, 0.0f, SightHeight);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        // 피해 콜백이 공격을 끝내거나 새 실행을 시작했으면 이전 검사를 중단한다.
        if (!IsActive() || bIsAbilityEnding || LandingDamageRun != CurrentRun || GetAvatarActorFromActorInfo() != Attacker || Attacker->IsActorBeingDestroyed())
        {
            break;
        }

        ALMBaseCharacter* Receiver = Cast<ALMBaseCharacter>(Overlap.GetActor());
        if (!IsValid(Receiver) || Receiver == Attacker || ProcessedReceivers.Contains(TWeakObjectPtr<AActor>(Receiver)))
        {
            continue;
        }

        UCapsuleComponent* Capsule = Receiver->GetCapsuleComponent();

        // 공통 몸체와 적대·생존·무적 규칙을 사용하여 일반 몬스터와 보스를 함께 처리한다.
        if (!IsValid(Capsule) || Overlap.GetComponent() != Capsule || Capsule->GetCollisionResponseToChannel(WallChannel) == ECR_Ignore || !Receiver->CanReceiveAttackFrom(Attacker))
        {
            continue;
        }

        const FVector Center = Capsule->GetComponentLocation();
        const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

        // shortcut: 직립 캡슐 몸체 기준이다. 다중 몸체·기울어진 보스를 만들 때 형상 조회를 확장한다.
        // 적의 중심이 범위 밖이어도 몸체가 닿으면 포함하도록 표면까지의 거리를 계산한다.
        const float AxisHalfHeight = FMath::Max(0.0f, CapsuleHalfHeight - CapsuleRadius);
        const double HorizontalGap = FMath::Max(0.0, FVector::Dist2D(Origin, Center) - Radius);
        const double VerticalGap = FMath::Max(0.0, FMath::Abs(Center.Z - Origin.Z) - AxisHalfHeight - HalfHeight);

        // 수평·수직 거리를 함께 검사하여 둥근 캡슐 끝의 대각선 빈 공간을 제외한다.
        const bool bTouchesArea = FMath::Square(HorizontalGap) + FMath::Square(VerticalGap) <= FMath::Square(static_cast<double>(CapsuleRadius));

        if (!bTouchesArea)
            continue;

        FCollisionQueryParams WallQuery = Query;
        WallQuery.AddIgnoredActor(Receiver);
        AttachedActors.Reset();
        Receiver->GetAttachedActors(AttachedActors, true, true);
        WallQuery.AddIgnoredActors(AttachedActors);

        const FVector VisibilityEnd(Center.X, Center.Y, Center.Z - CapsuleHalfHeight + SightHeight);
        if (World->LineTraceTestByChannel(VisibilityStart, VisibilityEnd, WallChannel, WallQuery))
        {
            continue;
        }

        // 피격 이벤트보다 먼저 기록하여 같은 적에게 중복 요청하지 않는다.
        ProcessedReceivers.Add(TWeakObjectPtr<AActor>(Receiver));

        if (LandingData->bDrawDebug)
            DrawDebugLine(World, VisibilityStart, VisibilityEnd, FColor::Yellow, false, 1.0f, 0, 2.0f);

        // 바닥 충돌을 적의 피격점으로 전달하지 않고 기존 GAS 피해 접수만 재사용한다.
        Receiver->ReceiveCombatDamage(Attacker, LandingData, FHitResult());
    }
}
