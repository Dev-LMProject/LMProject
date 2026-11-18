// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LMInteractable.generated.h"
UENUM(BlueprintType)
enum class ELMInteractionRequestHandling : uint8
{
	PassThrough = 0, // 기존 Interact로 진행한다.
	Accepted,        // 별도 기능이 요청을 수락했으므로 여기서 끝낸다.
	Rejected         // 요청을 거절했으므로 여기서 끝낸다.
};
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
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsInteractionBusy() const;
	virtual bool IsInteractionBusy_Implementation() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	ELMInteractionRequestHandling HandleInteractionRequest(AActor* RequestActor);
	virtual ELMInteractionRequestHandling HandleInteractionRequest_Implementation(AActor* RequestActor);
};
