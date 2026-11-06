#include "LMAbilitySystem/LMAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "LMGameplayTags/LMGameplayTags.h"

/** 입력 태그에 연결된 능력의 최초 누름을 기록한다. */
void ULMAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	QueueAbilityInput(InputTag, ELMAbilityInputPhase::Pressed);
}

/** 정상적인 버튼 해제를 기록한다. */
void ULMAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	QueueAbilityInput(InputTag, ELMAbilityInputPhase::Released);
}

/** 입력 환경 변경 등으로 중단된 입력을 기록한다.*/
void ULMAbilitySystemComponent::AbilityInputTagCanceled(FGameplayTag InputTag)
{
	QueueAbilityInput(InputTag, ELMAbilityInputPhase::Canceled);
}

/** 태그에 연결된 Spec을 찾고 중복 누름을 걸러 기록한다. */
void ULMAbilitySystemComponent::QueueAbilityInput(FGameplayTag InputTag, ELMAbilityInputPhase Phase)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability ||
			!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}
		
		const bool bWasHeld = HeldInputs.Contains(Spec.Handle);
		
		if (Phase == ELMAbilityInputPhase::Pressed)
		{
			if (bWasHeld)
			{
				continue;
			}

			HeldInputs.Add(Spec.Handle);
		}
		else
		{
			HeldInputs.Remove(Spec.Handle);
			
			if (Phase == ELMAbilityInputPhase::Released && !bWasHeld)
			{
				continue;
			}
		}
		
		PendingInputs.Add({Spec.Handle, Phase});
	}
}

/** 취소를 먼저 처리한 뒤 정상 누름·해제를 원래 순서대로 전달한다. */
void ULMAbilitySystemComponent::ProcessAbilityInput(bool bGamePaused)
{
    const FGameplayTag BlockedTag =
        FGameplayTag::RequestGameplayTag(
            FName(TEXT("State.Input.Blocked")), false);

	const bool bInputBlocked = HasMatchingGameplayTag(LMGameplayTags::State_Input_Blocked);

    if (bGamePaused || bInputBlocked || !GetAvatarActor())
    {
        ClearAbilityInput();
        return;
    }

    TArray<FLMAbilityInputEvent> Events = MoveTemp(PendingInputs);
    PendingInputs.Reset();

    const uint32 ProcessingGeneration = InputGeneration;
    TSet<FGameplayAbilitySpecHandle> CanceledHandles;

    for (const FLMAbilityInputEvent& Event : Events)
    {
        if (Event.Phase == ELMAbilityInputPhase::Canceled)
        {
            CanceledHandles.Add(Event.Handle);
        }
    }

    // 같은 프레임에 취소된 입력은 활성화나 정상 해제로 처리하지 않는다.
    for (const FGameplayAbilitySpecHandle Handle : CanceledHandles)
    {
        if (InputGeneration != ProcessingGeneration)
        {
            return;
        }

        if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
        {
            Spec->InputPressed = false;
            CancelAbilityHandle(Handle);
        }
    }

    for (const FLMAbilityInputEvent& Event : Events)
    {
        // 실행 도중 UI 전환이나 조종 해제가 일어나면 남은 입력을 버린다.
        if (InputGeneration != ProcessingGeneration)
        {
            return;
        }
		
    	// 앞선 능력이 입력을 차단했으면 남은 입력도 취소한다.
    	if (HasMatchingGameplayTag(LMGameplayTags::State_Input_Blocked) || !GetAvatarActor())
    	{
    		ClearAbilityInput();
    		return;
    	}
    	
        if (CanceledHandles.Contains(Event.Handle))
        {
            continue;
        }

        FGameplayAbilitySpec* Spec =
            FindAbilitySpecFromHandle(Event.Handle);

        if (!Spec || !Spec->Ability)
        {
            continue;
        }

        if (Event.Phase == ELMAbilityInputPhase::Pressed)
        {
            // WaitInputRelease가 초기 입력 상태를 올바르게 읽도록 먼저 설정한다.
            Spec->InputPressed = true;

            if (Spec->IsActive())
            {
                AbilitySpecInputPressed(*Spec);
            }
            else
            {
                // 최초 활성화 입력을 추가 입력으로 다시 보내지 않는다.
                TryActivateAbility(Event.Handle);
            }
        }
        else if (Event.Phase == ELMAbilityInputPhase::Released)
        {
            Spec->InputPressed = false;

            if (Spec->IsActive())
            {
                AbilitySpecInputReleased(*Spec);
            }
        }
    }
}

/** 남은 능력 입력을 취소하고, 지정한 Avatar가 바뀌면 후속 취소를 중단한다. */
void ULMAbilitySystemComponent::ClearAbilityInput(const AActor* ExpectedAvatar)
{
	if (ExpectedAvatar && GetAvatarActor() != ExpectedAvatar)
	{
		return;
	}

	TSet<FGameplayAbilitySpecHandle> HandlesToCancel = HeldInputs;

	for (const FLMAbilityInputEvent& Event : PendingInputs)
	{
		HandlesToCancel.Add(Event.Handle);
	}

	// 입력이 이미 처리용 목록으로 옮겨졌더라도 누름 상태인 능력은 정리한다.
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.InputPressed)
		{
			HandlesToCancel.Add(Spec.Handle);
		}
	}

	// 취소 콜백이 실행되기 전에 입력 기록부터 비운다.
	++InputGeneration;
	PendingInputs.Reset();
	HeldInputs.Reset();

	for (const FGameplayAbilitySpecHandle Handle : HandlesToCancel)
	{
		// 앞선 취소 콜백에서 새 몸체가 연결됐다면 그 입력과 실행은 보존한다.
		if (ExpectedAvatar && GetAvatarActor() != ExpectedAvatar)
		{
			break;
		}

		if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
		{
			Spec->InputPressed = false;
			CancelAbilityHandle(Handle);
		}
	}
}

/** 현재 활성화의 예측 키를 사용해 누름 Task에 사건을 전달한다. */
void ULMAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);
	
	if (!Spec.IsActive())
	{
		return;
	}
	
	const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
	
	if (ensureMsgf(Instance, TEXT("Input abilities must use InstancedPerActor.")))
	{
		InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputPressed,
			Spec.Handle,
			Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}

/** 현재 활성화의 예측 키를 사용해 해제 Task에 사건을 전달한다. */
void ULMAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);
	
	if (!Spec.IsActive())
	{
		return;
	}
	
	const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
	
	if (ensureMsgf(
		Instance,
		TEXT("Input abilities must use InstancedPerActor.")))
	{
		InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputReleased,
			Spec.Handle,
			Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}

/** 회수된 능력의 핸들이 입력 목록에 남지 않게 제거한다. */
void ULMAbilitySystemComponent::OnRemoveAbility(
	FGameplayAbilitySpec& AbilitySpec)
{
	const FGameplayAbilitySpecHandle RemovedHandle = AbilitySpec.Handle;

	HeldInputs.Remove(RemovedHandle);

	PendingInputs.RemoveAll(
		[RemovedHandle](const FLMAbilityInputEvent& Event)
		{
			return Event.Handle == RemovedHandle;
		});

	Super::OnRemoveAbility(AbilitySpec);
}
