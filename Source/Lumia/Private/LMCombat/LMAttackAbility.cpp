#include "LMCombat/LMAttackAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
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
#include "LMInterface/LMBreakable.h"

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

	// 왕복 거리는 Warp 목표가 결정하므로 별도 루트 배율을 중복 적용하지 않는다.
	const float TranslationScale = bUseRoundTripMove ? 1.0f : RootMotionTranslationScale;
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.0f, StartSection, true, TranslationScale, 0.0f, true);

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
	ResetParryState();
	StopWeaponHitTrace();

	// 첫 루트 이동 전에 목적지와 충돌 응답을 준비한다. 실패한 왕복은 원본 이동으로 재생하지 않는다.
	if (!PrepareAttackMovement(Montage, StartSection))
	{
		HandleAttackMontageAborted();
		return false;
	}

	Task->ReadyForActivation();

	// 무기 Task 생성 조건과 독립적으로 이번 GA가 시작한 재생을 기록한다.
	const FGameplayAbilityActorInfo* const ParryInfo = IsActive() && !bIsAbilityEnding ? GetCurrentActorInfo() : nullptr;
	const UAbilitySystemComponent* const ParryASC = ParryInfo ? ParryInfo->AbilitySystemComponent.Get() : nullptr;
	UAnimInstance* const PlayingAnim = ParryInfo ? ParryInfo->GetAnimInstance() : nullptr;
	if (IsValid(ParryASC) && ParryASC->GetAnimatingAbility() == this && GetCurrentMontage() == Montage && IsValid(PlayingAnim))
	{
		const FAnimMontageInstance* const PlayingInstance = PlayingAnim->GetActiveInstanceForMontage(Montage);
		if (PlayingInstance && !PlayingInstance->IsStopped())
		{
			ParryAnim = PlayingAnim;
			ParryMontageId = PlayingInstance->GetInstanceID();
		}
	}

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

// 현재 공격 몽타주의 패링 창을 연 배치를 기록하거나 비운다.
void ULMAttackAbility::SetParryWindow(const FBranchingPointNotifyPayload& Payload, const UAnimNotifyState& Notify, bool bOpen)
{
	if (!IsActive() || bIsAbilityEnding) return;
	if (ParryMontageId == INDEX_NONE || Payload.MontageInstanceID != ParryMontageId)
		return;

	// 공격 시작 때 기록한 애님 인스턴스와 현재 알림의 메시·몽타주를 대조한다.
	const FGameplayAbilityActorInfo* const ParryInfo = GetCurrentActorInfo();
	const UAnimInstance* const PlayingAnim = ParryAnim.Get();
	const UAnimMontage* const PlayingMontage = GetCurrentMontage();
	if (!ParryInfo || !IsValid(PlayingAnim) || PlayingAnim != ParryInfo->GetAnimInstance())
		return;
	if (!IsValid(PlayingMontage) || Payload.SequenceAsset != PlayingMontage || Payload.SkelMeshComponent != PlayingAnim->GetSkelMeshComponent())
		return;

	// 이전 타의 End가 같은 재생에서 새로 열린 창을 닫지 않도록 배치를 대조한다.
	if (!bOpen)
	{
		if (OpenParryWindow.Get() == &Notify)
			OpenParryWindow.Reset();
		return;
	}

	// 활성 목록에서 빠지거나 정지 중인 재생은 새 창을 열지 못한다.
	const FAnimMontageInstance* const PlayingInstance = PlayingAnim->GetActiveInstanceForMontage(PlayingMontage);
	if (!PlayingInstance || PlayingInstance->GetInstanceID() != ParryMontageId || PlayingInstance->IsStopped())
		return;

	// 같은 배치의 Begin이 중복 전달돼도 기존 창을 유지한다.
	if (OpenParryWindow.Get() != &Notify)
		OpenParryWindow = &Notify;
}

