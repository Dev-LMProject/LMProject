// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Hacking/LMHackingDefinition.h"
#include "LMHackingWidget.generated.h"

class ULMHackingComponent;
enum class ELMHackingPhase : uint8;
/**
 * 
 */
UCLASS()
class LUMIA_API ULMHackingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	ULMHackingWidget(const FObjectInitializer& ObjectInitializer);
	void InitializeForSession(ULMHackingComponent * _HackingComponent);	
	void ClearSession();
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void RequestClose();

	// 클릭한 셀 인덱스와 선택 숫자를 현재 해킹 세션에 전달한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	bool RequestMove(int32 CellIndex, uint8 NewValue);

	// 현재 해킹 세션에 실패한 퍼즐의 재시작을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void RequestRetry();
protected:
	//자식 버튼에 포커스가 있어도 ESC를 받아 처리
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Hacking")
	void OnPuzzleInitialized(const FLMHexPuzzleDefinition& Definition);

	// 현재 숫자, 남은 횟수, 진행 단계와 결과 문구를 BP에 전달한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LM|Hacking")
	void OnPuzzleUpdated(const TArray<uint8>& Values, int32 RemainingMoves, ELMHackingPhase Phase, const FText& ResultText);
private:
	TWeakObjectPtr<ULMHackingComponent> HackingComponent;
	UFUNCTION()
	void RefreshPuzzle();
};
