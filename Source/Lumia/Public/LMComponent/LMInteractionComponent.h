// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "LMInterface/LMInteractable.h"
#include "LMInteractionComponent.generated.h"


struct FGameplayTag;

UENUM(BlueprintType)
enum class InteractionFailure:uint8 {
	Succsess=0,
	NotHasAllRequireTags,
	CantAccessActor,
	IsBusy,
	ExecutionFailed,
	InvalidActionTags,
	NotHasInterface
};

USTRUCT(BlueprintType)
struct FLMInteractionResult
{
	GENERATED_BODY()
	//인터렉션 가능 여부
	bool bCanInteraction=false;
	InteractionFailure Failure=InteractionFailure::Succsess;
	FGameplayTagContainer HasTags;
	FGuid InteractionId; 
};
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LUMIA_API ULMInteractionComponent : public UActorComponent,public ILMInteractable
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULMInteractionComponent();
	//인터렉션 가능 여부 체크
	UFUNCTION(BlueprintCallable,Category="Interaction")
	FLMInteractionResult RequestInteraction(AActor* RequestActor ,UPARAM(ref) FGameplayTagContainer& ActionTags);

	constexpr FGuid GetId() const {return InteractionId;}
	constexpr bool GetIsBusy() const{return bIsBusy;}
	UFUNCTION(BlueprintCallable,Category="Interaction")
	void SetIsBusy(bool BUsedCond)
	{
		bIsBusy = BUsedCond;
	}
protected:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Interaction|GameplayTags")
	FGameplayTagContainer HasTagContainer;
	// Called when the game starts
	UPROPERTY(EditAnywhere, Category = "Interaction|UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;
	
	FLMInteractionResult EvaluateInteraction(FGameplayTagContainer* ActionTags);
private:
	FGuid InteractionId;
	
	bool bIsBusy=false;
};
