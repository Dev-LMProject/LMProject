#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMUserHUDWidget.generated.h"

class ULMInteractionHUDWidget;

/** 전체 HUD의 레이아웃과 자식 위젯 배치는 부모 HUD에서 관리한다. */
UCLASS()
class LUMIA_API ULMUserHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 기존 UserHUD BP의 디자인을 사용해 애셋 변경 없이 조합한다. */
    ULMUserHUDWidget(const FObjectInitializer& ObjectInitializer);

    /** 컨트롤러가 생성된 자식에게 입력을 전달할 수 있도록 제공한다. */
    ULMInteractionHUDWidget* GetInteractionHUD() const { return InteractionHUD; }

protected:
    /** 자식은 부모 구조를 몰라도 되도록 여기서 생성하고 배치한다. */
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY()
    TSubclassOf<UUserWidget> HUDLayoutClass;

    UPROPERTY(Transient)
    TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;
};
