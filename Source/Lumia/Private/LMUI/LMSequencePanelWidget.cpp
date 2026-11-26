#include "LMUI/LMSequencePanelWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Evaluation/MovieScenePlayback.h"
#include "MovieScene.h"
#include "UObject/UnrealType.h"

namespace
{
// 월드 위젯이 그려지기 전에 예약된 UMG 평가를 처리한다.
void FlushPanelAnimation(const FWidgetAnimationState& AnimationState)
{
	if (const TSharedPtr<UE::MovieScene::FSharedPlaybackState> Playback = AnimationState.GetSharedPlaybackState())
	{
		if (const TSharedPtr<FMovieSceneEntitySystemRunner> Runner = Playback->GetRunner())
			Runner->Flush();
	}
}
}

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

// 기존 위젯 블루프린트에 저장된 클립 참조를 변수 이름으로 얻는다.
UWidgetAnimation* ULMSequencePanelWidget::FindPanelAnimation(ELMSequencePanelAnimation Clip) const
{
	FName ClipName;
	switch (Clip)
	{
	case ELMSequencePanelAnimation::Appear: ClipName = TEXT("ANIM_Appear"); break;
	case ELMSequencePanelAnimation::Disappear: ClipName = TEXT("ANIM_Disappear"); break;
	case ELMSequencePanelAnimation::Flicker: ClipName = TEXT("ANIM_Flicker"); break;
	default: return nullptr;
	}

	const FObjectPropertyBase* ClipProperty = FindFProperty<FObjectPropertyBase>(GetClass(), ClipName);
	return ClipProperty ? Cast<UWidgetAnimation>(ClipProperty->GetObjectPropertyValue_InContainer(this)) : nullptr;
}

// UMG 클립의 시간이 시퀀서의 현재 값으로만 결정되도록 평가한다.
void ULMSequencePanelWidget::UpdateWidgetAnimation(ELMSequencePanelAnimation Clip, float Alpha)
{
	UWidgetAnimation* NextClip = FindPanelAnimation(Clip);
	if (ActivePanelAnimation != NextClip)
	{
		// 일시정지 상태의 클립도 종료하고 예약된 평가를 마친다.
		if (const TSharedPtr<FWidgetAnimationState> OldState = PanelAnimationHandle.PinAnimationState())
		{
			OldState->SetPlaybackStatus(EMovieScenePlayerStatus::Paused);
			OldState->Stop();
			FlushPanelAnimation(*OldState);
		}

		// 종료 평가에서 첫 프레임이 적용될 수 있으므로 클립 전 상태를 다시 적용한다.
		if (UWidget* OldPoseWidget = PanelPoseWidget.Get())
		{
			OldPoseWidget->SetRenderTransform(PanelRestTransform);
			OldPoseWidget->SetRenderOpacity(PanelRestOpacity);
		}
		PanelPoseWidget.Reset();

		// 다음 클립이 값을 덮기 전에 내부 패널의 현재 상태를 저장한다.
		if (NextClip)
		{
			if (UWidget* NewPoseWidget = GetWidgetFromName(TEXT("Overlay_Root")))
			{
				PanelPoseWidget = NewPoseWidget;
				PanelRestTransform = NewPoseWidget->GetRenderTransform();
				PanelRestOpacity = NewPoseWidget->GetRenderOpacity();
			}
		}

		PanelAnimationHandle = FWidgetAnimationHandle();
		ActivePanelAnimation = NextClip;
	}
	if (!NextClip) return;

	// 끝 프레임의 다음 틱은 재생 범위 밖이므로 마지막 평가 가능 틱까지만 사용한다.
	const float ClipStart = NextClip->GetStartTime();
	const float TickTime = static_cast<float>(NextClip->GetMovieScene()->GetTickResolution().AsInterval());
	const float ClipEnd = FMath::Max(ClipStart, NextClip->GetEndTime() - TickTime);
	const float ClipTime = FMath::Lerp(ClipStart, ClipEnd, FMath::Clamp(Alpha, 0.0f, 1.0f));
	TSharedPtr<FWidgetAnimationState> CurrentState = PanelAnimationHandle.PinAnimationState();
	if (!CurrentState || !CurrentState->IsValid() || CurrentState->IsPendingDelete())
	{
		PanelAnimationHandle = PlayAnimation(NextClip, ClipTime, 1, EUMGSequencePlayMode::Forward, 0.0f, true);
		CurrentState = PanelAnimationHandle.PinAnimationState();
	}
	if (!CurrentState) return;

	// 위치를 바꾼 뒤 일시정지를 요청하면 해당 프레임이 즉시 평가된다.
	CurrentState->SetCurrentTime(ClipTime);
	CurrentState->Pause();
	FlushPanelAnimation(*CurrentState);
}
