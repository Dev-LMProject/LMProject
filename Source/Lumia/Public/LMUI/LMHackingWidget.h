// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Hacking/LMHackingDefinition.h"
#include "LMComponent/LMHackingComponent.h"
#include "LMHackingWidget.generated.h"

class ULMHackingComponent;
class ULMHackingPuzzleData;

// 퍼즐 UI 입력과 세션 표시를 관리한다.
UCLASS()
class LUMIA_API ULMHackingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	// 위젯의 초기 포커스 가능 상태를 설정한다.
	ULMHackingWidget(const FObjectInitializer& ObjectInitializer);
	// 현재 위젯이 표시할 해킹 세션을 연결한다.
	void InitializeForSession(ULMHackingComponent* _HackingComponent);
	// 세션 알림 구독과 참조를 정리한다.
	void ClearSession();
	// 현재 세션의 진행 상태에 따라 닫기 또는 완료를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void RequestClose();

	// 클릭한 셀 인덱스와 선택 숫자를 현재 해킹 세션에 전달한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	bool RequestMove(int32 CellIndex, uint8 NewValue);

	// 현재 해킹 세션에 실패한 퍼즐의 재시작을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void RequestRetry();
protected:
	// 자식 버튼에 포커스가 있어도 ESC 입력을 처리한다.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// 디자이너에서 지정한 퍼즐 데이터로 그리드 미리보기를 갱신한다.
	virtual void NativePreConstruct() override;
	// 위젯이 화면에 만들어질 때 연결된 퍼즐 상태를 표시한다.
	virtual void NativeConstruct() override;
	// 위젯이 제거될 때 세션 알림과 참조를 정리한다.
	virtual void NativeDestruct() override;

	// 퍼즐 설정을 BP에 전달해 보드 표시를 초기화한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Hacking")
	void OnPuzzleInitialized(const FLMHexPuzzleDefinition& Definition);

	// 현재 숫자, 남은 횟수, 진행 단계와 결과 문구를 BP에 전달한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Hacking")
	void OnPuzzleUpdated(const TArray<uint8>& Values, int32 RemainingMoves, ELMHackingPhase Phase, const FText& ResultText);
private:
	// 위젯 디자이너에서 보드 모양을 확인할 퍼즐 데이터.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Hacking|Preview", meta = (AllowPrivateAccess = "true", DesignerRebuild))
	TObjectPtr<ULMHackingPuzzleData> PreviewPuzzleData = nullptr;

	TWeakObjectPtr<ULMHackingComponent> HackingComponent;
	// 연결된 세션의 보드·횟수·단계 표시를 갱신한다.
	UFUNCTION()
	void RefreshPuzzle();
};
