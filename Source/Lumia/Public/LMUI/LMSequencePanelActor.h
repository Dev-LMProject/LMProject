#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LMSequencePanelActor.generated.h"

class ULMSequencePanelWidget;
class UWidgetComponent;

// 월드 공간 상태창을 배치하고 레벨 시퀀스의 값을 위젯에 전달한다.
UCLASS(Blueprintable)
class LUMIA_API ALMSequencePanelActor : public AActor
{
	GENERATED_BODY()

public:
	// 배치용 루트와 월드 공간 위젯 컴포넌트를 생성한다.
	ALMSequencePanelActor();

	// 시퀀서가 평가한 타이핑 진행률을 적용한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void SetTypingAlpha(float _TypingAlpha);

	// 시퀀서가 평가한 다운로드 진행률을 적용한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void SetDownloadAlpha(float _DownloadAlpha);

	// 시퀀서가 평가한 패널 전체 투명도를 적용한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Sequence Panel")
	void SetPanelOpacity(float _PanelOpacity);

	// 위젯이 교체되었거나 패널 값이 바뀌면 현재 상태를 적용한다.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LM|Sequence Panel")
	void RefreshPanel();

protected:
	// 에디터 배치와 컴포넌트 재등록 후 현재 상태를 복원한다.
	virtual void PostRegisterAllComponents() override;

	// 플레이 시작 시 컴포넌트가 만든 위젯에 초기 상태를 적용한다.
	virtual void BeginPlay() override;

#if WITH_EDITOR
	// Details 패널에서 수정한 값을 월드 위젯에 반영한다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> Widget_SystemPanel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Interp, Category = "LM|Sequence Panel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float TypingAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Interp, Category = "LM|Sequence Panel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float DownloadAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Interp, Category = "LM|Sequence Panel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float PanelOpacity = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "LM|Sequence Panel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULMSequencePanelWidget> WidgetRef;

	float LastTypingAlpha = -1.0f;
	float LastDownloadAlpha = -1.0f;
	float LastPanelOpacity = -1.0f;
};
