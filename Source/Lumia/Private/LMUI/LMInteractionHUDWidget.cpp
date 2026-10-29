#include "LMUI/LMInteractionHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LMComponent/LMInteractorComponent.h"
#include "UObject/ConstructorHelpers.h"

ULMInteractionHUDWidget::ULMInteractionHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<UUserWidget> HUDLayoutAsset(
        TEXT("/Game/UI/WBP_UserHUD"));
    static ConstructorHelpers::FClassFinder<UUserWidget> ListLayoutAsset(
        TEXT("/Game/UI/WBP_InteractionList"));
    HUDLayoutClass = HUDLayoutAsset.Class;
    ListLayoutClass = ListLayoutAsset.Class;
}

void ULMInteractionHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!ensureMsgf(HUDLayoutClass && ListLayoutClass,
        TEXT("Interaction HUD requires WBP_UserHUD and WBP_InteractionList.")))
    {
        return;
    }

    UUserWidget* HUDLayout = CreateWidget<UUserWidget>(this, HUDLayoutClass);
    ListLayout = CreateWidget<UUserWidget>(this, ListLayoutClass);
    if (!IsValid(HUDLayout) || !IsValid(ListLayout))
    {
        return;
    }

    UOverlay* HUDRoot = Cast<UOverlay>(HUDLayout->GetRootWidget());
    ListSizeBox = Cast<USizeBox>(ListLayout->GetRootWidget());
    TargetList = Cast<UScrollBox>(ListLayout->GetWidgetFromName(TEXT("ScrollBox_101")));
    if (!ensureMsgf(HUDRoot && ListSizeBox && TargetList,
        TEXT("Interaction HUD expects an Overlay root and a SizeBox containing ScrollBox_101.")))
    {
        return;
    }

    // 레이아웃은 기존 BP를 사용하고, 입력은 게임 컨트롤러가 받는다.
    WidgetTree->RootWidget = HUDLayout;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ListSizeBox->SetWidthOverride(340.0f);
    TargetList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
    TargetList->SetConsumeMouseWheel(EConsumeMouseWheel::Never);

    UOverlaySlot* ListSlot = HUDRoot->AddChildToOverlay(ListLayout);
    ListSlot->SetHorizontalAlignment(HAlign_Right);
    ListSlot->SetVerticalAlignment(VAlign_Center);
    ListSlot->SetPadding(FMargin(0.0f, 0.0f, 40.0f, 0.0f));
    ListLayout->SetVisibility(ESlateVisibility::Collapsed);
}

void ULMInteractionHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(
            this, &ThisClass::HandlePossessedPawnChanged);
        BindInteractor(PlayerController->GetPawn());
    }
}

void ULMInteractionHUDWidget::NativeDestruct()
{
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->OnPossessedPawnChanged.RemoveDynamic(
            this, &ThisClass::HandlePossessedPawnChanged);
    }
    BindInteractor(nullptr);
    Super::NativeDestruct();
}

void ULMInteractionHUDWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    BindInteractor(NewPawn);
}

void ULMInteractionHUDWidget::BindInteractor(APawn* Pawn)
{
    if (ULMInteractorComponent* PreviousInteractor = Interactor.Get())
    {
        PreviousInteractor->InteractionTargetsChanged.RemoveDynamic(
            this, &ThisClass::RefreshTargets);
    }

    Interactor = IsValid(Pawn) ? Pawn->FindComponentByClass<ULMInteractorComponent>() : nullptr;
    SelectedActor.Reset();
    if (ULMInteractorComponent* CurrentInteractor = Interactor.Get())
    {
        CurrentInteractor->InteractionTargetsChanged.AddUniqueDynamic(
            this, &ThisClass::RefreshTargets);
    }
    RefreshTargets();
}

