#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/PlayerState.h"
#include "LMPlayerState.generated.h"

/**
 * 플레이어의 ASC를 소유하고, 공격 능력 부여와 조회를 담당하는 클래스
 * 
 * 캐릭터와 별도로 존재하는 PlayerState에 ASC를 두면, 캐릭터가 사망하거나 교체되어도 같은 PlayerState가 유지되는 동안 ASC를 유지할 수 있습니다.
 * 새 캐릭터와 ASC를 연결하는 초기화는 별도로 필요합니다.
 */
UCLASS()
class LUMIA_API ALMPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	/** 플레이어가 소유할 ASC를 한 번 생성한다. */
	ALMPlayerState();

	/** 이 플레이어가 사용하는 ASC를 반환한다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 공격 능력이 아직 없을 때만 ASC에 부여한다. */
	void EnsureAttackAbilityGranted();

	/** 부여한 공격 능력의 식별자를 반환한다. */
	FGameplayAbilitySpecHandle GetAttackAbilityHandle() const;

protected:
	/** 플레이어가 소유하며, 능력 부여 실행과 GameplayEffect 등을 관리하는 ASC. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** ASC에 부여할 공격 능력 클래스. 클래스 기본값에서 설정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LM|GAS")
	TSubclassOf<UGameplayAbility> AttackAbilityClass;

private:
	/** ASC에 부여한 공격 능력을 식별하는 핸들. 부여 전에는 유효하지 않다. */
	FGameplayAbilitySpecHandle AttackAbilityHandle;
};
