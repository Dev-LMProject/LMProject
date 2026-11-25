#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMSequencePanelWidget.generated.h"

class UProgressBar;
class UTextBlock;

// 시퀀서가 전달한 진행률을 상태창의 글자와 게이지에 표시한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMSequencePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 현재 진행률만으로 타이핑 문구의 길이를 결정한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void UpdateTyping(float Alpha);

	// 게이지, 정수 퍼센트, 완료 문구를 같은 진행률로 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void UpdateDownload(float Alpha);

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
};
