// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMHackingWidget.generated.h"

class ULMHackingComponent;
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
protected:
	//자식 버튼에 포커스가 있어도 ESC를 받아 처리
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
private:
	TWeakObjectPtr<ULMHackingComponent> HackingComponent;
};
