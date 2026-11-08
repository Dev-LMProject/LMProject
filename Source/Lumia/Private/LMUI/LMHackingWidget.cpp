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

void ULMHackingWidget::InitializeForSession(ULMHackingComponent* InHackingComponent)
{
	HackingComponent = InHackingComponent;
}

void ULMHackingWidget::ClearSession()
{
	HackingComponent.Reset();
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