// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "LMInterface/LMInteractable.h"
#include "LMInteractionComponent.generated.h"
class UPrimitiveComponent;
class ULMInteractorComponent;
struct FHitResult;

struct FGameplayTag;

DECLARE_MULTICAST_DELEGATE(FLMInteractionTagsChanged);

UENUM(BlueprintType)
enum class InteractionFailure:uint8 {

	Succsess=0,
	NotHasAllRequireTags,
	CantAccessActor,
	IsBusy,
	ExecutionFailed,
	InvalidActionTags,
	NotHasInterface
};

USTRUCT(BlueprintType)
struct FLMInteractionResult
{
	GENERATED_BODY()
	//인터렉션 가능 여부
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bCanInteraction=false;
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	InteractionFailure Failure=InteractionFailure::Succsess;
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTagContainer HasTags;
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGuid InteractionId; 
};
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LUMIA_API ULMInteractionComponent : public UActorComponent, public ILMInteractable
{
    GENERATED_BODY()

public:
    /** 대상의 감지는 이벤트로 처리하고 결과 식별용 GUID는 기존 방식으로 만든다. */
    ULMInteractionComponent();

    /** 숨겨진 후보도 태그가 맞게 바뀌면 HUD가 다시 표시할 수 있게 알린다. */
    FLMInteractionTagsChanged InteractionTagsChanged;

    /** 표시와 실행이 같은 태그 포함 규칙을 사용한다. 빈 요청 태그는 허용하지 않는다. */
    bool MatchesInteractionTags(const FGameplayTagContainer& ActionTags) const;

    /** BP에서 컨테이너를 직접 수정해 변경 알림을 우회하지 않도록 복사본을 반환한다. */
    UFUNCTION(BlueprintGetter, Category="Interaction|GameplayTags")
    FGameplayTagContainer GetInteractionTags() const { return HasTagContainer; }

    /** 런타임 태그 변경은 이 함수를 통해 HUD에 즉시 반영한다. */
    UFUNCTION(BlueprintSetter, Category="Interaction|GameplayTags")
    void SetInteractionTags(FGameplayTagContainer InTags);

    /** 실제 동작 직전에 조건을 다시 확인해 오래된 후보 목록만 믿지 않는다. */
    UFUNCTION(BlueprintCallable, Category="Interaction")
    FLMInteractionResult RequestInteraction(
        AActor* RequestActor, UPARAM(ref) FGameplayTagContainer& ActionTags);

    /** 실행 결과의 식별자를 제공하며 후보 중복 판정에는 사용하지 않는다. */
    FORCEINLINE FGuid GetId() const { return InteractionId; }
    /** 외부 동작이 아직 진행 중인지 조회한다. */
    FORCEINLINE bool GetIsBusy() const { return bIsBusy; }

    /** 성공한 외부 동작이 끝났을 때 다시 요청을 받을 수 있게 한다. */
    UFUNCTION(BlueprintCallable, Category="Interaction")
    FORCEINLINE void SetIsBusy(const bool bUsedCond) { bIsBusy = bUsedCond; }

    /** BP 소유 센서를 생성하지 않고 연결하며 교체 시 이전 등록을 정리한다. */
    UFUNCTION(BlueprintCallable, Category="Interaction")
    void SetDetectionCollision(UPrimitiveComponent* InCollision);

    /** 연결하지 않은 다른 콜리전의 접촉을 센서로 오인하지 않게 한다. */
    UPrimitiveComponent* GetDetectionCollision() const;

    // 플레이어의 콜리전이 나중에 연결되는 경우에도 기존 겹침을 다시 확인한다.
    void RefreshDetectedInteractor();

protected:
    UPROPERTY(EditAnywhere, BlueprintGetter=GetInteractionTags, BlueprintSetter=SetInteractionTags,
        Category="Interaction|GameplayTags")
    FGameplayTagContainer HasTagContainer;

    /** 후보 표시와 별개로 실행 시점의 Busy·태그 조건을 판단한다. */
    FLMInteractionResult EvaluateInteraction(
        AActor* RequestActor, FGameplayTagContainer* ActionTags);

    /** BP 또는 사전 초기화에서 연결한 센서도 같은 경로로 동기화한다. */
    virtual void BeginPlay() override;
    /** EndOverlap이 없어도 컴포넌트 종료 시 플레이어에게 해제를 알린다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 콜백을 먼저 끊고 기록을 비워 종료 중 재진입에서도 중복 해제를 피한다. */
    void ClearDetection();

    FGuid InteractionId;
    bool bIsBusy = false;
    bool bIsEndingPlay = false;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> DetectionCollision;

    // 싱글 플레이 대상이므로 현재 감지한 플레이어 하나만 기록한다.
    TWeakObjectPtr<ULMInteractorComponent> DetectedInteractor;

    /** 이벤트와 초기 연결이 같은 판정 경로를 사용하게 한다. */
    UFUNCTION()
    void OnDetectionBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    /** 일부 Body만 이탈했을 때도 남아 있는 겹침을 기준으로 유지한다. */
    UFUNCTION()
    void OnDetectionEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);
};
