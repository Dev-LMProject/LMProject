#include "LMUI/LMUserHUDWidget.h"

#include "LMUI/LMInteractionHUDWidget.h"

ULMInteractionHUDWidget* ULMUserHUDWidget::GetInteractionHUD() const
{
    return InteractionHUD.Get();
}
