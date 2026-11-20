// Fill out your copyright notice in the Description page of Project Settings.


#include "LMUI/LMHackingWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "LMComponent/LMHackingComponent.h"
#include "Hacking/LMHackingPuzzleData.h"

// 위젯의 초기 포커스 가능 상태를 설정한다.
ULMHackingWidget::ULMHackingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

// 에디터에서는 지정한 퍼즐 데이터만 사용해 기존 BP 표시 이벤트로 미리보기를 구성한다.
void ULMHackingWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 실행 중 데이터 경로에는 손대지 않고 디자이너 미리보기에서만 사용한다.
	if (!IsDesignTime() || !IsValid(PreviewPuzzleData))
	{
		return;
	}

	const FLMHexPuzzleDefinition& Definition = PreviewPuzzleData->Definition;
	const int64 CellCount = static_cast<int64>(Definition.Rows) * Definition.Cols;
	// 잘못된 크기나 과도한 셀 수가 디자이너에서 대량 위젯 생성으로 이어지지 않게 막는다.
	if (Definition.Rows <= 0 || Definition.Cols <= 0 || CellCount > 1024 || CellCount != Definition.InitialValues.Num())
	{
		return;
	}

	// WBP가 기존 런타임과 같은 이벤트로 셀 생성과 값 표시를 처리하도록 설정 데이터를 전달한다.
	OnPuzzleInitialized(Definition);
	OnPuzzleUpdated(Definition.InitialValues, Definition.MaxMoves, ELMHackingPhase::Playing, FText::GetEmpty());
}

// 위젯이 화면에 만들어질 때 연결된 퍼즐 상태를 표시한다.
void ULMHackingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ULMHackingComponent* Component = HackingComponent.Get();
	if (!IsValid(Component))
	{
		return;
	}

	Component->OnMoveResolved.AddUniqueDynamic(this, &ThisClass::HandleMoveResolved);
	Component->OnPuzzleChanged.AddUniqueDynamic(this, &ThisClass::RefreshPuzzle);
	OnPuzzleInitialized(Component->GetDefinition());
	RefreshPuzzle();
}

// 위젯이 제거될 때 세션 알림과 참조를 정리한다.
void ULMHackingWidget::NativeDestruct()
{
	ClearSession();
	Super::NativeDestruct();
}

// 현재 위젯이 표시할 해킹 세션을 연결한다.
void ULMHackingWidget::InitializeForSession(ULMHackingComponent* _HackingComponent)
{
	ClearSession();
	HackingComponent = _HackingComponent;
}

// 세션 알림 구독과 참조를 정리한다.
void ULMHackingWidget::ClearSession()
{
	if (ULMHackingComponent* Component = HackingComponent.Get())
	{
		Component->OnMoveResolved.RemoveDynamic(this, &ThisClass::HandleMoveResolved);
		Component->OnPuzzleChanged.RemoveDynamic(this, &ThisClass::RefreshPuzzle);
	}

	bIsMovePresentationActive = false;
	bIsClearPresentationActive = false;
	HackingComponent.Reset();
}


// 연결된 세션을 표시하고 성공 연출을 시작한다.
void ULMHackingWidget::RefreshPuzzle()
{
	ULMHackingComponent* Component = HackingComponent.Get();
	if (!IsValid(Component) || bIsMovePresentationActive)
	{
		return;
	}

	OnPuzzleUpdated(Component->GetCellValues(), Component->GetRemainingMoves(), Component->GetPhase(), Component->GetResultText());

	// 화면 갱신 중 세션이 바뀌었거나 아직 성공하지 않았다면 완료 연출을 시작하지 않는다.
	if (Component != HackingComponent.Get() || !IsValid(Component) || Component->GetPhase() != ELMHackingPhase::AwaitingConfirmation || bIsClearPresentationActive)
	{
		return;
	}

	if (!IsValid(Anim_Cleared))
	{
		// 애니메이션이 없는 위젯도 성공 세션을 정상적으로 정리한다.
		UE_LOG(LogTemp, Warning, TEXT("%s: Widget animation Anim_Cleared was not bound."), *GetPathName());
		RequestClose();
		return;
	}

	// 성공 상태 갱신이 반복되어도 완료 연출은 한 번만 재생한다.
	bIsClearPresentationActive = true;
	PlayAnimation(Anim_Cleared);
}

