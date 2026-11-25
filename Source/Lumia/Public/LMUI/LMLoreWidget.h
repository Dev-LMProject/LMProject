#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMLoreWidget.generated.h"

class UButton;
class ULMLoreData;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidgetAnimation;

// 로어 원본을 채팅 목록으로 표시하고 닫기 입력을 전달한다.
UCLASS()
class LUMIA_API ULMLoreWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 일시정지 중에도 ESC를 받을 수 있도록 포커스를 허용한다.
	ULMLoreWidget(const FObjectInitializer& ObjectInitializer);

	// 필수 연결을 확인한 뒤 이번 창의 로어 원본과 닫기 완료 이벤트를 연결한다.
	bool InitializeLore(ULMLoreData* _LoreData);

	// 컨트롤러가 화면을 제거하기 전에 로어 참조를 해제한다.
	void ClearLore();

	// 현재 컨트롤러에 이 창의 닫기를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Lore")
	void RequestClose();

protected:
	// 디자이너의 텍스트 스타일을 사용해 대화 목록을 구성한다.
	virtual void NativeConstruct() override;

	// 외부에서 화면을 제거한 경우에도 컨트롤러가 입력과 정지를 복원하게 한다.
	virtual void NativeDestruct() override;

	// 스크롤이나 버튼에 포커스가 있어도 ESC로 창을 닫는다.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// DA 제목을 표시할 디자이너 위젯이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	// 마우스로 창을 닫을 디자이너 버튼이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	// 긴 대화를 스크롤할 디자이너 위젯이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> MessageScroll;

	// DA의 각 메시지를 순서대로 추가할 디자이너 패널이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> MessageList;

	// 디자이너 미리보기와 발신자 글꼴·색상 원본이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PreviewSender;

	// 디자이너 미리보기와 메시지 글꼴·색상 원본이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PreviewMessage;

	// WBP에 같은 이름으로 작성된 열기 애니메이션이다.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Open;

	// WBP에 같은 이름으로 작성된 닫기 애니메이션이다.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Close;

	// 화면이 열려 있는 동안 사용할 로어 원본이다.
	UPROPERTY(Transient)
	TObjectPtr<ULMLoreData> LoreData;

	// 닫기 연출 중 반복 입력으로 재생과 정리가 중복되지 않게 한다.
	bool bIsClosingLore = false;

	// 로어를 정리할 때 이 창의 닫기 완료 연결만 해제한다.
	FDelegateHandle CloseDoneHandle;

	// DA의 각 줄을 디자이너 미리보기의 글꼴과 색상으로 만든다.
	void BuildMessages();

	// 닫기 애니메이션의 종료만 골라 창을 닫는다.
	void OnCloseAnimationFinished(FWidgetAnimationState& FinishedAnimation);

	// 현재 화면을 컨트롤러에 즉시 정리하도록 요청한다.
	void FinishLoreClose();
};
