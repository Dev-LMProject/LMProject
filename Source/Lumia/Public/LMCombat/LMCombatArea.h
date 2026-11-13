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
	
private:
// 플레이어의 Capsule이 지정 박스와 실제로 겹치는지 확인한다.
    bool IsInside(const UBoxComponent* Box, const ALMPlayerCharacter* Player) const;

    // 고정 명단에 속한 살아 있는 관찰자의 현재 Sight를 확인한다.
    bool CanSeePlayer(ALMFieldMonsterCharacter* Observer, ALMPlayerCharacter* Player) const;

    // 최초 발견을 확정하고 고정 그룹 전체에 대상을 전달한다.
    void BeginCombat();

    bool bRegistered = false;
    bool bEndingPlay = false;
};
