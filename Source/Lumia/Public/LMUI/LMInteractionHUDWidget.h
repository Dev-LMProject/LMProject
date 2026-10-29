#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMInteractionHUDWidget.generated.h"

class APawn;
class ULMInteractorComponent;
class UScrollBox;
class UTextBlock;

// 자신의 상호작용 목록을 표시하고 사용자가 고른 액터를 보관한다.
UCLASS()
class LUMIA_API ULMInteractionHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 상호작용 목록 BP 위젯 클래스를 불러온다.
    ULMInteractionHUDWidget(const FObjectInitializer& ObjectInitializer);

    // 표시 목록에서 이전 또는 다음 액터를 순환 선택한다.
    void MoveSelection(int32 Direction);

    // 현재 선택 액터를 Interactor에 전달해 상호작용을 요청한다.
    FLMInteractionResult TryInteractSelected();

    // 현재 선택된 유효한 액터를 반환한다.
    UFUNCTION(BlueprintPure, Category="Interaction")
    AActor* GetSelectedActor() const;

protected:
    // 목록 레이아웃을 생성하고 스크롤·초기 표시 상태를 설정한다.
    virtual void NativeOnInitialized() override;
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

    // 감지 후보의 태그를 검사해 HUD 행을 갱신하고 숨겨진 대상의 선택을 해제한다.
    UFUNCTION()
    void RefreshTargets();

    // 선택한 행의 표시와 색상을 갱신하고 해당 행으로 스크롤한다.
    void UpdateSelectionDisplay();

    UPROPERTY()
    TSubclassOf<UUserWidget> ListLayoutClass;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ListLayout;

    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> TargetList;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> TargetLabels;

    TWeakObjectPtr<ULMInteractorComponent> Interactor;
    // 숨겨진 후보의 태그 변경도 받아 재진입 없이 다시 표시한다.
    TArray<TWeakObjectPtr<ULMInteractionComponent>> ObservedInteractions;
    TArray<TWeakObjectPtr<AActor>> DisplayedTargets;
    TWeakObjectPtr<AActor> SelectedActor;
};
