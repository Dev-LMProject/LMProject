// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "LMAbilityTaskScanFade.generated.h"

/**
 * 
 */
//Fade효과를 위한 Alpha값 전달용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMScanFadeDelegate,float,Alpha);
UCLASS()
class LUMIA_API ULMAbilityTaskScanFade : public UAbilityTask
{
	GENERATED_BODY()
public:
	ULMAbilityTaskScanFade(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(BlueprintAssignable)
	FLMScanFadeDelegate OnUpdate;
	
	UPROPERTY(BlueprintAssignable)
	FLMScanFadeDelegate OnFinished;
	
	//Task를 실행하는 GA를 소유자로 자동 전달하기 위해 HidePin,DefaultToSelf 부여
	UFUNCTION(BlueprintCallable,Category = "LM|Ability|Tasks",meta = (DisplayName = "Scan Fade",HidePin = "OwningAbility",DefaultToSelf = "OwningAbility",BlueprintInternalUseOnly = "true"))
	static ULMAbilityTaskScanFade* CreateScanFade(UGameplayAbility* OwningAbility,float Duration);
	
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	
private:
	float DurationSec=0.f;
	float ElapsedSec=0.f;
	
	
};
