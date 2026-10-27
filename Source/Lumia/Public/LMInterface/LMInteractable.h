// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LMInteractable.generated.h"

// This class does not need to be modified.
UINTERFACE()
class ULMInteractable : public UInterface
{
	GENERATED_BODY()

};

/**
 * 
 */
class LUMIA_API ILMInteractable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(AActor* Interactor);
};
