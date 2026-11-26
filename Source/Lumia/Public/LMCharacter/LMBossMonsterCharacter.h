#pragma once

#include "CoreMinimal.h"
#include "LMAbilitySystem/LMAbilitySet.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMBossMonsterCharacter.generated.h"

class ALMPlayerCharacter;
class ULMAbilitySystemComponent;
class ULMAttackAbility;
class ULMCombatAttributeSet;

// 보스의 근접 공격 실행이 끝나면 실행 번호와 성공 여부를 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLMOnBossMeleeFinished, int64, Run, bool, bSucceeded);

// StateTree가 시작한 근접 공격 한 번의 진행 상태다.
UENUM(BlueprintType)
enum class ELMBossMonsterMeleeResult : uint8
{
	Running,
	Succeeded,
	Failed
};

// 시길룸의 전투 능력을 소유하고 전용 StateTree의 근접 공격 요청을 처리한다.
UCLASS()
class LUMIA_API ALMBossMonsterCharacter : public ALMBaseCharacter
{
	GENERATED_BODY()

public:
	// 시길룸의 전투 컴포넌트와 기본 진영을 설정한다.
	ALMBossMonsterCharacter();

	// 공격을 시작하면 실행 번호를 반환하고, 시작할 수 없으면 0을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Boss")
	int64 TryStartMelee(ALMPlayerCharacter* _TargetPlayer, TSubclassOf<ULMAttackAbility> _AttackClass);

	// 이번 공격의 진행 상태를 반환하며, 지난 실행 번호는 실패로 처리한다.
	UFUNCTION(BlueprintPure, Category = "LM|Boss")
	ELMBossMonsterMeleeResult ReadMeleeResult(int64 _Run) const;

	// StateTree를 이탈하는 Task가 시작한 공격만 취소한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Boss")
	void StopMelee(int64 _Run);

	// 이번 공격을 기다리는 BP 태스크에 종료 결과를 전달한다.
	UPROPERTY(BlueprintAssignable, Category = "LM|Boss")
	FLMOnBossMeleeFinished OnMeleeFinished;

	// 능력 부여가 끝나면 BP에서 트리 시작 조건을 다시 확인할 수 있게 알린다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Boss")
	void OnCombatReady();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LM|Boss")
	bool bCombatReady = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Boss", meta = (ClampMin = "1.0", Units = "cm"))
	float MeleeRange = 200.0f;

protected:
	// 시길룸을 ASC에 연결하고 공격 세트를 부여한다.
	virtual void BeginPlay() override;

	// 실행 중인 공격과 이 몸체가 부여한 능력을 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 같은 실행의 종료 결과를 한 번만 기록하고 구독을 해제한다.
	bool FinishMelee(int64 _Run, ELMBossMonsterMeleeResult _Result);

	UPROPERTY(VisibleAnywhere, Category = "LM|Boss")
	TObjectPtr<ULMAbilitySystemComponent> BossASC;

	UPROPERTY(VisibleAnywhere, Category = "LM|Boss")
	TObjectPtr<ULMCombatAttributeSet> BossAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Boss")
	TObjectPtr<ULMAbilitySet> AttackSet;

	UPROPERTY(Transient)
	FLMAbilitySetGrantedHandles GrantedAttacks;

	int64 MeleeRun = 0;
	ELMBossMonsterMeleeResult MeleeResult = ELMBossMonsterMeleeResult::Failed;
	FGameplayAbilitySpecHandle ActiveAttack;
	FDelegateHandle EndedHandle;
};
