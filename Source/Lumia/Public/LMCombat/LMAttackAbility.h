// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LMAttackAbility.generated.h"

class ACharacter;
class UAnimMontage;

/** 공격을 시작할 수 있는 이동 상태를 지정한다. */
UENUM(BlueprintType)
enum class ELMAttackMovementRequirement : uint8
{
	Any,
	Grounded,
	Airborne
};

/** 캐릭터 종류와 관계없이 공격 실행과 몽타주 종료를 공통 처리한다. */
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	/** 공격 능력의 공통 실행 정책과 상태 태그를 설정한다. */
	ULMAttackAbility();
	
	/** 기본 GAS 조건에 더해 지상 / 공증 실행 조건을 검사한다. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	/** 종료 경로와 관계없이 이 공격이 변경한 루트 모션 배율을 정리한다. */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
protected:
	/** 능력을 실제로 수행하는 Avatar 캐릭터를 반환한다. */
	UFUNCTION(BlueprintPure, Category="LM|Attack")
	ACharacter* GetAttackCharacter() const;
	
	/** 몽타주를 시작하고 완료·중단·실패를 이 능력의 종료와 연결한다. */
	UFUNCTION(BlueprintCallable, Category = "LM|Attack")
	bool PlayAttackMontage(UAnimMontage* Montage, FName StartSection, float RootMotionTranslationScale = 1.0f);
	
	/** 공격을 시작할 수 있는 이동 상태 조건. 기본값은 지상이며, 낙하 공격은 Airborne으로 설정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Attack")
	ELMAttackMovementRequirement MovementRequirement = ELMAttackMovementRequirement::Grounded;
	
private:
	/** 몽타주가 끝까지 재생되면 공격을 정상 종료한다. */
	UFUNCTION()
	void HandleAttackMontageCompleted();
	
	/** 몽타주 실행 실패나 중단을 취소된 공격 종료로 처리한다. */
	UFUNCTION()
	void HandleAttackMontageAborted();
};
