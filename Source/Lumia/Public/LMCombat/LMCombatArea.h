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
	
	// 실제 Capsule 겹침을 기준으로 목록과 현재 교전을 즉시 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
	void SyncOverlaps();

	// 준비가 늦은 대상도 다시 확인하되 영역당 검사 횟수를 제한한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
	void RequestRecheck();

	// 개별 몬스터의 시야 보고를 영역의 실제 후보와 다시 대조한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
	void ReportPlayerSeen(ALMFieldMonsterCharacter* Observer, ALMPlayerCharacter* Player);

	// 이 영역이 공유한 대상을 해제하고 현재 교전을 종료한다.
	UFUNCTION(BlueprintCallable, Category = "LM|CombatArea")
	void EndCombat();
protected:

	virtual void BeginPlay() override;

	// 영역이 사라질 때 남은 대상과 소속 연결을 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|CombatArea")
	TObjectPtr<USceneComponent> AreaRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|CombatArea")
	TObjectPtr<UBoxComponent> DetectionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|CombatArea")
	TObjectPtr<UBoxComponent> CombatBounds;

	//현재 감지 박스 안에 있는 몬스터
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
	TArray<TObjectPtr<ALMFieldMonsterCharacter>> DetectedMonsters;

	//이미 전투에 들어온 몬스터
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
	TArray<TObjectPtr<ALMFieldMonsterCharacter>> CombatParticipants;

	//감지 박스 내 들어온 플레이어
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
	TObjectPtr<ALMPlayerCharacter> OverlappedPlayer;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
	bool bCombatActive = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "LM|CombatArea")
	int32 AreaRunId = 0;
	
private:
	// 목록 동기화 후 기존 교전을 유지하거나 현재 시야로 최초 교전을 시작한다.
	void RefreshCore();

	// 캐릭터의 실제 Capsule이 영역과 겹치는지 확인한다.
	bool IsInside(const UBoxComponent* Box, const ALMBaseCharacter* Character) const;

	// 현재 소속·영역·생존 조건과 Sight 결과를 함께 확인한다.
	bool CanSeePlayer(ALMFieldMonsterCharacter* Observer, ALMPlayerCharacter* Player) const;

	// 새 교전 상태를 먼저 확정한 뒤 등록된 몬스터들에게 대상을 공유한다.
	void BeginCombat(ALMPlayerCharacter* _ActivePlayer);

	// 살아 있는 소속 몬스터에게 대상 정보를 저장한다.
	void AddParticipant(ALMFieldMonsterCharacter* Monster);

	double NextRecheckTime = 0.0;
	bool bUpdating = false;
	bool bEndingPlay = false;
	bool bIsSight = false;
};