// 열린 패링 창과 이 공격에서 기록한 몽타주 재생을 비운다.
void ULMAttackAbility::ResetParryState()
{
	OpenParryWindow.Reset();
	ParryAnim.Reset();
	ParryMontageId = INDEX_NONE;
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

	// 실제 종료가 지연돼도 이 실행의 패링 창은 바로 닫는다.
	ResetParryState();

	if (ScopeLockCount > 0)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	// 몽타주 종료와 외부 콜백보다 먼저 판정을 닫아 취소 후 피해가 남지 않게 한다.
	StopWeaponHitTrace();
	PassReturnTarget.Reset();
	
	// 정상 종료와 취소 모두에서 이 공격이 등록한 접근 타깃과 접촉 정지를 해제한다.
	FinishAttackMovement();
	
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
	PassReturnTarget.Reset();

	// 같은 능력 인스턴스를 다시 사용할 때 이전 실행의 접근 상태를 남기지 않는다.
	FinishAttackMovement();

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
		FinishAttackMovement();

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

	// 섹션 전환에도 이전 이동을 정리하고 새 목적지를 준비한다.
	if (!PrepareAttackMovement(Montage, SectionName))
	{
		HandleAttackMontageAborted();
		return;
	}

	// 이동 준비 콜백을 마친 뒤 섹션이 실제로 바뀌기 직전에 이전 창을 닫는다.
	OpenParryWindow.Reset();
	MontageJumpToSection(SectionName);
}

