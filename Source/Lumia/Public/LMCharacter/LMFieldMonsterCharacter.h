#pragma once

#include "CoreMinimal.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMFieldMonsterCharacter.generated.h"

class ULMAbilitySystemComponent;
class ULMCombatAttributeSet;
class UWidgetComponent;
class UAnimSequence;
struct FOnAttributeChangeData;

/** 
 * 본인 ASC와 전투 속성을 소유하는 일반 몬스터의 기반 몸체다. 
 * 자체 ASC와 체력, 임시 HP UI, 일반 몬스터의 피격·소진 반응을 관리한다.
 */
UCLASS()
class LUMIA_API ALMFieldMonsterCharacter : public ALMBaseCharacter
{
	GENERATED_BODY()

public:
	/** 몬스터 ASC / 전투 속성 / 체력 표시 컴포넌트를 생성한다. */
	ALMFieldMonsterCharacter();

protected:
	/** 플레이 시작 시 ASC를 몸체에 연결하고 체력 초기화를 확인한다. */
	virtual void BeginPlay() override;
	
	/** 타이머·구독·자신이 추가한 태그를 정리하고 Base 종료를 수행한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** 이 몬스터가 소유하는 GAS 컴포넌트다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Combat")
	TObjectPtr<ULMAbilitySystemComponent> AbilitySystemComponent;

	/** 이 몬스터의 실제 전투 수치를 보관한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Combat")
	TObjectPtr<ULMCombatAttributeSet> CombatAttributes;

	/** 머리 위에 표시할 임시 HP 위젯이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|UI")
	TObjectPtr<UWidgetComponent> HealthWidgetComponent;

	/** AnimBP의 HitReact_Front와 같은 에셋을 지정하여 반응 시간을 맞춘다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Reaction")
	TObjectPtr<UAnimSequence> HitReactionAnimation;

	/** AnimBP가 피격 상태 전환에 읽는 값이다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LM|Reaction")
	bool bIsHitReacting = false;

	/** 일반 몬스터의 HP 소진 상태다. 보스의 공통 사망 정책으로 사용하지 않는다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LM|Reaction")
	bool bHealthDepleted = false;

private:
	/** 현재 HP 또는 최대 HP가 바뀌면 표시값을 갱신한다. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** 자신의 현재 체력을 위젯에 전달한다. */
	void RefreshHealthWidget();

	/** 확정된 피해 결과로 일반 몬스터의 피격 또는 소진을 처리한다. */
	void HandleDamageApplied(float ActualDamage, bool bDepleted);

	/** 피격 제한을 자신이 추가한 만큼만 해제한다. */
	void FinishHitReaction();

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle DamageAppliedHandle;
	FTimerHandle HitReactionTimer;

	bool bOwnsStunTag = false;
	bool bOwnsDeadTag = false;
	bool bEndingPlay = false;
};
