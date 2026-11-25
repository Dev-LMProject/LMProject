#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "GameplayAbilitySpecHandle.h"
#include "LMFieldMonsterCharacter.generated.h"

class ALMPlayerCharacter;
class ULMAbilitySystemComponent;
class ULMCombatAttributeSet;
class UWidgetComponent;
class UAnimInstance;
class UAnimSequence;
struct FOnAttributeChangeData;
class ALMCombatArea;
class ULMAttackAbility;

// 기본공격 실행과 공격권을 정리했을 때 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLMOnBasicAttackFinished);

/** 
 * 본인 ASC와 전투 속성을 소유하는 일반 몬스터의 기반 몸체다. 
 * 자체 ASC와 체력, 임시 HP UI, 일반 몬스터의 피격·소진 반응을 관리한다.
 */
UCLASS()
class LUMIA_API ALMFieldMonsterCharacter : public ALMBaseCharacter
{
	GENERATED_BODY()

public:
	// 지정 엘리트에서만 플레이어의 일반 타격을 1 피해와 생존 하한으로 제한한다.
	UFUNCTION(BlueprintCallable, Category = "LM|SceneCombat")
	void SetEliteFightRules(bool _bEliteFightRules);

	// 지정 플레이어의 시연을 준비하거나 끝내며 같은 활성 요청은 타격 기록을 유지한다.
	UFUNCTION(BlueprintCallable, Category = "LM|SceneCombat")
	bool SetSceneFight(ALMPlayerCharacter* _ScenePlayer, bool _bSceneFight);

	// 지정 시연의 첫타와 결정타를 순서대로 한 번씩 기존 피해 경로에 적용한다.
	UFUNCTION(BlueprintCallable, Category = "LM|SceneCombat")
	bool ApplySceneHit(ALMPlayerCharacter* _ScenePlayer, bool bFinalHit);

	// 일반 엘리트 피해와 해당 Spec에만 표시된 시연 예외를 구분한다.
	virtual float ChangeIncomingDamage(const FGameplayEffectSpec& DamageSpec, float RequestedDamage, float OldHealth, float MaximumHealth) override;

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

	// AnimBP가 패링 후퇴 중에만 뒷걸음 자세를 사용하도록 현재 이동 소스를 확인한다.
	bool IsParryRetreating() const;

	// AnimBP에 현재 그로기 상태를 전달한다.
	bool IsGroggy() const;
	
	// 이동 값을 읽고 BP가 사용할 공격 허가와 행동 유지 판정을 함께 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	bool ReadMoveData(ALMPlayerCharacter*& _TargetPlayer, FVector& _StartLocation, bool& _bReturning, bool& _bCanMove);

	// 공격 접근과 대기 이동을 갱신하고 반납 이후의 이격 완료를 보고한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	bool ReadCombatMoveData(FVector& CombatLocation, double& CombatDistance, bool& bAtCombatLocation, bool& bNeedMove);

	// 피격 높이나 차폐와 별개로 현재 대상의 공격 허용 반경을 벗어났는지 확인한다.
	bool IsOutsideAttackDistance() const;
	
	// 대상이 없는 복귀 성공에서만 최초 방향을 복원하고 복귀를 완료한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	void FinishReturn();

protected:
	// 시연 첫타는 최대 HP의 이 비율을 깎되 최소 HP 1을 남긴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|SceneCombat", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SceneHitDamageRatio = 0.5f;

	// 일반 몬스터의 공격이 패링되면 경직과 후퇴를 연결한다.
	virtual void ReceiveParryReaction(ALMBaseCharacter* Parrier) override;

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

	// 패링당했을 때 지상에서 물러나는 목표 거리다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Reaction", meta = (ClampMin = "0.0", Units = "cm"))
	float ParryRetreatDistance = 120.0f;

	// 패링 피격이 끝난 뒤 추가로 행동을 막는 시간이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Reaction", meta = (ClampMin = "0.0", Units = "s"))
	float ParryGroggyDuration = 1.5f;

	/** AnimBP가 피격 상태 전환에 읽는 값이다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LM|Reaction")
	bool bIsHitReacting = false;

	/** 일반 몬스터의 HP 소진 상태다. 보스의 공통 사망 정책으로 사용하지 않는다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LM|Reaction")
	bool bIsDead = false;
	


private:
#if WITH_DEV_AUTOMATION_TESTS
	// UI를 실행하지 않고 기존 몬스터 사망 수신자를 연결하는 검사에만 사용한다.
	friend class FLMEliteCombatDamageTest;
#endif

	TWeakObjectPtr<ALMPlayerCharacter> ScenePlayer;
	bool bEliteFightRules = false;
	bool bSceneFight = false;
	bool bSceneFirstHitDone = false;
	bool bSceneFinalHitDone = false;

	/** 현재 HP 또는 최대 HP가 바뀌면 표시값을 갱신한다. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** 자신의 현재 체력을 위젯에 전달한다. */
	void RefreshHealthWidget();

