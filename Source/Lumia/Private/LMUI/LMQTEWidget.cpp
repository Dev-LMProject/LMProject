#include "LMUI/LMQTEWidget.h"
#include "LMQTE/LMQTEComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "Components/Widget.h"
#include "InputCoreTypes.h"

// 이번 요청과 표시만 연결하고 판정 상태를 복제하지 않는다.
void ULMQTEWidget::InitializeQTE(ULMQTEComponent* _QTEOwner, int32 _QTERequestId, const FLMQTEConfig& Config)
{
	QTEOwner = _QTEOwner;
	QTERequestId = _QTERequestId;
	DragInputTag = Config.InputTag;
	bDragMode = Config.InteractionTag == LMGameplayTags::QTE_Interaction_Drag;
	bDragging = false;
	SetVisibility(bDragMode ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	OnQTEStarted(Config);
}

// 별도의 타이머 없이 실제 판정기의 진행 비율을 사용한다.
void ULMQTEWidget::UpdateQTE(float RemainingRatio, float InputProgress)
{
	OnQTEUpdated(RemainingRatio, InputProgress);
}

// 제거 콜백이 같은 요청을 다시 취소하지 않도록 참조부터 비운다.
void ULMQTEWidget::ClearQTE()
{
	bDragging = false;
	bDragMode = false;
	QTEOwner.Reset();
	DragInputTag = FGameplayTag();
	QTERequestId = 0;
	OnQTEClosed();
}

// 전체 화면이 아닌 실제 드래그 안내 영역을 좌표 기준으로 삼는다.
bool ULMQTEWidget::ReadDragPoint(const FPointerEvent& PointerEvent, FVector2D& NormalizedPos) const
{
	if (!IsValid(DragArea)) return false;
	const FGeometry& DragGeometry = DragArea->GetCachedGeometry();
	const FVector2D DragSize = DragGeometry.GetLocalSize();
	if (DragSize.X <= 0.0 || DragSize.Y <= 0.0) return false;
	NormalizedPos = DragGeometry.AbsoluteToLocal(PointerEvent.GetScreenSpacePosition()) / DragSize;
	return FMath::IsFinite(NormalizedPos.X) && FMath::IsFinite(NormalizedPos.Y);
}

// 시작 영역을 누른 경우에만 후속 포인터 이동을 확보한다.
FReply ULMQTEWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D NormalizedPos = FVector2D::ZeroVector;
	ULMQTEComponent* ActiveQTE = QTEOwner.Get();
	if (bDragMode && ActiveQTE && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && ReadDragPoint(InMouseEvent, NormalizedPos))
	{
		const int32 StartingId = QTERequestId;
		ActiveQTE->BeginDrag(StartingId, NormalizedPos);
		// 잘못 잡은 경우도 해제를 받아 다음 새 드래그 시도를 허용한다.
		if (QTEOwner.Get() == ActiveQTE && QTERequestId == StartingId && ActiveQTE->GetActiveRequestId() == StartingId)
		{
			bDragging = true;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// 드래그 중인 요청에만 새 위치를 전달한다.
FReply ULMQTEWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		FVector2D NormalizedPos = FVector2D::ZeroVector;
		if (ULMQTEComponent* ActiveQTE = QTEOwner.Get(); ActiveQTE && ReadDragPoint(InMouseEvent, NormalizedPos))
			ActiveQTE->MoveDrag(QTERequestId, NormalizedPos);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// 정상 해제는 캡처 상실 취소와 구분한다.
FReply ULMQTEWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragMode && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		FVector2D NormalizedPos = FVector2D::ZeroVector;
		if (ULMQTEComponent* ActiveQTE = QTEOwner.Get())
		{
			const int32 ReleasedId = QTERequestId;
			// 마지막 이동이 성공하여 위젯을 제거하더라도 실제 해제 기록은 남긴다.
			ActiveQTE->SubmitButtonRelease(DragInputTag);
			if (ReadDragPoint(InMouseEvent, NormalizedPos))
			{
				ActiveQTE->MoveDrag(ReleasedId, NormalizedPos);
				ActiveQTE->EndDrag(ReleasedId);
			}
			else
				ActiveQTE->CancelQTE(ReleasedId);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

// 창 전환 등으로 포인터 추적이 끊기면 성공을 만들지 않고 종료한다.
void ULMQTEWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	const bool bWasDragging = bDragging;
	bDragging = false;
	if (ULMQTEComponent* ActiveQTE = QTEOwner.Get(); bWasDragging && ActiveQTE)
		ActiveQTE->CancelQTE(QTERequestId);
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

// 외부 제거와 판정기 자체 종료가 서로 재진입하지 않게 연결을 먼저 끊는다.
void ULMQTEWidget::NativeDestruct()
{
	ULMQTEComponent* ClosingQTE = QTEOwner.Get();
	const int32 ClosingId = QTERequestId;
	QTEOwner.Reset();
	QTERequestId = 0;
	bDragging = false;
	if (ClosingQTE)
		ClosingQTE->CancelQTE(ClosingId);
	Super::NativeDestruct();
}
