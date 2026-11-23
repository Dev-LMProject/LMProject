#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMPuzzle/LMCubeRoomPuzzle.h"
#include "LMCubePanel.generated.h"

class UImage;
class UMaterialInstanceDynamic;

// 실제 큐브의 촬영 화면을 보여 주고 방향키를 퍼즐에 전달한다.
UCLASS()
class LUMIA_API ULMCubePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// 컨트롤러가 화면에 연결할 퍼즐을 지정한다.
	void OpenFor(ALMCubeRoomPuzzle* _Puzzle, UWidget* _GameHUD);

	// 화면 제거 전에 입력 전달 대상을 끊는다.
	void ClearPuzzle();

	// 필수 이미지와 실제 렌더 타깃이 연결됐는지 검사한다.
	bool HasCubeView() const;

	// 상태가 바뀌었을 때만 BP의 조작 표시를 갱신한다.
	void RefreshView();

protected:
	// 퍼즐을 표시하는 동안 기존 상호작용 줄을 잠시 숨긴다.
	virtual void NativeConstruct() override;

	// 화면이 외부에서 제거돼도 퍼즐 조작을 정리한다.
	virtual void NativeDestruct() override;

	// 자식에 포커스가 있어도 방향키와 취소 키를 먼저 받는다.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 화면의 빈 곳을 눌러도 키보드 포커스를 유지한다.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// BP가 키 표시와 성공 강조만 바꾼다.
	UFUNCTION(BlueprintImplementableEvent, Category="Cube")
	void OnPanelUpdated(bool bCanTurn, bool bSolved);

private:
	// 이 화면에서 조작하는 큐브 퍼즐. 퍼즐 액터가 사라지면 참조도 무효가 된다.
	TWeakObjectPtr<ALMCubeRoomPuzzle> Puzzle;
	
	// BP Designer의 CubeImage와 연결하며 누락된 화면은 열지 않는다.
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CubeImage;

	// 이 화면만 사용하는 UI 머티리얼이다.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CubeMaterial;

	TWeakObjectPtr<UWidget> GameHUD;
	ESlateVisibility OldHUDVisibility = ESlateVisibility::Visible;
	bool bHUDHidden = false;

	// 화면 생성 실패와 정상 종료 모두 원래 HUD 표시 상태로 돌린다.
	void RestoreGameHUD();
};
