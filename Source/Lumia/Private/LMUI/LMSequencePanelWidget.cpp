#include "LMUI/LMSequencePanelWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// 재생 방향에 상관없이 해당 프레임의 문자 수를 적용한다.
void ULMSequencePanelWidget::UpdateTyping(float Alpha)
{
	if (!TXT_Typing) return;

	// 앞부분을 매번 다시 구해 이전 프레임의 입력과 무관하게 표시한다.
	const float Progress = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const int32 LetterCount = FMath::FloorToInt(static_cast<double>(FullText.Len()) * Progress);
	TXT_Typing->SetText(FText::FromString(FullText.Left(LetterCount)));
}

// 같은 진행률에서 게이지와 다운로드 상태를 함께 계산한다.
void ULMSequencePanelWidget::UpdateDownload(float Alpha)
{
	if (!PB_Download || !TXT_Percent || !TXT_Status) return;

	// 100% 표기와 완료 상태는 현재 진행률이 끝에 도달했을 때만 표시한다.
	const float Progress = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const int32 WholePercent = FMath::FloorToInt(Progress * 100.0f);
	PB_Download->SetPercent(Progress);
	TXT_Percent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), WholePercent)));
	TXT_Status->SetText(Progress == 1.0f ? NSLOCTEXT("LMSequencePanel", "Complete", "COMPLETE") : NSLOCTEXT("LMSequencePanel", "InProgress", "IN PROGRESS"));
}
