#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LMCombatAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

/** 실제 HP 감소량과 소진 여부를 알린다. 소진 이후의 사망·무력화는 수신 캐릭터의 정책이다. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FLMOnDamageApplied, float, bool);

/** 플레이어·일반 몬스터·보스가 공유할 전투 수치와 HP 차감을 관리하며 종류별 피격 반응은 결정하지 않는다. */
UCLASS()
class LUMIA_API ULMCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** 생성 시 체력과 임시 피해 속성의 기본값을 설정한다. */
	ULMCombatAttributeSet();

	// 원래 유효 피해를 소비하고 캐릭터별 보호와 실제 체력 감소 결과를 반영한다.
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// 효과 적용과 직접 변경 모두에서 검 게이지의 현재값 범위를 지킨다.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// 기본값을 바꾸는 공통 충전·소비 경로에도 같은 범위를 적용한다.
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	// 최대 충전량이 줄어들면 현재 충전량도 함께 줄인다.
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 특수공격에 사용할 현재 충전량이며 수치의 유일한 원본이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Special")
	FGameplayAttributeData SpecialCharge;

	// 완충 및 첫 타격 소비의 기준이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Special")
	FGameplayAttributeData MaxSpecialCharge;

	// 검 게이지 속성의 조회·설정·초기화 접근자를 제공한다.
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ULMCombatAttributeSet, SpecialCharge)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(SpecialCharge)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(SpecialCharge)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(SpecialCharge)
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ULMCombatAttributeSet, MaxSpecialCharge)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxSpecialCharge)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxSpecialCharge)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxSpecialCharge)

	/** 현재 남아 있는 체력이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Combat")
	FGameplayAttributeData Health;

	/** 현재 체력이 가질 수 있는 최대값이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Combat")
	FGameplayAttributeData MaxHealth;

	/** 이번에 전달받은 피해량이다. 처리한 뒤 즉시 0으로 비운다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Combat")
	FGameplayAttributeData IncomingDamage;
	
	/** UI용 속성 변경과 구분되는 실제 피해 결과 알림이다. */
	FLMOnDamageApplied OnDamageApplied;

	/** Health의 속성 정보 조회, 현재값 조회·설정, 초기화 함수를 생성한다. */
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ULMCombatAttributeSet, Health)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

	/** MaxHealth의 속성 정보 조회, 현재값 조회·설정, 초기화 함수를 생성한다. */
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ULMCombatAttributeSet, MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

	/** IncomingDamage의 속성 정보 조회, 현재값 조회·설정, 초기화 함수를 생성한다. */
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ULMCombatAttributeSet, IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(IncomingDamage)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(IncomingDamage)

private:
	// 음수·비정상 수치가 UI와 소비 판정으로 전달되지 않도록 제한한다.
	void ClampSpecialCharge(const FGameplayAttribute& Attribute, float& NewValue) const;
};
