// Fill out your copyright notice in the Description page of Project Settings.


#include "LMComponent/LMInteractionComponent.h"

#include "Blueprint/UserWidget.h"

ULMInteractionComponent::ULMInteractionComponent()
{

	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled=false;
	InteractionId=FGuid::NewGuid();
	bIsBusy=false;
}

//자신이 요청자와 상호작용이 가능한지
FLMInteractionResult ULMInteractionComponent::EvaluateInteraction(AActor* RequestActor,FGameplayTagContainer* ActionTags)
{
	FLMInteractionResult R;
	//자신이 이미 상호작용중
	if (bIsBusy)
	{
		R.Failure=InteractionFailure::IsBusy;
		return R;
	}
	
	R.InteractionId=InteractionId;
	R.bCanInteraction=HasTagContainer.HasAll(*ActionTags);
	//유효 태그 검사
	if (!R.bCanInteraction)
	{
		R.Failure=InteractionFailure::NotHasAllRequireTags;
		return R;
	}
	
	return R;
}
//대상에게 인터렉션 요청->대상이 요청자와 인터렉션가능한지 확인->가능한경우 인터페이스로 구현된 interact 실행
FLMInteractionResult ULMInteractionComponent::RequestInteraction(AActor* RequestActor,FGameplayTagContainer& ActionTags)
{
	FLMInteractionResult R;
	AActor* TargetActor = GetOwner();
	
	if (!IsValid(TargetActor) || !IsValid(RequestActor))
	{
		R.Failure = InteractionFailure::CantAccessActor;
		return R;
	}

	if (!ActionTags.IsValid())
	{
		R.Failure = InteractionFailure::InvalidActionTags;
		return R;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(
			ULMInteractable::StaticClass()))
	{
		R.Failure = InteractionFailure::NotHasInterface; 
		return R;
	}
	
	R = EvaluateInteraction(RequestActor, &ActionTags);
	if (!R.bCanInteraction)
	{
		return R;
	}

	//접근 가능
	
	bIsBusy=true;
	const bool bStarted = ILMInteractable::Execute_Interact(TargetActor, RequestActor);

	if (!bStarted)
	{
		bIsBusy = false;
		R.Failure = InteractionFailure::ExecutionFailed;
	}

	return R;
}




