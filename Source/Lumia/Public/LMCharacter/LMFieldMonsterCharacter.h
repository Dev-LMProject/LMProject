#pragma once

#include "CoreMinimal.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "GameplayAbilitySpecHandle.h"
#include "LMFieldMonsterCharacter.generated.h"

class ALMPlayerCharacter;
class ULMAbilitySystemComponent;
class ULMCombatAttributeSet;
class UWidgetComponent;
class UAnimSequence;
struct FOnAttributeChangeData;
class ALMCombatArea;
class ULMAttackAbility;

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
	
	// 시야 보고를 보낼 소속 영역을 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|CombatArea")
	FORCEINLINE ALMCombatArea* GetCombatArea() const{return CombatArea.Get();}

	// 다른 유효 영역에 속하지 않았을 때만 이 영역에 등록한다.
	bool RegisterCombatArea(ALMCombatArea* _CombatArea);

	// 실제 소속 영역이 요청한 경우 대상과 소속을 함께 해제한다.
	void UnregisterCombatArea(ALMCombatArea* _CombatArea);

	// 자기 영역이 공유한 대상만 저장하며 None을 받으면 해제한다.
	bool SetTargetPlayer(ALMCombatArea* SourceArea, ALMPlayerCharacter* _TargetPlayer);

	// 현재 영역에서 공유받은 플레이어를 반환한다. 
	// BP와 StateTree가 같은 대상 저장소를 읽도록 한다.
	UFUNCTION(BlueprintPure, Category = "LM|CombatArea")
	FORCEINLINE ALMPlayerCharacter* GetTargetPlayer() const{return TargetPlayer.Get();}

	// ASC 준비 여부와 구분하여 확정된 사망 상태를 반환한다.
	// 준비되지 않은 몬스터를 사망한 몬스터로 취급하지 않는다.
	FORCEINLINE bool IsDead() const{return bIsDead;}
	
	FORCEINLINE bool IsHitReacting() const{return bIsHitReacting;}
	
	// 이동 값을 읽고 BP가 사용할 공격 허가와 행동 유지 판정을 함께 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	bool ReadMoveData(ALMPlayerCharacter*& _TargetPlayer, FVector& _StartLocation, bool& _bReturning, bool& _bCanMove);

	// 대상이 없는 복귀 성공에서만 최초 방향을 복원하고 복귀를 완료한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	void FinishReturn();

protected:
	// 아래 값은 ReadMoveData에서 갱신하는 스냅샷이며 실제 허가는 CombatArea가 소유한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue")
	bool bHasAttackTurn = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue")
	int64 AttackCombatId = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue")
	int64 AttackId = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue")
	bool bAttackStopping = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue")
	int64 WaitId = 0;

	// 현재 교전에서 사용할 수 있는 공격권인지 나타낸다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue")
	bool bAttackAllowed = false;

	// 경로 이동 또는 유효한 공격을 계속할 수 있는지 나타낸다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|AI")
	bool bCanWork = false;

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
	bool bIsDead = false;
	


private:
	/** 현재 HP 또는 최대 HP가 바뀌면 표시값을 갱신한다. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** 자신의 현재 체력을 위젯에 전달한다. */
	void RefreshHealthWidget();

	/** 확정된 피해 결과로 일반 몬스터의 피격 또는 소진을 처리한다. */
	void HandleDamageApplied(float ActualDamage, bool _bIsDead);

	/** 피격 제한을 자신이 추가한 만큼만 해제한다. */
	void FinishHitReaction();

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle DamageAppliedHandle;
	FTimerHandle HitReactionTimer;
	
	bool bOwnsStunTag = false;
	bool bOwnsDeadTag = false;
	bool bEndingPlay = false;
	
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "LM|CombatArea")
	TObjectPtr<ALMCombatArea> CombatArea;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "LM|CombatArea")
	TWeakObjectPtr<ALMPlayerCharacter> TargetPlayer;

	// 이동을 시작하기 전 배치된 위치를 한 번만 저장한다.
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "LM|AI")
	FVector StartLocation = FVector::ZeroVector;

	// 복귀가 끝나면 배치 당시 바라보던 방향으로 다시 맞춘다.
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "LM|AI")
	FRotator StartRotation = FRotator::ZeroRotator;

	// 실제 교전 대상이 해제된 뒤 최초 위치로 돌아가는 동안 유지한다.
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "LM|AI")
	bool bReturning = false;
	
//GA
public:
	// BP가 GA를 실행하기 전에 공격권과 종료 감시를 준비한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	bool PrepareAttack();

	// 이동과 기본공격을 중단하며 실제 GA 종료까지 공격권을 유지한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	void StopAttack();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Attack")
	TSubclassOf<ULMAttackAbility> AttackAbilityClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|AI")
	bool bAttackRunning = false;

private:
	// 같은 실행의 실제 종료만 반영하고 해당 공격권을 한 번 반납한다.
	void FinishAttack(TWeakObjectPtr<ALMCombatArea> _RunArea, int64 _RunCombatId, int64 _RunAttackId);
	
	// 현재 공격권으로 기본공격 GA를 한 번 실행한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|Attack", meta = (AllowPrivateAccess = "true"))
	FGameplayAbilitySpecHandle AttackAbilityHandle;
	FDelegateHandle AttackEndedHandle;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALMCombatArea> RunArea;

	int64 RunCombatId = 0;
	int64 RunAttackId = 0;
	
	//플레이어 주시용 함수
	void UpdateCombatFocus();
};
