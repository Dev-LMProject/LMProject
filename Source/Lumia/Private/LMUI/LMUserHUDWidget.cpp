#include "LMUI/LMUserHUDWidget.h"
// 화면 픽셀을 위젯 부모의 로컬 좌표로 바꾸는 함수가 선언되어 있다.
#include "Blueprint/SlateBlueprintLibrary.h"
// Canvas 안에서 표식의 위치를 바꾸는 UCanvasPanelSlot의 정의다.
#include "Components/CanvasPanelSlot.h"
// 부모 패널의 Geometry를 읽을 때 필요한 UPanelWidget의 정의다.
#include "Components/PanelWidget.h"
// 월드 위치를 화면 픽셀로 투영하는 PlayerController 함수를 사용한다.
#include "GameFramework/PlayerController.h"
// 표식이 따라갈 대상의 표적점을 구한다.
#include "LMCamera/LMTargetQueryLibrary.h"
#include "LMUI/LMInteractionHUDWidget.h"

// 기존 Controller가 상호작용 목록 위젯을 가져갈 수 있게 반환한다.
ULMInteractionHUDWidget* ULMUserHUDWidget::GetInteractionHUD() const
{
	return InteractionHUD.Get();
}

// 선택 대상의 표적점을 화면에 투영하고 표식의 Canvas 위치를 변경한다.
void ULMUserHUDWidget::UpdateFocusMarker(AActor* FocusTarget)
{
	// Designer에 FocusMarker가 없으면 움직일 표식 자체가 없다.
	if (!FocusMarker) return;

	// 선택이 없어졌거나 대상이 파괴 중이면 이전 위치의 표식을 숨긴다.
	if (!IsValid(FocusTarget) || FocusTarget->IsActorBeingDestroyed())
	{
		FocusMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 이 UserHUD를 생성할 때 소유자로 지정한 PlayerController를 가져온다.
	APlayerController* Controller = GetOwningPlayer();

	// Parent는 표식을 직접 담고 있는 패널이다.
	// Slot은 그 패널 안에서 표식이 놓일 위치와 크기를 보관한다.
	UPanelWidget* Parent = FocusMarker->GetParent();
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(FocusMarker->Slot);

	// CanvasPanel의 직접 자식이 아니면 Canvas 위치를 설정할 수 없다.
	if (!Controller || !Parent || !CanvasSlot)
	{
		FocusMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector2D ParentSize = Parent->GetCachedGeometry().GetLocalSize();

	// 위젯을 막 생성한 첫 프레임에는 부모의 실제 화면 크기가 아직 없을 수 있다.
	// 이때 좌표를 계산하면 표식이 잘못된 위치에 나타날 수 있어 다음 갱신까지 숨긴다.
	if (ParentSize.X <= 0.0 || ParentSize.Y <= 0.0)
	{
		FocusMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector WorldPoint = ULMTargetQueryLibrary::GetFocusPoint(FocusTarget);
	FVector2D PixelPosition;

	// 월드의 표적점이 게임 뷰포트의 어느 픽셀에 보이는지 계산한다.
	// false이므로 플레이어 표시 영역의 시작 위치를 빼지 않은 뷰포트 기준 좌표를 받는다.
	if (!Controller->ProjectWorldLocationToScreen(WorldPoint, PixelPosition, false))
	{
		FocusMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FVector2D LocalPosition;

	// 화면 픽셀과 Canvas 좌표는 DPI·부모 위치·배율 때문에 숫자가 다를 수 있다.
	// 실제 부모 Geometry를 사용해 Canvas가 이해하는 좌표로 변환한다.
	// 마지막 false는 입력 좌표가 데스크톱 좌표가 아닌 게임 뷰포트 기준이기 때문이다.
	USlateBlueprintLibrary::ScreenToWidgetLocal(this, Parent->GetCachedGeometry(), PixelPosition, LocalPosition, false);

	if (LocalPosition.ContainsNaN())
	{
		FocusMarker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 아래 에디터 설정에서 앵커는 좌상단, Alignment는 중앙으로 맞춘다.
	// 그러면 이 위치에 표식의 중심이 놓인다.
	CanvasSlot->SetPosition(LocalPosition);

	// 표식은 보이되 자신과 자식 위젯이 마우스 입력을 가로채지 않게 한다.
	FocusMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
}