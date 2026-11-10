#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "LMAttackAbility.generated.h"

class ACharacter;
class UAnimMontage;
class ULMAttackData;
class ULMAbilityTaskWeaponHitTrace;

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
	
	/** 회복 관련 사건을 구독한 뒤 기존 블루프린트의 공격 실행을 시작한다. */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

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
	
	/** 이 공격의 회복 상태와 외부에서 조회할 공통 상태 태그를 함께 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "LM|Attack")
	void SetAttackRecovery(bool bNewRecovery);

	/** 이 공격이 현재 취소 가능한 마무리 구간에 있는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "LM|Attack")
	bool IsInAttackRecovery() const;
	
	/** 검 판정을 연결할 공격에 지정하는 기본 피해와 판정 설정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Attack")
	TObjectPtr<ULMAttackData> AttackData;

private:
	/** 몽타주가 끝까지 재생되면 공격을 정상 종료한다. */
	UFUNCTION()
	void HandleAttackMontageCompleted();
	
	/** 몽타주 실행 실패나 중단을 취소된 공격 종료로 처리한다. */
	UFUNCTION()
	void HandleAttackMontageAborted();

	/** 현재 공격 몽타주에서 발생한 회복 시작 알림만 처리한다. */
	UFUNCTION()
	void HandleRecoveryBegin(FGameplayEventData Payload);

	/** 현재 공격이 회복 중일 때만 취소 요청을 받아들인다. */
	UFUNCTION()
	void HandleRecoveryCancel(FGameplayEventData Payload);
	
	/** 현재 공격 실행에 속한 검 판정 Task다. */
	UPROPERTY(Transient)
	TObjectPtr<ULMAbilityTaskWeaponHitTrace> WeaponHitTraceTask;
	
	/** 현재 공격의 검 판정을 종료하고 보관 중인 참조를 비운다. */
	void StopWeaponHitTrace();

	/** 이 능력이 회복 태그를 추가했는지 기록하여 중복 추가·제거를 방지한다. */
	bool bInAttackRecovery = false;
};
