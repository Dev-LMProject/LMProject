// Fill out your copyright notice in the Description page of Project Settings.


#include "LMUI/LMHackingWidget.h"
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
        Component->OnPuzzleChanged.RemoveDynamic(this, &ThisClass::RefreshPuzzle);
    }

	HackingComponent.Reset();
}


// 연결된 세션의 보드·횟수·단계 표시를 갱신한다.
void ULMHackingWidget::RefreshPuzzle()
{
	ULMHackingComponent* Component = HackingComponent.Get();
	if (!IsValid(Component)) return;

	OnPuzzleUpdated(Component->GetCellValues(), Component->GetRemainingMoves(), Component->GetPhase(), Component->GetResultText());
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
		Component->ConfirmHacking(this);
		break;

	default:
		break;
	}
}

// 클릭한 셀 인덱스와 선택 숫자를 현재 해킹 세션에 전달한다.
bool ULMHackingWidget::RequestMove(int32 CellIndex, uint8 NewValue)
{
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

