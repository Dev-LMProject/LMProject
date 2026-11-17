// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Hacking/LMHackingDefinition.h"
#include "LMComponent/LMHackingComponent.h"
#include "LMHackingWidget.generated.h"

class ULMHackingComponent;
class ULMHackingPuzzleData;
class UWidgetAnimation;

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

	// BP 연출 종료 후 실제 퍼즐 값으로 화면을 다시 맞춘다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void FinishMovePresentation();

	// 클릭한 셀 인덱스와 선택 숫자를 현재 해킹 세션에 전달한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	bool RequestMove(int32 CellIndex, uint8 NewValue);

	// 현재 해킹 세션에 실패한 퍼즐의 재시작을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void RequestRetry();
protected:
	// Anim_Cleared 종료 시 성공 상태의 해킹 세션을 확정한다.
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
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

	// 정화와 F 확정 영역을 BP가 전파 순서대로 연출하도록 알린다.
	UFUNCTION(BlueprintNativeEvent, Category = "LM|Hacking")
	void OnMovePropagationStarted(const TArray<int32>& AffectedCellIndices);
	// BP 이벤트를 구현하지 않은 화면의 기본 갱신 동작을 제공한다.
	virtual void OnMovePropagationStarted_Implementation(const TArray<int32>& AffectedCellIndices);

	// 완료 연출은 WBP의 같은 이름의 애니메이션에 자동 연결된다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animations", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Cleared = nullptr;

private:
	// 위젯 디자이너에서 보드 모양을 확인할 퍼즐 데이터.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Hacking|Preview", meta = (AllowPrivateAccess = "true", DesignerRebuild))
	TObjectPtr<ULMHackingPuzzleData> PreviewPuzzleData = nullptr;

	TWeakObjectPtr<ULMHackingComponent> HackingComponent;

	// 파동 연출 중 전체 값 갱신이 셀 글리치를 덮어쓰지 않도록 막는다.
	bool bIsMovePresentationActive = false;
	// 완료 연출의 중복 재생과 재생 중 조기 닫기를 막는다.
	bool bIsClearPresentationActive = false;

	// 연결된 세션을 표시하고 성공 연출을 시작한다.
	UFUNCTION()
	void RefreshPuzzle();

	// 컴포넌트가 전달한 정화와 F 확정 조작을 BP에 전달한다.
	UFUNCTION()
	void HandleMoveResolved(uint8 OriginalValue, uint8 NewValue, const TArray<int32>& AffectedCellIndices);
};
