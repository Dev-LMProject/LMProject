#include "LMUI/LMCubePanel.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "LMPlayer/LMPlayerController.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

// 현재 퍼즐의 촬영 결과를 연결하고 입력을 받을 준비를 한다.
void ULMCubePanel::OpenFor(ALMCubeRoomPuzzle* _Puzzle, UWidget* _GameHUD)
{
    Puzzle = _Puzzle;
    GameHUD = _GameHUD;
    SetIsFocusable(true);

    // 위젯마다 머티리얼을 따로 만들어 다른 화면의 텍스처를 바꾸지 않는다.
    CubeMaterial = IsValid(CubeImage) ? CubeImage->GetDynamicMaterial() : nullptr;
    if (IsValid(CubeMaterial) && Puzzle.IsValid())
        CubeMaterial->SetTextureParameterValue(TEXT("CubeView"), Puzzle->GetCubeViewTarget());
}

// 컨트롤러가 정상적으로 닫을 때 Destruct의 중복 종료를 막는다.
void ULMCubePanel::ClearPuzzle()
{
    Puzzle.Reset();
    RestoreGameHUD();
}

// 퍼즐 화면 뒤에 평상시 상호작용 글자가 남지 않게 한다.
void ULMCubePanel::NativeConstruct()
{
    Super::NativeConstruct();

    if (GameHUD.IsValid() && !bHUDHidden && HasCubeView())
    {
        OldHUDVisibility = GameHUD->GetVisibility();
        bHUDHidden = true;
        GameHUD->SetVisibility(ESlateVisibility::Collapsed);
    }
}

// 이미지뿐 아니라 머티리얼의 실제 텍스처 연결까지 확인한다.
bool ULMCubePanel::HasCubeView() const
{
    if (!Puzzle.IsValid() || !IsValid(CubeImage) || !IsValid(CubeMaterial) || !IsValid(Puzzle->GetCubeViewTarget()))
    {
        return false;
    }

    UTexture* ViewTexture = nullptr;
    return CubeMaterial->GetTextureParameterValue(FMaterialParameterInfo(TEXT("CubeView")), ViewTexture) && ViewTexture == Puzzle->GetCubeViewTarget();
}

// 회전 중 매 프레임 표시를 다시 만들지 않고 상태 변경 때만 갱신한다.
void ULMCubePanel::RefreshView()
{
    if (!Puzzle.IsValid()) return;

    const FLMCubeView View = Puzzle->GetView();
    OnPanelUpdated(View.bCanTurn, View.bSolved);
}

// 자신이 숨겼던 HUD만 원래 표시 상태로 복구한다.
void ULMCubePanel::RestoreGameHUD()
{
    if (bHUDHidden && GameHUD.IsValid())
        GameHUD->SetVisibility(OldHUDVisibility);

    bHUDHidden = false;
    GameHUD.Reset();
}

// 예상하지 못한 위젯 제거도 입력 잠금을 남기지 않는다.
void ULMCubePanel::NativeDestruct()
{
    ALMCubeRoomPuzzle* ClosingPuzzle = Puzzle.Get();

    Puzzle.Reset();
    RestoreGameHUD();

    if (IsValid(ClosingPuzzle))
        ClosingPuzzle->ClosePuzzle();

    Super::NativeDestruct();
}

// 현재 화면의 입력만 전달하며 키를 길게 눌러도 자동 회전하지 않는다.
FReply ULMCubePanel::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    ALMPlayerController* Owner = Cast<ALMPlayerController>(GetOwningPlayer());

    if (!Puzzle.IsValid() || !Owner || !Owner->IsCubePuzzleActive(Puzzle.Get()) || InKeyEvent.IsRepeat())
    {
        return FReply::Handled();
    }

    const FKey Pressed = InKeyEvent.GetKey();

    if (Pressed == EKeys::Up)
        Puzzle->Turn(ELMCubeTurn::Forward);
    else if (Pressed == EKeys::Down)
        Puzzle->Turn(ELMCubeTurn::Back);
    else if (Pressed == EKeys::Left)
        Puzzle->Turn(ELMCubeTurn::Left);
    else if (Pressed == EKeys::Right)
        Puzzle->Turn(ELMCubeTurn::Right);
    else if (Pressed == EKeys::BackSpace)
        Puzzle->RequestLeave();

    return FReply::Handled();
}

// 빈 배경을 눌러도 다음 방향키를 이 위젯이 받는다.
FReply ULMCubePanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    SetUserFocus(GetOwningPlayer());
    return FReply::Handled();
}
