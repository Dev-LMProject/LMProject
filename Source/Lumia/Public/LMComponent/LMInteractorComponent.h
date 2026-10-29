#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LMInteractorComponent.generated.h"

class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLMInteractionTargetsChanged);

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class LUMIA_API ULMInteractorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** 겹침 이벤트로 갱신하므로 프레임마다 검사하지 않는다. */
    ULMInteractorComponent();

    /** HUD가 목록의 원본을 다시 조회하도록 실제 변경만 알린다. */
    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FLMInteractionTargetsChanged InteractionTargetsChanged;

    /** BP에서 만든 콜리전의 크기·배치를 유지하며 감지 설정과 현재 겹침을 연결한다. */
    UFUNCTION(BlueprintCallable, Category="Interaction")
    void SetInteractionCollision(UPrimitiveComponent* InCollision);

    /** BP에서 지정한 콜리전만 감지 기준으로 사용한다. */
    UFUNCTION(BlueprintPure, Category="Interaction")
    UPrimitiveComponent* GetInteractionCollision() const;

    /** 반복 Overlap이 같은 HUD 항목과 알림을 늘리지 않게 한다. */
    void RegisterInteractionTarget(AActor* TargetActor);
    /** 파괴 중인 타겟도 제거할 수 있도록 입력 유효성으로 조기 반환하지 않는다. */
    void UnregisterInteractionTarget(AActor* TargetActor);

    /** 내부 약한 참조를 노출하지 않고 유효한 액터의 복사본만 넘긴다. */
    UFUNCTION(BlueprintPure, Category="Interaction")
    TArray<AActor*> GetInteractionTargets() const;

protected:
    /** 다시 플레이에 진입한 인스턴스의 종료 상태를 해제한다. */
    virtual void BeginPlay() override;
    /** 종료 알림 중 재등록을 막고 HUD에 빈 목록을 전달한다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 이미 빈 목록에는 불필요한 HUD 갱신을 보내지 않는다. */
    void ClearInteractionTargets();

    TArray<TWeakObjectPtr<AActor>> InteractionTargets;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> InteractionCollision;

    bool bIsEndingPlay = false;
};