	// 확정된 피해로 체력이 소진되면 사망을 처리한다.
	void HandleDamageApplied(float ActualDamage, bool _bIsDead);

	// 패링 경직을 시작하고 그로기 전환을 예약한다.
	bool BeginParryHitReaction();

	// 이전 공격이 정리된 뒤 저장한 방향으로 한 번 후퇴한다.
	void TryStartParryRetreat();

	// 이 패링이 등록한 후퇴와 임시 시선을 정리한다.
	void StopParryRetreat();

	// 패링 피격이 끝나면 현재 경직을 유지한 채 그로기로 전환한다.
	void BeginParryGroggy();

	/** 피격 제한을 자신이 추가한 만큼만 해제한다. */
	void FinishHitReaction();

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle DamageAppliedHandle;
	FTimerHandle HitReactionTimer;
	FVector PendingParryRetreatDirection = FVector::ZeroVector;
	uint16 ParryRetreatSourceId = 0;
	bool bIsGroggy = false;
	
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
	// 필드 몬스터의 현재 공격권으로 지정된 기본공격 GA를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	bool TryStartBasicAttack();

	// BP가 GA를 실행하기 전에 공격권과 종료 감시를 준비한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	bool PrepareAttack();

	// 이동과 기본공격을 중단하며 실제 GA 종료까지 공격권을 유지한다.
	UFUNCTION(BlueprintCallable, Category = "LM|AI")
	void StopAttack();

	// 기본공격 정리가 끝나면 대기 중인 BP 태스크에 알린다.
	UPROPERTY(BlueprintAssignable, Category = "LM|AI")
	FLMOnBasicAttackFinished OnBasicAttackFinished;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Attack")
	TSubclassOf<ULMAttackAbility> AttackAbilityClass;

	// 캡슐 중심 사이의 수평 거리로 판단하는 기본공격 시작 사거리다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Attack", meta = (ClampMin = "1.0", Units = "cm"))
	float AttackDistance = 200.0f;

	// 지상 기본공격에서 허용하는 두 캡슐 발 위치의 높이 차이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Attack", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackHeight = 80.0f;

	// 반납 직후 이격 목표가 도착 오차를 감안해도 공격 반경 밖에 오도록 여유를 준다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|AI", meta = (ClampMin = "1.0", Units = "cm"))
	float RetreatClearance = 10.0f;

	// 공격권 없이 추적하거나 원호를 따라 이동할 때 적용할 속도 비율이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|AI", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FollowSpeedScale = 0.4f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|AI", meta = (AllowPrivateAccess = "true"))
	bool bAttackRunning = false;

private:
	// 현재 대상이 사거리와 발 높이·벽 조건을 만족하는지 같은 기준으로 검사한다.
	bool CanStartAttack() const;

	// 대기 이동을 시작할 때 속도를 낮추고, 끝날 때 이전 속도를 복원한다.
	void SetSlowFollow(bool _bSlowFollowing);

	// 대기 중에는 거리를 좁히거나 플레이어 양옆의 원호로 이동한다.
	bool ReadWaitMoveLocation(const ALMCombatArea& QueueArea, const ALMPlayerCharacter& CombatPlayer, FVector& Location);

	bool bSlowFollowing = false;
	bool bClosingWaitDistance = false;
	bool bWaitArcRight = true;
	FVector WaitArcOffset = FVector::ZeroVector;
	float SavedWalkSpeed = 0.0f;

	// 같은 실행의 실제 종료만 반영하고 해당 공격권을 한 번 반납한다.
	void FinishAttack(TWeakObjectPtr<ALMCombatArea> _RunArea, int64 _RunCombatId, int64 _RunAttackId);

	// 평타 섹션이 끝나기 직전에 다음 타를 이어갈 사거리를 확인한다.
	UFUNCTION()
	void HandleAttackSectionNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
	
	// 현재 공격권으로 기본공격 GA를 한 번 실행한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|Attack", meta = (AllowPrivateAccess = "true"))
	FGameplayAbilitySpecHandle AttackAbilityHandle;
	FDelegateHandle AttackEndedHandle;
	TWeakObjectPtr<UAnimInstance> AttackNotifyAnim;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALMCombatArea> RunArea;

	int64 RunCombatId = 0;
	int64 RunAttackId = 0;
	
	//플레이어 주시용 함수
	void UpdateCombatFocus();
};