// 현재 포커스를 한 번 읽어 공격 종류에 맞는 이동과 공통 공격 대상을 준비한다.
bool ULMAttackAbility::PrepareAttackMovement(UAnimMontage* Montage, FName SectionName)
{
	// 같은 GA의 직전 타가 남긴 접근·대상 보관을 먼저 정리한다.
	FinishAttackMovement();

	ACharacter* Character = GetAttackCharacter();
	ULMPlayerMeleeMovementComponent* Movement = Character ? Cast<ULMPlayerMeleeMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	ALMPlayerController* Controller = Character ? Cast<ALMPlayerController>(Character->GetController()) : nullptr;

	const bool bUseApproach = MeleeApproachSections.Contains(SectionName);
	const bool bUsePass = PassMoveSections.Contains(SectionName);
	if (!bUsePass) PassReturnTarget.Reset();
	if (!bUseRoundTripMove && !bUseApproach && !bUsePass) return true;

	if (!Movement || !IsValid(Montage) || (!bUsePass && !Movement->IsMovingOnGround()))
		return !bUseRoundTripMove;

	if (!ensureMsgf(static_cast<int32>(bUseRoundTripMove) + static_cast<int32>(bUseApproach) + static_cast<int32>(bUsePass) == 1, TEXT("%s: Only one attack movement may own a section."), *GetName()))
		return false;

	if (bUsePass)
	{
		if (Controller && Controller->CanUpdateFocus())
		{
			// 다음 타가 시작될 때만 최신 유효 적을 읽고 진행 중인 타의 방향은 유지한다.
			Controller->BeginAttackFocus(this, Controller->GetFocusTarget());
			Controller->OnFocusFrame.Broadcast();
			if (!IsActive() || bIsAbilityEnding) return false;
		}

		// 갱신 콜백 뒤 생존을 확인한다. 화면 밖으로 간 기존 적도 복귀 방향에만 쓰고 포커스로 강제 등록하지 않는다.
		ALMBaseCharacter* PassTarget = PassReturnTarget.Get();
		if (!PassTarget || !PassTarget->CanBeAttackTargetFor(Cast<ALMBaseCharacter>(Character)))
			PassTarget = Controller && Controller->CanUpdateFocus() ? Cast<ALMBaseCharacter>(Controller->GetAttackFocusTarget()) : nullptr;
		PassReturnTarget = PassTarget;
		// 대상이 없어도 원래 전방 모션으로 공격하며 적 몸체 통과는 같은 구간에 적용한다.
		return Movement->BeginPassMove(this, Montage, PassTarget);
	}

	AActor* FocusActor = Controller && Controller->CanUpdateFocus() ? Controller->GetFocusTarget() : nullptr;
	ALMBaseCharacter* Target = Cast<ALMBaseCharacter>(FocusActor);

	bool bHasTarget = false;
	if (bUseRoundTripMove)
	{
		// 고정된 몸 방향·거리를 사용하므로 적 사망이나 시야 이탈이 복귀를 취소하지 않는다.
		bHasTarget = Target && Target->CanBeAttackTargetFor(Cast<ALMBaseCharacter>(Character));
		const FVector Direction = bHasTarget ? (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D() : Character->GetActorForwardVector();
		if (!Movement->BeginRoundTripMove(this, Montage, Direction, RoundTripDistance, RoundTripWindupDistance))
			return false;
	}
	else
	{
		// 전달된 대상만 검사하며, 실패해도 다른 몬스터를 대신 찾지 않는다.
		bHasTarget = Movement->BeginMeleeApproach(this, Montage, Target);
	}

	// 실제로 받아들인 대상만 카메라·표식과 공유한다.
	// 대상이 없어도 이번 타가 끝날 때까지 중간 재선정은 막는다.
	if (Controller)
		Controller->BeginAttackFocus(this, bHasTarget ? Target : nullptr);

	return true;
}

// 특수 대기에 진입하면 몸체 충돌만 복구하고 다음 타에서 쓸 포커스는 남긴다.
void ULMAttackAbility::EndAttackTravel()
{
	ACharacter* Character = GetAttackCharacter();

	if (ULMPlayerMeleeMovementComponent* Movement = Character ? Cast<ULMPlayerMeleeMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		Movement->EndAttackMove(this);
}

// 회복·종료·취소에서는 임시 충돌과 포커스 소유권을 모두 정리한다.
void ULMAttackAbility::FinishAttackMovement()
{
	EndAttackTravel();
	ACharacter* Character = GetAttackCharacter();
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
    ObjectQuery.AddObjectTypesToQuery(ECC_GameTraceChannel4); // LMEnemyBody
	// 살아 있는 플레이어의 착지라면 환경 피격 상자도 후보로 조회한다.
	if (Attacker->IsPlayerControlled() && Attacker->IsAliveForCombat())
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
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
	
	// 이번 착지에서 물체마다 한 번씩, 당시 맞은 부품과 접점을 함께 기억한다.
	TMap<TWeakObjectPtr<AActor>, FHitResult> PendingBreakHits;

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
    	
    	AActor* HitActor = Overlap.GetActor();
    	UPrimitiveComponent* HitBody = Overlap.GetComponent();

    	// 플레이어가 살아 있고 이번 오버랩이 지정된 피격 상자에서 나온 것인지 확인한다.
    	if (Attacker->IsPlayerControlled() && Attacker->IsAliveForCombat() && ILMBreakable::IsHitBody(HitActor, HitBody, WallChannel))
    	{
    		// 직접 디딘 상자라면 착지 이벤트의 실제 접점을 그대로 사용한다.
    		const bool bLandedOnBody = LandingHit->GetActor() == HitActor && LandingHit->GetComponent() == HitBody;
    		FVector Surface = Origin;
    		if (!bLandedOnBody && HitBody->GetClosestPointOnCollision(VisibilityStart, Surface) <= 0.0f)
    			continue;

    		// 첫 오버랩은 큰 박스로 후보만 모은다. 실제 착지 반경과 높이를 둘 다 만족해야 한다.
    		if (FVector::DistSquared2D(Origin, Surface) > FMath::Square(static_cast<double>(Radius)) || FMath::Abs(Surface.Z - Origin.Z) > HalfHeight)
    			continue;

    		// 목표 상자만 이번 가림선에서 제외한다. 다른 문이나 벽이 앞에 있으면 충격이 닿지 않는다.
    		FCollisionQueryParams CoverQuery = Query;
    		CoverQuery.AddIgnoredComponent(HitBody);
    		if (World->LineTraceTestByChannel(VisibilityStart, Surface, WallChannel, CoverQuery))
    			continue;

    		// 여기서 바로 문을 열면 뒤쪽 적의 가림 판정이 달라지므로 접점만 저장한다.
    		PendingBreakHits.Add(TWeakObjectPtr<AActor>(HitActor), FHitResult(HitActor, HitBody, Surface, FVector::ZeroVector));
    		continue;
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
	
	// 적 판정이 끝난 뒤에만 환경 물체를 부숴 같은 충격이 열린 통로 너머로 새지 않게 한다.
	for (const auto& Pair : PendingBreakHits)
	{
		// 이전 대상의 반응으로 공격이 끝났거나 공격자가 사망했으면 남은 이벤트를 보내지 않는다.
		if (!IsActive() || bIsAbilityEnding || LandingDamageRun != CurrentRun || GetAvatarActorFromActorInfo() != Attacker || !IsValid(Attacker) || !Attacker->IsAliveForCombat())
			break;

		AActor* HitActor = Pair.Key.Get();
		const FHitResult& SavedHit = Pair.Value;

		// 다른 반응으로 물체가 사라졌거나 피격 상자가 이미 꺼졌을 수 있다.
		if (!ILMBreakable::IsHitBody(HitActor, SavedHit.GetComponent(), WallChannel))
			continue;

		ILMBreakable::Execute_ReceiveBreakHit(HitActor, Origin, SavedHit.ImpactPoint);
	}
}
