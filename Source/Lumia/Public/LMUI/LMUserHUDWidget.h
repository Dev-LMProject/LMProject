#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMUserHUDWidget.generated.h"

class AActor;
class ULMInteractionHUDWidget;
class ALMPlayerState;
class APawn;

// 기존 상호작용 목록을 유지하면서 선택 대상의 화면 표식도 표시한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMUserHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Designer에서 InteractionHUD라는 이름으로 배치한 기존 자식 위젯을 반환한다.
    ULMInteractionHUDWidget* GetInteractionHUD() const;

    // 표적점의 화면 위치와 고정 색상을 갱신하며 대상이 없으면 표식을 숨긴다.
    void UpdateFocusMarker(AActor* FocusTarget, bool bLocked = false);

    // 현재 스냅샷을 표시하며 새 외형 위젯을 연결한 직후에도 사용할 수 있다.
    UFUNCTION(BlueprintCallable, Category = "LM|Special")
    void RefreshSpecialGauge();

protected:
    // 기존 HUD 생성이 끝나면 게이지 변경 및 플레이어 소유 알림을 연결한다.
    virtual void NativeConstruct() override;

    // HUD 재생성 때 같은 알림이 중복 등록되지 않도록 정리한다.
    virtual void NativeDestruct() override;

    // BP는 표시 허용 이후 위젯을 만들고 외형 인터페이스로 네 표시값을 전달한다.
    UFUNCTION(BlueprintImplementableEvent, Category = "LM|Special")
    void UpdateSpecialGauge(bool bShowGauge, float ChargeRatio, bool bUnlocked, bool bFull, bool bCanPrepare);

private:
    // 현재 플레이어 상태로 구독을 바꾸고 첫 표시값을 즉시 전달한다.
    void BindSpecialGauge();

    // 이전 플레이어 상태의 구독만 제거한다.
    void UnbindSpecialGauge();

    // 소유 캐릭터가 준비된 경우에도 초기 표시 연결을 놓치지 않는다.
    UFUNCTION()
    void HandleGaugePawnChanged(APawn* OldPawn, APawn* NewPawn);

    TWeakObjectPtr<ALMPlayerState> GaugePlayerState;
    FDelegateHandle GaugeChangedHandle;

    // 기존 상호작용 HUD 연결이므로 이름과 타입을 유지한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;

    // Designer에서 이름이 FocusMarker인 자식 UserWidget이 이 변수에 연결된다.
    // Optional이므로 아직 표식을 배치하지 않은 기존 HUD도 사용할 수 있다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UUserWidget> FocusMarker;
};
