// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LMCombatArea.generated.h"

class ALMBaseCharacter;
class ALMFieldMonsterCharacter;
class ALMPlayerCharacter;
class UBoxComponent;
class USceneComponent;

UCLASS()
class LUMIA_API ALMCombatArea : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALMCombatArea();
	
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
	
	// 공격권 소유자는 플레이어 위치를 읽고 나머지는 외부 대기 슬롯을 읽는다.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LM|CombatArea")
	bool ReadCombatLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location);

	// 플레이어를 중심으로 외부 대기 슬롯의 위치를 계산한다.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LM|CombatPlace")
	bool GetSlotLocation(ALMFieldMonsterCharacter* Monster, int32 SlotIndex, FVector& Location) const;
	
	// 실제 경로 이동과 같은 반경·높이 기준으로 도착 여부를 조회한다.
	UFUNCTION(BlueprintPure, Category = "LM|CombatPlace")
	bool IsAtPlace(ALMFieldMonsterCharacter* Monster, const FVector& Location) const;
	
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

	// 같은 교전과 같은 대기 번호의 바깥 대기 구역 도착 또는 이탈만 반영한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatQueue")
	void ReportWaitReady(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 WaitId, bool bReady);
protected:

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
	
	// 외부 대기 링의 반경이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float WaitDistance = 500.0f;

	// 외부 대기 링의 슬롯 개수다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "1"))
	int32 WaitSlotCount = 12;

	// 전투 위치에 도착했다고 판단하는 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float MoveReach = 50.0f;

	// 공간 계산에 확보하고 재이동 판단에도 사용하는 여유 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceReach = 100.0f;

	// NavMesh가 후보 위치를 수평으로 보정할 수 있는 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceShift = 40.0f;

	// 캡슐과 이동 오차 외에 추가로 확보하는 간격이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceGap = 50.0f;

	// NavMesh 위치 보정에 허용하는 높이 차이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceHeight = 150.0f;

	// 대기 위치에서 간보기 이동을 위해 확보하는 거리다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace", meta = (ClampMin = "0.0", Units = "cm"))
	float PlaceMove = 80.0f;

	// 기준 방향에서 위치 후보를 탐색할 각도다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatPlace")
	TArray<float> PlaceAngles = {0.0f, 30.0f, -30.0f, 60.0f, -60.0f};
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|CombatQueue", meta = (ClampMin = "1"))
	int32 MaxAttackers = 1;
	
private:
// 플레이어의 Capsule이 지정 박스와 실제로 겹치는지 확인한다.
    bool IsInside(const UBoxComponent* Box, const ALMPlayerCharacter* Player) const;

    // 고정 명단에 속한 살아 있는 관찰자의 현재 Sight를 확인한다.
    bool CanSeePlayer(ALMFieldMonsterCharacter* Observer, ALMPlayerCharacter* Player) const;

    // 최초 발견을 확정하고 고정 그룹 전체에 대상을 전달한다.
    void BeginCombat();

    bool bRegistered = false;
    bool bEndingPlay = false;
	
	// 개전할 때 고정 명단의 살아 있는 몬스터에게 서로 다른 방향을 배정한다.
	void SetCombatPlaces();

	// 공격 중에도 유지하는 몬스터별 외부 복귀 슬롯 번호다.
	TMap<TWeakObjectPtr<ALMFieldMonsterCharacter>, int32> WaitSlots;

	// 대기 순서와 이전 공격 이후의 복귀 조건을 보관한다.
	struct FLMQueueEntry
	{
		TWeakObjectPtr<ALMFieldMonsterCharacter> Monster;
		int64 WaitId = 0;
		bool bWaitRequired = false;
	};

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
	
	// 외부 대기 링의 빈 슬롯 중 현재 몬스터와 가장 가까운 번호를 찾는다.
	int32 FindFreeSlot(ALMFieldMonsterCharacter* Monster, FVector& Location) const;
};
