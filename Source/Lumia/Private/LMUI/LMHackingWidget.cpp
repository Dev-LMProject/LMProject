// Fill out your copyright notice in the Description page of Project Settings.


#include "LMUI/LMHackingWidget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "LMComponent/LMHackingComponent.h"

ULMHackingWidget::ULMHackingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

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

void ULMHackingWidget::NativeDestruct()
{
	ClearSession();
	Super::NativeDestruct();
}

void ULMHackingWidget::InitializeForSession(ULMHackingComponent* InHackingComponent)
{
    ClearSession();
    HackingComponent = InHackingComponent;
}

void ULMHackingWidget::ClearSession()
{
    if (ULMHackingComponent* Component = HackingComponent.Get())
    {
        Component->OnPuzzleChanged.RemoveDynamic(this, &ThisClass::RefreshPuzzle);
    }

    HackingComponent.Reset();
}



void ULMHackingWidget::RefreshPuzzle()
{
    ULMHackingComponent* Component = HackingComponent.Get();
    if (!IsValid(Component))
    {
        return;
    }

    OnPuzzleUpdated(Component->GetCellValues(), Component->GetRemainingMoves());
}

void ULMHackingWidget::RequestClose()
{
	ULMHackingComponent* Component = HackingComponent.Get();
	if (!IsValid(Component))
	{
		return;
	}

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

bool ULMHackingWidget::RequestMove(int32 CellIndex, uint8 NewValue)
{
	ULMHackingComponent* Component = HackingComponent.Get();
	return IsValid(Component) && Component->SubmitMove(CellIndex, NewValue);
}

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

