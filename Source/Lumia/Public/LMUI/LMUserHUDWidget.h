#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMUserHUDWidget.generated.h"

class ULMInteractionHUDWidget;

// 전체 HUD의 레이아웃과 자식 위젯 배치는 부모 HUD에서 관리한다.
UCLASS()
class LUMIA_API ULMUserHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 부모 UserHUD의 BP 레이아웃 클래스를 불러온다.
    ULMUserHUDWidget(const FObjectInitializer& ObjectInitializer);

    // 생성된 자식 상호작용 HUD를 반환한다.
    ULMInteractionHUDWidget* GetInteractionHUD() const { return InteractionHUD; }

protected:
    // UserHUD 레이아웃과 자식 상호작용 HUD를 생성하고 화면 위치를 설정한다.
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY()
    TSubclassOf<UUserWidget> HUDLayoutClass;

    UPROPERTY(Transient)
    TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;
};
