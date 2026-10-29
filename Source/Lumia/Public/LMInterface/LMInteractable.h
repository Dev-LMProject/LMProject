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
	// 요청자의 상호작용 동작을 실행하고 시작 성공 여부를 반환한다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(AActor* Interactor);
};
