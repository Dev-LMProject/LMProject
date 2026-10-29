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
    // Tick을 비활성화하고 상호작용 식별자와 진행 상태를 초기화한다.
    ULMInteractionComponent();

    // 숨겨진 후보도 태그가 맞게 바뀌면 HUD가 다시 표시할 수 있게 알린다.
    FLMInteractionTagsChanged InteractionTagsChanged;

    // 요청 태그가 비어 있지 않고 대상의 태그에 모두 포함되는지 반환한다.
    bool MatchesInteractionTags(const FGameplayTagContainer& ActionTags) const;

    // 대상이 가진 상호작용 태그의 복사본을 반환한다.
    UFUNCTION(BlueprintGetter, Category="Interaction|GameplayTags")
    FGameplayTagContainer GetInteractionTags() const { return HasTagContainer; }

    // 대상의 태그를 변경하고 태그 변경 델리게이트를 발행한다.
    UFUNCTION(BlueprintSetter, Category="Interaction|GameplayTags")
    void SetInteractionTags(FGameplayTagContainer InTags);

    // 요청자·태그·인터페이스·진행 상태를 검사하고 Interact를 실행한다.
    UFUNCTION(BlueprintCallable, Category="Interaction")
    FLMInteractionResult RequestInteraction(
        AActor* RequestActor, UPARAM(ref) FGameplayTagContainer& ActionTags);

    // 상호작용 식별자를 반환한다.
    FORCEINLINE FGuid GetId() const { return InteractionId; }
    // 상호작용 진행 여부를 반환한다.
    FORCEINLINE bool GetIsBusy() const { return bIsBusy; }

    // 상호작용 진행 상태를 설정한다.
    UFUNCTION(BlueprintCallable, Category="Interaction")
    FORCEINLINE void SetIsBusy(const bool bUsedCond) { bIsBusy = bUsedCond; }

    // BP 감지 콜리전을 연결하고 충돌 설정·겹침 이벤트·현재 감지를 갱신한다.
    UFUNCTION(BlueprintCallable, Category="Interaction")
    void SetDetectionCollision(UPrimitiveComponent* InCollision);

    // 연결된 유효한 감지 콜리전을 반환한다.
    UPrimitiveComponent* GetDetectionCollision() const;

    // 현재 겹친 플레이어를 확인하고 대상 액터의 등록·해제를 갱신한다.
    void RefreshDetectedInteractor();

protected:
    UPROPERTY(EditAnywhere, BlueprintGetter=GetInteractionTags, BlueprintSetter=SetInteractionTags,
        Category="Interaction|GameplayTags")
    FGameplayTagContainer HasTagContainer;

    // 진행 상태와 태그를 검사해 상호작용 가능 여부를 반환한다.
    FLMInteractionResult EvaluateInteraction(
        AActor* RequestActor, FGameplayTagContainer* ActionTags);

    // 종료 상태를 초기화하고 연결된 감지 콜리전을 설정한다.
    virtual void BeginPlay() override;
    // 종료 상태를 설정하고 감지 이벤트와 대상 등록을 정리한다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 겹침 이벤트를 해제하고 기록된 플레이어의 대상 목록에서 자신을 제거한다.
    void ClearDetection();

    FGuid InteractionId;
    bool bIsBusy = false;
    bool bIsEndingPlay = false;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> DetectionCollision;

    // 싱글 플레이 대상이므로 현재 감지한 플레이어 하나만 기록한다.
    TWeakObjectPtr<ULMInteractorComponent> DetectedInteractor;

    // 감지 콜리전의 겹침이 시작되면 플레이어 감지를 갱신한다.
    UFUNCTION()
    void OnDetectionBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    // 감지 콜리전의 겹침이 끝나면 남은 겹침을 기준으로 감지를 갱신한다.
    UFUNCTION()
    void OnDetectionEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);
};
