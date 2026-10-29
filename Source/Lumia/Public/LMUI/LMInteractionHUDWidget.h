#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMInteractionHUDWidget.generated.h"

class APawn;
class ULMInteractorComponent;

// 상호작용 표시 목록과 선택을 관리하고 BP에 변경을 알린다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMInteractionHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 표시 목록에서 이전 또는 다음 액터를 선택한다.
    void MoveSelection(int32 Direction);

    // 현재 선택 액터를 Interactor에 전달해 상호작용을 요청한다.
    FLMInteractionResult TryInteractSelected();

    // 현재 선택된 유효한 액터를 반환한다.
    UFUNCTION(BlueprintPure, Category="Interaction")
    AActor* GetSelectedActor() const;

protected:
    // 표시 대상 목록이 바뀌면 BP가 목록 행을 갱신하도록 알린다.
    UFUNCTION(BlueprintImplementableEvent, Category="Interaction")
    void OnInteractionTargetsChanged(const TArray<AActor*>& Targets);

    // 현재 선택 액터를 BP에 전달해 선택 표시를 갱신한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Interaction")
    void OnInteractionSelectionChanged(AActor* TargetActor);

    // Pawn 변경 이벤트를 구독하고 현재 Pawn의 Interactor에 연결한다.
    virtual void NativeConstruct() override;
    // Pawn·목록·태그 변경 구독을 해제하고 표시 목록과 선택을 비운다.
    virtual void NativeDestruct() override;

private:
    // 소유 Pawn이 변경되면 새 Pawn의 Interactor에 연결한다.
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

    // 이전 목록·요청 태그 구독을 해제하고 새 Interactor를 구독한 뒤 목록을 갱신한다.
    void BindInteractor(APawn* Pawn);

    // 감지 후보의 태그를 검사하고 표시 목록과 선택 변경을 BP에 알린다.
    UFUNCTION()
    void RefreshTargets();

    TWeakObjectPtr<ULMInteractorComponent> Interactor;
    // 숨겨진 후보의 태그 변경도 받아 재진입 없이 다시 표시한다.
    TArray<TWeakObjectPtr<ULMInteractionComponent>> ObservedInteractions;
    TArray<TWeakObjectPtr<AActor>> DisplayedTargets;
    TWeakObjectPtr<AActor> SelectedActor;
};
