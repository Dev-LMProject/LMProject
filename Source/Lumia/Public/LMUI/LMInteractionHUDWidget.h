#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMInteractionHUDWidget.generated.h"

class APawn;
class ULMInteractorComponent;
class UScrollBox;
class USizeBox;
class UTextBlock;

/** 기존 HUD 레이아웃에 감지 목록을 표시하고 사용자가 고른 액터를 보관한다. */
UCLASS()
class LUMIA_API ULMInteractionHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** BP 디자인을 그대로 사용해 애셋의 부모 클래스 변경 없이 동작을 연결한다. */
    ULMInteractionHUDWidget(const FObjectInitializer& ObjectInitializer);

    /** 휠과 방향키가 같은 순서로 목록을 순환하게 한다. */
    void MoveSelection(int32 Direction);

    /** 선택된 액터만 전달하며 감지 범위 이탈 여부는 Interactor가 다시 확인한다. */
    FLMInteractionResult TryInteractSelected(FGameplayTagContainer ActionTags);

    UFUNCTION(BlueprintPure, Category="Interaction")
    AActor* GetSelectedActor() const;

protected:
    /** 기존 두 위젯의 디자인을 런타임에 조합한다. */
    virtual void NativeOnInitialized() override;
    /** 늦게 만들어진 HUD도 현재 감지 목록을 한 번 받아 시작한다. */
    virtual void NativeConstruct() override;
    /** 제거된 HUD가 목록 변경을 계속 받지 않도록 구독을 해제한다. */
    virtual void NativeDestruct() override;

private:
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

    /** 플레이어 교체 시 이전 Interactor의 목록과 선택을 남기지 않는다. */
    void BindInteractor(APawn* Pawn);

    /** 목록 변경 때만 행을 갱신하고 사라진 대상의 선택을 해제한다. */
    UFUNCTION()
    void RefreshTargets();

    /** 선택 이동에는 행을 다시 만들지 않고 강조와 스크롤만 바꾼다. */
    void UpdateSelectionDisplay();

    UPROPERTY()
    TSubclassOf<UUserWidget> HUDLayoutClass;

    UPROPERTY()
    TSubclassOf<UUserWidget> ListLayoutClass;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ListLayout;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> ListSizeBox;

    UPROPERTY(Transient)
    TObjectPtr<UScrollBox> TargetList;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> TargetLabels;

    TWeakObjectPtr<ULMInteractorComponent> Interactor;
    TArray<TWeakObjectPtr<AActor>> DisplayedTargets;
    TWeakObjectPtr<AActor> SelectedActor;
};
