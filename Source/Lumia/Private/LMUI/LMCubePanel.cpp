#include "LMUI/LMCubePanel.h"
#include "Blueprint/WidgetTree.h"
#include "LMPlayer/LMPlayerController.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

// 면의 일치 여부만 계산하고 텍스트와 색상은 BP가 결정한다.
void ULMCubeFaceWidget::RefreshFace(const FLMCubeView& View)
{
    OnFaceUpdated(Side == View.Exit, Side == View.Guide, View.bUseGoal && Side == View.GoalExit, View.bUseGoal && Side == View.GoalGuide);
}

// 입력을 받을 준비를 하고 퍼즐의 수명은 강제로 유지하지 않는다.
void ULMCubePanel::OpenFor(ALMCubeRoomPuzzle* _Puzzle)
{
    Puzzle = _Puzzle;
    SetIsFocusable(true);
}

// 컨트롤러가 정상적으로 닫을 때 Destruct의 중복 종료를 막는다.
void ULMCubePanel::ClearPuzzle()
{
    Puzzle.Reset();
}

// Designer에 배치한 면 위젯을 한 번 수집한다.
void ULMCubePanel::NativeConstruct()
{
    Super::NativeConstruct();

    Faces.Reset();

    TArray<UWidget*> Children;
    WidgetTree->GetAllWidgets(Children);

    for (UWidget* Child : Children)
    {
        if (ULMCubeFaceWidget* Face = Cast<ULMCubeFaceWidget>(Child))
            Faces.Add(Face);
    }
}

// 방향 중복이나 누락이 있으면 불완전한 조작 화면을 열지 않는다.
bool ULMCubePanel::HasAllFaces() const
{
    TSet<ELMCubeFace> Sides;

    for (const ULMCubeFaceWidget* Face : Faces)
    {
        if (!IsValid(Face)) return false;
        Sides.Add(Face->GetSide());
    }

    return Faces.Num() == 6 && Sides.Num() == 6;
}

// 회전 중 매 프레임 표시를 다시 만들지 않고 상태 변경 때만 갱신한다.
void ULMCubePanel::RefreshView()
{
    if (!Puzzle.IsValid()) return;

    const TWeakObjectPtr<ALMCubeRoomPuzzle> UpdatingPuzzle = Puzzle;
    const FLMCubeView View = Puzzle->GetView();

    // BP 표시 이벤트가 화면을 닫아도 순회 중인 배열이 바뀌지 않게 한다.
    const TArray<TObjectPtr<ULMCubeFaceWidget>> FaceList = Faces;

    for (ULMCubeFaceWidget* Face : FaceList)
    {
        if (!Puzzle.IsValid() || Puzzle != UpdatingPuzzle) return;
        if (!IsValid(Face)) continue;

        Face->RefreshFace(View);
    }

    if (!Puzzle.IsValid() || Puzzle != UpdatingPuzzle) return;

    OnPanelUpdated(View.bCanTurn, View.bSolved);
}

// 예상하지 못한 위젯 제거도 입력 잠금을 남기지 않는다.
void ULMCubePanel::NativeDestruct()
{
    ALMCubeRoomPuzzle* ClosingPuzzle = Puzzle.Get();

    Puzzle.Reset();
    Faces.Reset();

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