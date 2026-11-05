#include "LMCombat/LMAttackAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"

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

	Task->ReadyForActivation();

	// 재생 시작에 즉시 실패했다면 콜백에서 이미 능력이 종료되었을 수 있다.
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