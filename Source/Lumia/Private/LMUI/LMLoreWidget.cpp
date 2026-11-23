#include "LMUI/LMLoreWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "LMPlayer/LMPlayerController.h"
#include "LMUI/LMLoreData.h"

// 일시정지된 상태에서도 위젯이 키 포커스를 받을 수 있게 한다.
ULMLoreWidget::ULMLoreWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

// 표시 전에 로어 원본을 보관한다.
void ULMLoreWidget::InitializeLore(ULMLoreData* _LoreData)
{
	LoreData = _LoreData;
}

// 빈 창을 표시한 채 일시정지하지 않도록 필수 연결을 확인한다.
bool ULMLoreWidget::HasLoreContent() const
{
	return IsValid(LoreData) && IsValid(TitleText) && IsValid(CloseButton) && IsValid(MessageScroll) && IsValid(MessageList) && IsValid(PreviewSender) && IsValid(PreviewMessage);
}

// 화면 제거로 인한 재진입 전에 로어 원본을 해제한다.
void ULMLoreWidget::ClearLore()
{
	LoreData = nullptr;
	bIsClosingLore = false;
}

// 닫기 연출을 시작하고 재생할 수 없으면 바로 화면을 정리한다.
void ULMLoreWidget::RequestClose()
{
	if (!IsValid(LoreData) || bIsClosingLore) return;

	// 연출 중 반복된 닫기 요청을 막고 열기 연출과 속성 제어가 겹치지 않게 한다.
	bIsClosingLore = true;
	if (IsValid(Open) && IsAnimationPlaying(Open))
		StopAnimation(Open);

	// 중지 콜백에서 화면이 이미 정리됐다면 닫기 연출을 다시 시작하지 않는다.
	if (!IsValid(LoreData) || !bIsClosingLore) return;
	if (IsValid(Close))
	{
		const FWidgetAnimationHandle CloseHandle = PlayAnimation(Close);
		if (CloseHandle.IsValid()) return;
	}

	// 애니메이션이 없거나 재생을 시작하지 못해도 게임이 멈춘 채 남지 않는다.
	FinishLoreClose();
}

// 버튼 입력과 DA 내용을 연결한 뒤 열기 애니메이션을 재생한다.
void ULMLoreWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 생성 콜백에서 세션이 이미 닫혔다면 새 목록을 만들지 않는다.
	if (!IsValid(LoreData)) return;
	if (IsValid(CloseButton))
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestClose);

	BuildMessages();
	// 화면이 구성된 뒤에만 열기 연출을 시작한다.
	if (IsValid(LoreData) && !bIsClosingLore && IsValid(Open))
		PlayAnimation(Open);
}

// 외부 제거에서도 남은 화면 소유권과 일시정지를 정리한다.
void ULMLoreWidget::NativeDestruct()
{
	if (IsValid(CloseButton))
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::RequestClose);

	// 외부에서 제거됐다면 보이지 않는 화면의 연출을 기다리지 않고 즉시 복원한다.
	if (IsValid(LoreData))
		FinishLoreClose();

	Super::NativeDestruct();
}

// 닫기 애니메이션이 끝나면 창을 제거하고 게임을 재개한다.
void ULMLoreWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	// 열기 등 다른 애니메이션의 종료는 닫기 완료로 취급하지 않는다.
	if (Animation != Close || !bIsClosingLore) return;
	FinishLoreClose();
}

// 창을 소유한 컨트롤러에 즉시 제거와 입력 복원을 요청한다.
void ULMLoreWidget::FinishLoreClose()
{
	// 컨트롤러의 제거 콜백이 다시 들어와도 닫기 연출을 반복하지 않는다.
	bIsClosingLore = false;
	ALMPlayerController* LoreController = GetOwningPlayer<ALMPlayerController>();
	if (IsValid(LoreController))
		LoreController->CloseLoreWidget(this);
}

// 자식 위젯에 포커스가 있어도 ESC를 닫기 요청으로 처리한다.
FReply ULMLoreWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// 누르고 있는 키가 닫기 요청을 반복하지 않게 한다.
		if (!InKeyEvent.IsRepeat())
			RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// 미리보기 글자 스타일을 복사해 DA의 대화를 작성 순서대로 표시한다.
void ULMLoreWidget::BuildMessages()
{
	if (!WidgetTree || !HasLoreContent())
	{
		return;
	}

	// 미리보기 위젯을 제거하기 전에 디자이너에서 지정한 스타일을 복사한다.
	const FSlateFontInfo SenderFont = PreviewSender->GetFont();
	const FSlateColor SenderColor = PreviewSender->GetColorAndOpacity();
	const FSlateFontInfo MessageFont = PreviewMessage->GetFont();
	const FSlateColor MessageColor = PreviewMessage->GetColorAndOpacity();
	TitleText->SetText(LoreData->Title);
	MessageList->ClearChildren();

	for (const FLMLoreLine& Line : LoreData->Lines)
	{
		// 각 메시지는 원본 순서를 유지하고 긴 본문은 창 너비에 맞게 줄바꿈한다.
		UTextBlock* SenderText = WidgetTree->ConstructWidget<UTextBlock>();
		UTextBlock* MessageText = WidgetTree->ConstructWidget<UTextBlock>();
		if (!IsValid(SenderText) || !IsValid(MessageText))
		{
			continue;
		}

		SenderText->SetText(Line.Sender);
		SenderText->SetFont(SenderFont);
		SenderText->SetColorAndOpacity(SenderColor);
		MessageList->AddChildToVerticalBox(SenderText);

		MessageText->SetText(Line.Message);
		MessageText->SetFont(MessageFont);
		MessageText->SetColorAndOpacity(MessageColor);
		MessageText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* MessageSlot = MessageList->AddChildToVerticalBox(MessageText))
			MessageSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 20.0f));
	}

	// 다시 열 때 이전 스크롤 위치가 아닌 첫 메시지부터 보여준다.
	if (IsValid(MessageScroll))
		MessageScroll->ScrollToStart();
}