void ULMInteractionHUDWidget::RefreshTargets()
{
    for (const TWeakObjectPtr<ULMInteractionComponent>& Observed : ObservedInteractions)
    {
        if (ULMInteractionComponent* Interaction = Observed.Get())
        {
            Interaction->InteractionTagsChanged.RemoveAll(this);
        }
    }
    ObservedInteractions.Reset();

    DisplayedTargets.Reset();
    if (ULMInteractorComponent* CurrentInteractor = Interactor.Get())
    {
        for (AActor* Target : CurrentInteractor->GetInteractionTargets())
        {
            ULMInteractionComponent* Interaction = Target->FindComponentByClass<ULMInteractionComponent>();
            if (!IsValid(Interaction))
            {
                continue;
            }

            ObservedInteractions.Add(TWeakObjectPtr<ULMInteractionComponent>(Interaction));
            Interaction->InteractionTagsChanged.AddUObject(this, &ThisClass::RefreshTargets);
            if (Interaction->MatchesInteractionTags(InteractionActionTags))
            {
                DisplayedTargets.Add(TWeakObjectPtr<AActor>(Target));
            }
        }
    }

    if (!DisplayedTargets.Contains(SelectedActor))
    {
        SelectedActor.Reset();
    }

    if (!IsValid(TargetList) || !IsValid(ListLayout) || !IsValid(ListSizeBox))
    {
        return;
    }

    TargetList->ClearChildren();
    TargetLabels.Reset();
    ListLayout->SetVisibility(DisplayedTargets.IsEmpty()
        ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (DisplayedTargets.IsEmpty())
    {
        return;
    }

    ListSizeBox->SetHeightOverride(FMath::Min(360.0f, 84.0f + 32.0f * DisplayedTargets.Num()));

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(NSLOCTEXT("LMInteractionHUD", "Title", "상호작용"));
    FSlateFontInfo TitleFont = Title->GetFont();
    TitleFont.Size = 20;
    Title->SetFont(TitleFont);
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(FColor::FromHex(TEXT("FFAA4C")))));
    TargetList->AddChild(Title);

    UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
    Hint->SetText(NSLOCTEXT("LMInteractionHUD", "Controls", "휠 / ↑↓ 선택 · E 상호작용"));
    FSlateFontInfo HintFont = Hint->GetFont();
    HintFont.Size = 13;
    Hint->SetFont(HintFont);
    Hint->SetColorAndOpacity(FSlateColor(FLinearColor(FColor::FromHex(TEXT("8CD9F3")))));
    TargetList->AddChild(Hint);

    for (int32 Index = 0; Index < DisplayedTargets.Num(); ++Index)
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo LabelFont = Label->GetFont();
        LabelFont.Size = 18;
        Label->SetFont(LabelFont);
        TargetLabels.Add(Label);
        if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(TargetList->AddChild(Label)))
        {
            RowSlot->SetPadding(FMargin(8.0f, 4.0f));
        }
    }
    UpdateSelectionDisplay();
}

void ULMInteractionHUDWidget::MoveSelection(int32 Direction)
{
    if (DisplayedTargets.IsEmpty() || Direction == 0)
    {
        return;
    }

    const int32 Step = Direction > 0 ? 1 : -1;
    const int32 PreviousIndex = DisplayedTargets.IndexOfByKey(SelectedActor);
    const int32 NextIndex = PreviousIndex == INDEX_NONE
        ? (Step > 0 ? 0 : DisplayedTargets.Num() - 1)
        : (PreviousIndex + Step + DisplayedTargets.Num()) % DisplayedTargets.Num();
    SelectedActor = DisplayedTargets[NextIndex];
    UpdateSelectionDisplay();
}

void ULMInteractionHUDWidget::UpdateSelectionDisplay()
{
    for (int32 Index = 0; Index < TargetLabels.Num(); ++Index)
    {
        AActor* Target = DisplayedTargets[Index].Get();
        const bool bSelected = Target && Target == SelectedActor.Get();
        TargetLabels[Index]->SetText(FText::FromString(FString::Printf(
            TEXT("%s%s"), bSelected ? TEXT("▶ ") : TEXT("   "),
            Target ? *Target->GetActorNameOrLabel() : TEXT(""))));
        TargetLabels[Index]->SetColorAndOpacity(FSlateColor(bSelected
            ? FLinearColor(FColor::FromHex(TEXT("FFAA4C"))) : FLinearColor::White));

        if (bSelected)
        {
            TargetList->ScrollWidgetIntoView(TargetLabels[Index], false);
        }
    }
}

AActor* ULMInteractionHUDWidget::GetSelectedActor() const
{
    return SelectedActor.Get();
}

void ULMInteractionHUDWidget::SetInteractionActionTags(FGameplayTagContainer InTags)
{
    if (InteractionActionTags != InTags)
    {
        InteractionActionTags = MoveTemp(InTags);
        RefreshTargets();
    }
}

FLMInteractionResult ULMInteractionHUDWidget::TryInteractSelected()
{
    if (ULMInteractorComponent* CurrentInteractor = Interactor.Get())
    {
        return CurrentInteractor->TryInteract(SelectedActor.Get(), InteractionActionTags);
    }

    FLMInteractionResult Result;
    Result.Failure = InteractionFailure::CantAccessActor;
    return Result;
}
