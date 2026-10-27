#include "LMCamera/LMTargetQueryLibrary.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "UnrealClient.h"

bool ULMTargetQueryLibrary::IsPointInCameraView(APlayerController* PlayerController,
	FVector WorldPosition, FVector2D& OutScreenPosition)
{
	OutScreenPosition = FVector2D::ZeroVector;
	if (!IsValid(PlayerController) || WorldPosition.ContainsNaN())
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
	{
		return false;
	}

	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		return false;
	}

	// 분할 화면과 레터박스를 제외한 해당 플레이어의 실제 표시 영역을 사용한다.
	const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
	FVector2D ScreenPosition;
	if (ViewRect.IsEmpty() || !FSceneView::ProjectWorldToScreen(WorldPosition, ViewRect,
		ProjectionData.ComputeViewProjectionMatrix(), ScreenPosition) || ScreenPosition.ContainsNaN())
	{
		return false;
	}

	OutScreenPosition = ScreenPosition - FVector2D(ViewRect.Min);
	return OutScreenPosition.X >= 0.0 && OutScreenPosition.X < ViewRect.Width() &&
		OutScreenPosition.Y >= 0.0 && OutScreenPosition.Y < ViewRect.Height();
}
