#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "LMAbilitySet.generated.h"

class UGameplayAbility;
class ULMAbilitySystemComponent;

/** 부여할 능력의 클래스·레벨·입력 연결을 정의한다. */
USTRUCT(BlueprintType)
struct LUMIA_API FLMAbilitySetEntry
{
	GENERATED_BODY()
	
	/** 
	 * 부여할 GameplayAbility 클래스. 
	 * 실행 중인 능력 객체가 아니라, 부여할 능력의 종류를 지정.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LM|GAS")
	TSubclassOf<UGameplayAbility> AbilityClass;
	
	/** 능력을 부여할 때 기록할 레벨. 최소값은 1이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|GAS", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;
	
	/**
	 * ASC가 입력과 이 능력을 연결할 때 사용할 태그.
	 * 부여 구현에서 AbilitySpec에 기록하고, ASC의 입력 처리에서 비교한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LM|GAS",
		meta=(Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * 실제로 능력을 부여한 결과를 보관하는 런타임 기록.
 * 부여 대상 ASC와 부여된 능력의 핸들을 함께 저장한다.
 * PlayerState나 장비 등 부여를 관리하는 쪽에서 보관한다.
 */
USTRUCT()
struct LUMIA_API FLMAbilitySetGrantedHandles
{
	GENERATED_BODY()
	
	/**
	 * 기록된 ASC에서 Handles에 포함된 능력만 회수한다.
	 * 다른 AbilitySet이나 장비가 부여한 능력을 함께 제거하지 않기 위한 함수다.
	 */
	void RemoveFromAbilitySystem();
	
	/**
	 * 이 능력들을 부여받은 ASC.
	 * 약한 참조를 사용하여 이 기록 때문에 ASC의 수명이 연장되지 않게 한다.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ULMAbilitySystemComponent> AbilitySystem;
	
	/**
	 * 부여에 성공한 AbilitySpec의 식별자 목록.
	 * 같은 GA 클래스라도 각각의 부여를 구분하여 정확히 회수하기 위해 저장한다.
	 */
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> Handles;
};

/**
 * 에디터에서 구성하는 능력 묶음 데이터 에셋.
 * 기본 능력이나 장비 능력 목록을 코드 수정 없이 설정할 수 있게 한다.
 * 여러 캐릭터가 같은 설정을 사용할 수 있으므로 
 */
UCLASS()
class LUMIA_API ULMAbilitySet : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/**
	 * Abilities에 설정한 능력들을 권한이 있는 ASC에 부여한다.
	 * 멀티플레이에서는 서버가 부여를 담당한다.
	 * 능력을 등록하는 함수이며, 실제 활성화 요청은 입력 처리 등에서 담당한다.
	 * 
	 * @param AbilitySystem
	 * 능력을 부여받을 ASC.
	 *
	 * @param SourceObject
	 * 능력의 부여 출처로 AbilitySpec에 기록할 객체.
	 * ASC의 Owner나 Avatar를 설정하는 인자는 아니다.
	 *
	 * @param OutGrantedHandles
	 * 부여 대상 ASC와 성공한 부여의 핸들을 기록할 결과 구조체.
	 * 참조(&)로 받아 호출한 쪽의 기록에 결과를 남긴다.
	 *
	 * 끝의 const는 이 함수가 AbilitySet 자체의 설정을 변경하지 않는다는 뜻이다.
	 * 인자로 받은 ASC와 OutGrantedHandles는 변경할 수 있다.
	 */
	void GiveAbilities(
		ULMAbilitySystemComponent* AbilitySystem,
		UObject* SourceObject,
		FLMAbilitySetGrantedHandles& OutGrantedHandles) const;
	
protected:
	/** AbilitySet이 제공할 능력 설정 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LM|GAS")
	TArray<FLMAbilitySetEntry> Abilities;
};
