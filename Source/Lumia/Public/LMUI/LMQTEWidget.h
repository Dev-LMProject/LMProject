#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMQTE/LMQTETypes.h"
#include "LMQTEWidget.generated.h"

class ULMQTEComponent;

// 표현은 BP가, 포인터 좌표 전달과 요청의 수명은 공통 위젯이 담당한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMQTEWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 하나의 요청에만 화면을 연결하고 BP가 머티리얼과 안내를 준비하게 한다.
	void InitializeQTE(ULMQTEComponent* _QTEOwner, int32 _QTERequestId, const FLMQTEConfig& Config);

	// 판정기가 계산한 비율을 그대로 전달하여 UI와 판정 시간이 어긋나지 않게 한다.
	void UpdateQTE(float RemainingRatio, float InputProgress);

	// 포인터 캡처와 요청 연결을 먼저 끊고 화면 종료 연출을 알린다.
	void ClearQTE();

protected:
	// 안내 문구와 동적 UI 머티리얼을 생성한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|QTE")
	void OnQTEStarted(const FLMQTEConfig& Config);

	// RemainingRatio를 머티리얼의 TimeLeft에, InputProgress를 진행 표시에 사용한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|QTE")
	void OnQTEUpdated(float RemainingRatio, float InputProgress);

	// 요청 종료 시 BP가 보관한 표현 참조를 정리한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|QTE")
	void OnQTEClosed();

	// 드래그를 시작할 때만 포인터를 캡처한다.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 표시 영역의 정규화 좌표를 판정기에 전달한다.
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 마지막 위치를 전달하고 이번 드래그를 끝낸다.
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 예상하지 못한 캡처 상실은 성공으로 해석하지 않는다.
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	// 화면이 외부에서 제거됐을 때 남은 요청을 취소한다.
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DragArea;

private:
	// 화면 크기와 DPI에 관계없이 DragArea 기준 0~1 좌표로 변환한다.
	bool ReadDragPoint(const FPointerEvent& PointerEvent, FVector2D& NormalizedPos) const;

	TWeakObjectPtr<ULMQTEComponent> QTEOwner;
	FGameplayTag DragInputTag;
	int32 QTERequestId = 0;
	bool bDragMode = false;
	bool bDragging = false;
};
