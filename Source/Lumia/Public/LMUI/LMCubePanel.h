#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMPuzzle/LMCubeRoomPuzzle.h"
#include "LMCubePanel.generated.h"

// 여섯 칸이 공유하는 표시 규칙이며 실제 문구와 모양은 WBP가 만든다.
UCLASS()
class LUMIA_API ULMCubeFaceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이 칸이 담당하는 방향을 읽는다.
	ELMCubeFace GetSide() const { return Side; }

	// 이 칸에 어떤 표식을 켤지 계산해 BP에 전달한다.
	void RefreshFace(const FLMCubeView& View);

protected:
	// 부모 위젯 Designer에서 각 칸의 방향을 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cube")
	ELMCubeFace Side = ELMCubeFace::Front;

	// BP는 전달받은 표식의 표시 여부만 바꾼다.
	UFUNCTION(BlueprintImplementableEvent, Category="Cube")
	void OnFaceUpdated(bool bCurrentExit, bool bCurrentGuide, bool bGoalExit, bool bGoalGuide);
};

// 방향키를 퍼즐에 전달하고 여섯 칸의 표시를 갱신한다.
UCLASS()
class LUMIA_API ULMCubePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// 컨트롤러가 화면에 연결할 퍼즐을 지정한다.
	void OpenFor(ALMCubeRoomPuzzle* _Puzzle);

	// 화면 제거 전에 입력 전달 대상을 끊는다.
	void ClearPuzzle();

	// 여섯 방향이 빠짐없이 한 번씩 배치됐는지 검사한다.
	bool HasAllFaces() const;

	// 상태가 바뀌었을 때만 전개도와 상태 문구를 갱신한다.
	void RefreshView();

protected:
	// 이름 문자열 대신 실제 위젯 타입으로 여섯 칸을 찾는다.
	virtual void NativeConstruct() override;

	// 화면이 외부에서 제거돼도 퍼즐 조작을 정리한다.
	virtual void NativeDestruct() override;

	// 자식에 포커스가 있어도 방향키와 취소 키를 먼저 받는다.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 화면의 빈 곳을 눌러도 키보드 포커스를 유지한다.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// BP가 조작 안내와 성공 문구를 표시한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Cube")
	void OnPanelUpdated(bool bCanTurn, bool bSolved);

private:
	// 이 화면에서 조작하는 큐브 퍼즐. 퍼즐 액터가 사라지면 참조도 무효가 된다.
	TWeakObjectPtr<ALMCubeRoomPuzzle> Puzzle;
	
	// 전개도의 각 면을 표시하는 위젯 6개다. 방이 돌면 이 목록을 순회해 각 면의 표시를 갱신한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULMCubeFaceWidget>> Faces;
};