// BP의 파동 연출이 끝난 뒤 컴포넌트의 최종 보드와 진행 상태를 다시 표시한다.
void ULMHackingWidget::FinishMovePresentation()
{
	if (!bIsMovePresentationActive) return;

	bIsMovePresentationActive = false;
	RefreshPuzzle();
}

// WBP 애니메이션 종료 중 Anim_Cleared만 해킹 성공 확정으로 연결한다.
void ULMHackingWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	// 위젯 진입 애니메이션 등 다른 시퀀스가 끝났을 때는 성공 처리하지 않는다.
	if (!bIsClearPresentationActive) return;
	if (Animation != Anim_Cleared.Get()) return;

	// 완료 애니메이션이 끝난 현재 성공 세션을 컨트롤러 경로로 닫는다.
	bIsClearPresentationActive = false;
	RequestClose();
}

// BP 이벤트를 구현하지 않은 화면은 다음 OnPuzzleChanged에서 최종 값을 갱신한다.
void ULMHackingWidget::OnMovePropagationStarted_Implementation(const TArray<int32>& AffectedCellIndices)
{
	// 기본 구현은 변경 목록을 사용하지 않고 다음 전체 갱신을 허용한다.
	static_cast<void>(AffectedCellIndices);
	bIsMovePresentationActive = false;
}

// 1에서 0 또는 0에서 F로 전파되는 조작만 즉시 갱신을 보류하고 BP 연출을 시작한다.
void ULMHackingWidget::HandleMoveResolved(uint8 OriginalValue, uint8 NewValue, const TArray<int32>& AffectedCellIndices)
{
	// 정화와 F 확정 이동만 전체 갱신을 연출 종료까지 보류한다.
	const bool bIsPurificationMove = OriginalValue == 1 && NewValue == 0;
	const bool bIsFSpreadMove = OriginalValue == 0 && NewValue == 15;

	if ((!bIsPurificationMove && !bIsFSpreadMove) || AffectedCellIndices.IsEmpty())
	{
		return;
	}

	bIsMovePresentationActive = true;
	OnMovePropagationStarted(AffectedCellIndices);
}

// 현재 세션의 진행 상태에 따라 닫기 또는 완료를 요청한다.
void ULMHackingWidget::RequestClose()
{
	ULMHackingComponent* Component = HackingComponent.Get();
	if (!IsValid(Component)) return;

	switch (Component->GetPhase())
	{
	case ELMHackingPhase::Starting:
	case ELMHackingPhase::Playing:
	case ELMHackingPhase::Failed:
		Component->CancelHacking(this);
		break;

	case ELMHackingPhase::AwaitingConfirmation:
		// 성공 파동과 완료 애니메이션을 끝까지 보여준 뒤에만 확정한다.
		if (!bIsMovePresentationActive && !bIsClearPresentationActive)
			Component->ConfirmHacking(this);
		break;

	default:
		break;
	}
}

// 클릭한 셀 인덱스와 선택 숫자를 현재 해킹 세션에 전달한다.
bool ULMHackingWidget::RequestMove(int32 CellIndex, uint8 NewValue)
{
	// 셀 파동이 진행되는 동안 중복 입력으로 연출과 실제 보드가 어긋나지 않게 막는다.
	if (bIsMovePresentationActive) return false;

	ULMHackingComponent* Component = HackingComponent.Get();
	return IsValid(Component) && Component->SubmitMove(CellIndex, NewValue);
}

// 위젯에 연결된 해킹 컴포넌트에 재시도 요청을 전달한다.
void ULMHackingWidget::RequestRetry()
{
	// 이 화면에 연결된 세션만 요청 대상으로 사용한다.
	ULMHackingComponent* Component = HackingComponent.Get();
	if (!IsValid(Component)) return;

	// 실패 상태와 세션 유효성 검사는 기존 컴포넌트가 담당한다.
	Component->RetryPuzzle();
}

// 자식 버튼에 포커스가 있어도 ESC 입력을 처리한다.
FReply ULMHackingWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// 누르고 있는 ESC가 닫기 요청을 반복하지 않도록 한다.
		if (!InKeyEvent.IsRepeat())
		{
			RequestClose();
		}

		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

