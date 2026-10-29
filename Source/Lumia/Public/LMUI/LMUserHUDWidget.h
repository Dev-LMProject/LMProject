#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMUserHUDWidget.generated.h"

class ULMInteractionHUDWidget;

// BP 디자이너에 배치한 상호작용 HUD를 컨트롤러에 제공한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMUserHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // BP 디자이너에 배치된 상호작용 HUD를 반환한다.
    ULMInteractionHUDWidget* GetInteractionHUD() const;

private:
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;
};
