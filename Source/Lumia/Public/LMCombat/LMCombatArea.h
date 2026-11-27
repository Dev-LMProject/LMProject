// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "LMCombatArea.generated.h"

class ALMBaseCharacter;
class ALMFieldMonsterCharacter;
class ALMPlayerCharacter;
class UBoxComponent;
class USceneComponent;
class UAbilitySystemComponent;

struct FGameplayTag;

// 지정한 몬스터 묶음의 사망이 모두 확인됐을 때 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLMOnMobsCleared);

UCLASS()
class LUMIA_API ALMCombatArea : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALMCombatArea();
	
	// 지정한 몬스터의 사망 감지를 시작하며 잘못된 목록이나 중복 시작은 거부한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
	bool WatchMobs(const TArray<ALMFieldMonsterCharacter*>& Mobs);

	// 감지 시작 한 번당 모든 대상의 사망을 한 번만 알린다.
	UPROPERTY(BlueprintAssignable, Category = "LM|CombatArea")
	FLMOnMobsCleared OnMobsCleared;

	// 배치된 몬스터를 복구하고 방 BP와 레벨의 재도전 준비를 알린다.
	UFUNCTION(BlueprintCallable, Category = "LM|Retry")
	bool ResetForRetry();

	// 소형방의 큐브 잠금과 사망 집계처럼 맵별 연결을 다시 준비한다.
	UPROPERTY(BlueprintAssignable, Category = "LM|Retry")
	FLMOnMobsCleared OnRoomReset;
	
    // 시작 시 감지 박스 안의 몬스터를 한 번만 등록한다.
    UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
    void RegisterMonsters();

    // 두 박스의 플레이어 입장과 퇴장을 이벤트 시점에 처리한다.
    UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
    void HandlePlayerOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp);

    // 감지 성공 보고가 실제 현재 Sight와 일치하면 그룹 전투를 시작한다.
    UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
    void ReportPlayerSeen(ALMFieldMonsterCharacter* Observer, AActor* Player, bool bSuccessfullySensed);

    // 교전 표시와 그룹 전체의 대상을 해제하되 소속은 유지한다.
    UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
    void EndCombat();
	
	// 공격 접근과 저속 추적에 사용할 플레이어 위치를 읽는다.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LM|CombatArea")
	bool ReadCombatLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location);

	// 반납한 몬스터에게 해당 번호의 이격 이동이 아직 필요한지 확인한다.
	bool NeedsRetreat(const ALMFieldMonsterCharacter* Monster, int64 WaitId) const;

	// 같은 대기 번호의 실제 후퇴 거리와 양쪽 간격을 확인한다.
	bool CanFinishRetreat(const ALMFieldMonsterCharacter& Monster, int64 WaitId) const;

	// 공격권을 반납한 위치를 기준으로 이동 가능한 후퇴 목표를 찾는다.
	bool ReadAttackRetreatLocation(const ALMFieldMonsterCharacter& Monster, const ALMPlayerCharacter& Player, float AttackRadius, float Clearance, int64 WaitId, FVector& Location) const;

	// 공격 반경 밖의 이격 지점을 몬스터의 NavMesh에서 찾는다.
	bool ReadRetreatLocation(const ALMFieldMonsterCharacter& Monster, const ALMPlayerCharacter& Player, float AttackRadius, float Clearance, FVector& Location) const;
	
	// 원호 기준과 진행 방향을 갱신하고 대기 몬스터가 옆걸음으로 갈 위치를 찾는다.
	bool ReadWaitArcLocation(const ALMFieldMonsterCharacter& Monster, const FVector& PlayerCenter, FVector& ArcOffset, float AttackRadius, bool& bRight, FVector& Location) const;

	// 실제 경로 이동과 같은 반경·높이 기준으로 도착 여부를 조회한다.
	UFUNCTION(BlueprintPure, Category = "LM|CombatPlace")
	bool IsAtPlace(ALMFieldMonsterCharacter* Monster, const FVector& Location) const;

	// 공격권 없이 이동할 때 사용할 전투 구역의 도착 반경을 반환한다.
	float GetMoveReach() const { return MoveReach; }
	
	// 준비 완료나 피격 회복 사건에서 현재 큐의 빈 공격 정원을 보충한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatQueue")
	void FillAttackSlots();

	// 몬스터의 기존 데이터 갱신 경로에서 허가 기록과 대기 번호를 읽는다.
	bool ReadAttackTurn(ALMFieldMonsterCharacter* Monster, int64& _CombatId, int64& AttackId, bool& bStopping, int64& WaitId) const;

	// 현재 교전 번호를 변경 없이 반환한다.
	FORCEINLINE int64 GetCombatId() const { return CombatId; }

	// 실제 공격 종료가 확인된 정확한 기록만 반납한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatQueue")
	bool FinishAttackTurn(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 AttackId, bool bRequeue = true);

	// 중단 의도만 기록하며 실제 종료 전에는 정원을 비우지 않는다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatQueue")
	bool RequestStopAttackTurn(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 AttackId);

	// 같은 교전과 대기 번호의 이격 완료를 한 번 반영한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatQueue")
	void ReportWaitReady(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 WaitId, bool bReady);

	// 저속 추적에서 원호 배회로 전환하는 플레이어와의 수평 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "1.0", Units = "cm"))
	float WaitDistance = 500.0f;

	// 원호 배회 중 저속 추적을 다시 시작하는 수평 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "1.0", Units = "cm"))
	float FollowDistance = 600.0f;

	// 배회를 시작한 방향에서 좌우로 이동할 최대 각도다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "15.0", ClampMax = "75.0", Units = "deg"))
	float WaitArcAngle = 40.0f;
