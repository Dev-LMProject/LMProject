#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMInteractorComponent.generated.h"

class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLMInteractionTargetsChanged);
DECLARE_MULTICAST_DELEGATE(FLMInteractionActionTagsChanged);

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LUMIA_API ULMInteractorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Interactor 컴포넌트의 Tick을 비활성화한다.
    ULMInteractorComponent();

    // HUD가 목록의 원본을 다시 조회하도록 실제 변경만 알린다.
    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FLMInteractionTargetsChanged InteractionTargetsChanged;

    // 요청 태그 변경을 알리고 HUD가 표시 대상을 다시 판정하게 한다.
    FLMInteractionActionTagsChanged InteractionActionTagsChanged;

    // 상호작용 요청 태그의 복사본을 반환한다.
    UFUNCTION(BlueprintGetter, Category="Interaction")
    FGameplayTagContainer GetInteractionActionTags() const { return InteractionActionTags; }

    // 요청 태그를 변경하고 태그 변경 델리게이트를 발행한다.
    UFUNCTION(BlueprintSetter, Category="Interaction")
    void SetInteractionActionTags(FGameplayTagContainer InTags);

    // BP의 플레이어 콜리전을 연결하고 충돌 설정과 현재 감지 대상을 갱신한다.
    UFUNCTION(BlueprintCallable, Category="Interaction")
    void SetInteractionCollision(UPrimitiveComponent* InCollision);

    // 연결된 유효한 플레이어 콜리전을 반환한다.
    UFUNCTION(BlueprintPure, Category="Interaction")
    UPrimitiveComponent* GetInteractionCollision() const;

    // 유효한 상호작용 액터를 중복 없이 등록하고 목록 변경을 알린다.
    void RegisterInteractionTarget(AActor* TargetActor);
    // 지정한 액터와 무효한 항목을 제거하고 목록 변경을 알린다.
    void UnregisterInteractionTarget(AActor* TargetActor);

    // 현재 등록된 유효한 액터 목록을 반환한다.
    UFUNCTION(BlueprintPure, Category="Interaction")
    TArray<AActor*> GetInteractionTargets() const;

    // 선택 액터의 유효성과 목록 포함 여부를 검사하고 보유한 태그로 상호작용을 요청한다.
    UFUNCTION(BlueprintCallable, Category="Interaction")
    FLMInteractionResult TryInteract(AActor* TargetActor);

protected:
    // Interactor의 종료 상태를 초기화한다.
    virtual void BeginPlay() override;
    // 종료 상태를 설정하고 콜리전 참조와 대상 목록을 비운다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 대상 목록을 비우고 목록이 변경되면 델리게이트를 발행한다.
    void ClearInteractionTargets();

    TArray<TWeakObjectPtr<AActor>> InteractionTargets;

    UPROPERTY(EditDefaultsOnly, BlueprintGetter=GetInteractionActionTags,
        BlueprintSetter=SetInteractionActionTags, Category="Interaction", meta=(AllowPrivateAccess="true"))
    FGameplayTagContainer InteractionActionTags;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> InteractionCollision;

    bool bIsEndingPlay = false;
};
