#include "LMUI/LMUserHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "LMUI/LMInteractionHUDWidget.h"
#include "UObject/ConstructorHelpers.h"

ULMUserHUDWidget::ULMUserHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<UUserWidget> HUDLayoutAsset(
        TEXT("/Game/UI/WBP_UserHUD"));
    HUDLayoutClass = HUDLayoutAsset.Class;
}

void ULMUserHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!ensureMsgf(HUDLayoutClass, TEXT("User HUD requires WBP_UserHUD.")))
    {
        return;
    }

    UUserWidget* HUDLayout = CreateWidget<UUserWidget>(this, HUDLayoutClass);
    if (!IsValid(HUDLayout))
    {
        return;
    }

    UOverlay* HUDRoot = Cast<UOverlay>(HUDLayout->GetRootWidget());
    if (!ensureMsgf(HUDRoot, TEXT("User HUD expects an Overlay root.")))
    {
        return;
    }

    WidgetTree->RootWidget = HUDLayout;
    SetVisibility(ESlateVisibility::HitTestInvisible);

    InteractionHUD = CreateWidget<ULMInteractionHUDWidget>(this, ULMInteractionHUDWidget::StaticClass());
    if (!IsValid(InteractionHUD))
    {
        return;
    }

    UOverlaySlot* InteractionSlot = HUDRoot->AddChildToOverlay(InteractionHUD);
    InteractionSlot->SetHorizontalAlignment(HAlign_Right);
    InteractionSlot->SetVerticalAlignment(VAlign_Center);
    InteractionSlot->SetPadding(FMargin(0.0f, 0.0f, 40.0f, 0.0f));
}
