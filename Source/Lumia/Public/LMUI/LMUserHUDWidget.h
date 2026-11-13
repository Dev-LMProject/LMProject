#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMUserHUDWidget.generated.h"

class AActor;
class ULMInteractionHUDWidget;

// 기존 상호작용 목록을 유지하면서 선택 대상의 화면 표식도 표시한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMUserHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Designer에서 InteractionHUD라는 이름으로 배치한 기존 자식 위젯을 반환한다.
    ULMInteractionHUDWidget* GetInteractionHUD() const;

    // 대상의 표적점을 화면 위치로 바꾸고 FocusMarker의 Canvas 위치를 변경한다.
    // 대상이 nullptr이면 표식을 숨긴다.
    void UpdateFocusMarker(AActor* FocusTarget);

private:
    // 기존 상호작용 HUD 연결이므로 이름과 타입을 유지한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;

    // Designer에서 이름이 FocusMarker인 자식 UserWidget이 이 변수에 연결된다.
    // Optional이므로 아직 표식을 배치하지 않은 기존 HUD도 사용할 수 있다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UUserWidget> FocusMarker;
};