protected:
	// 몬스터를 복구하기 전에 방이 소유한 시퀀스와 QTE를 정리한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Retry", meta = (ForceAsFunction))
	void OnRetryStarted();

	// 몬스터 복구가 끝나면 낙하 없이 다시 입장할 상태를 준비한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Retry", meta = (ForceAsFunction))
	void OnRetryReady();

	virtual void BeginPlay() override{ bRegistered = false; bEndingPlay = false; Super::BeginPlay();}

	// 영역이 사라질 때 남은 대상과 소속 연결을 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|CombatArea")
    TObjectPtr<USceneComponent> AreaRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|CombatArea")
    TObjectPtr<UBoxComponent> DetectionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|CombatArea")
    TObjectPtr<UBoxComponent> CombatBounds;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
    TArray<TObjectPtr<ALMFieldMonsterCharacter>> RegisteredMonsters;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
    TObjectPtr<ALMPlayerCharacter> TargetPlayer;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
    bool bIsSight = false;
	
	// 전투 위치에 도착했다고 판단하는 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float MoveReach = 50.0f;

	// 공격권 반납 후 몬스터 캡슐 표면 사이에 남겨야 할 수평 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float RetreatGap = 30.0f;

	// 기존 Reader가 전달하는 재이동 여유 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceReach = 100.0f;

	// NavMesh가 후보 위치를 수평으로 보정할 수 있는 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceShift = 40.0f;

	// NavMesh 위치 보정에 허용하는 높이 차이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatQueue", meta = (ClampMin = "1"))
	int32 MaxAttackers = 1;
	
private:
	// 사망 태그가 추가된 대상의 구독을 끝내고 남은 대상을 확인한다.
	void HandleMobDeath(FGameplayTag Tag, int32 TagCount, TWeakObjectPtr<UAbilitySystemComponent> MobKey);
	
	// 몬스터의 사망 처리가 끝난 다음 타이머 실행 시점에 완료를 알린다.
	void FinishMobWatch();

	// 완료 또는 영역 종료 때 남은 구독과 완료 예약을 정리한다.
	void StopMobWatch();

	// 사망을 아직 받지 못한 대상의 구독만 보관하며 대상 수명은 연장하지 않는다.
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle> PendingMobDeaths;
	FTimerHandle MobsClearedTimer;
	bool bWatchingMobs = false;
	
	// 후보 위치가 현재 몬스터의 NavMesh 보정 범위에 있는지 확인한다.
	bool ProjectCombatLocation(const ALMFieldMonsterCharacter& Monster, const FVector& Candidate, FVector& Location) const;

	// 플레이어의 Capsule이 지정 박스와 실제로 겹치는지 확인한다.
    bool IsInside(const UBoxComponent* Box, const ALMPlayerCharacter* Player) const;

    // 고정 명단에 속한 살아 있는 관찰자의 현재 Sight를 확인한다.
    bool CanSeePlayer(ALMFieldMonsterCharacter* Observer, ALMPlayerCharacter* Player) const;

    // 최초 발견을 확정하고 고정 그룹 전체에 대상을 전달한다.
    void BeginCombat();

    bool bRegistered = false;
    bool bEndingPlay = false;
	bool bResettingForRetry = false;
	
	// 대기 순서와 이전 공격 이후의 복귀 조건을 보관한다.
	struct FLMQueueEntry
	{
		TWeakObjectPtr<ALMFieldMonsterCharacter> Monster;
		int64 WaitId = 0;
		bool bWaitRequired = false;
		FVector RetreatStart = FVector::ZeroVector;
		// 옛 공격권이 아직 반납되지 않은 대기 항목은 거리를 기록하지 않는다.
		float RetreatDistance = -1.0f;
	};

	// 몬스터와 대기 번호가 일치하는 후퇴 대기 항목을 읽는다.
	const FLMQueueEntry* FindRetreatEntry(const ALMFieldMonsterCharacter* Monster, int64 WaitId) const;

	// 이동 중 몸체 충돌과 도착 지점의 몬스터 간격을 함께 확인한다.
	bool HasRetreatSpace(const ALMFieldMonsterCharacter& Monster, const FVector& GoalLocation, double GoalAllowance) const;

	// 접근·공격·종료 대기 중인 허가를 보관한다.
	struct FLMAttackTurn
	{
		TWeakObjectPtr<ALMFieldMonsterCharacter> Monster;
		//이번 전투 번호
		int64 CombatId = 0;
		//개별 공격 허가 번호
		int64 AttackId = 0;
		bool bStopping = false;
	};

	// 고정 명단에 속한 살아 있는 몬스터인지 확인한다.
	bool IsQueueMember(ALMFieldMonsterCharacter* Monster) const;

	// 현재 허가와 종료 대기를 모두 포함해 몬스터의 기록을 찾는다.
	int32 FindAttackTurn(ALMFieldMonsterCharacter* Monster) const;

	//기다리는 순서
	TArray<FLMQueueEntry> AttackQueue;
	//현재 정원을 점유한 공격 허가된 몬스터
	TArray<FLMAttackTurn> ActiveAttacks;

	// 대기 상태가 현재 교전 번호를 읽되 직접 변경하지 못하게 한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatQueue", meta = (AllowPrivateAccess = "true"))
	int64 CombatId = 0;
	int64 NextAttackId = 0;
};
