#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMSequencePanelWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidgetAnimation;

// 시퀀서가 현재 프레임에 적용할 위젯 애니메이션이다.
UENUM(BlueprintType)
enum class ELMSequencePanelAnimation : uint8
{
	None,
	Appear,
	Disappear,
	Flicker
};

// 시퀀서가 전달한 진행률을 상태창의 글자와 게이지에 표시한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMSequencePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 현재 진행률만으로 타이핑 문구의 길이를 결정한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void UpdateTyping(float Alpha);

	// 액터의 문구가 있으면 해당 문구를 현재 진행률에 맞춰 표시한다.
	void UpdateTypingFromText(const FString& TypingText, float Alpha);

	// 게이지, 정수 퍼센트, 완료 문구를 같은 진행률로 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void UpdateDownload(float Alpha);

	// 선택한 UMG 클립을 현재 진행률에서 평가한다.
	void UpdateWidgetAnimation(ELMSequencePanelAnimation Clip, float Alpha);

protected:
	// 위젯 블루프린트의 클래스 기본값에서 원본 문구를 교체한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (MultiLine = true))
	FString FullText = TEXT("INITIALIZING SYSTEM...\nNEURAL LINK ESTABLISHED\nDOWNLOADING DATA...");

	UPROPERTY(BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Typing;

	UPROPERTY(BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Percent;

	UPROPERTY(BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (BindWidget))
	TObjectPtr<UProgressBar> PB_Download;

	UPROPERTY(BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Status;

private:
	// 기존 위젯 블루프린트가 생성한 애니메이션 변수를 찾는다.
	UWidgetAnimation* FindPanelAnimation(ELMSequencePanelAnimation Clip) const;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UWidgetAnimation> ActivePanelAnimation;

	FWidgetAnimationHandle PanelAnimationHandle;
	TWeakObjectPtr<UWidget> PanelPoseWidget;
	FWidgetTransform PanelRestTransform;
	float PanelRestOpacity = 1.0f;
};
