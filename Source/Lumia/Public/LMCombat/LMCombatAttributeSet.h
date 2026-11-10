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

	/** GameplayEffect가 전달한 피해를 소비하고 실제 체력 감소량을 계산한다. */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

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
};
