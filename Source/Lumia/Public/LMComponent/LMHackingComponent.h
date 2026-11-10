#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Hacking/LMHackingCell.h"
#include "Hacking/LMHackingDefinition.h"
#include "LMHackingComponent.generated.h"

class AActor;
class ALMPlayerController;
class ULMHackingWidget;
class ULMHackingPuzzleData;

// 퍼즐 계산 결과와 해킹 완료 확인을 구분하는 진행 단계.
UENUM(BlueprintType)
enum class ELMHackingPhase : uint8
{
	Idle,                 // 활성 세션 없음.
	Starting,             // 세션 예약과 UI 진입 준비.
	Playing,              // 퍼즐 조작 가능.
	Failed,               // 횟수 소진. 재시도 또는 취소 가능.
	AwaitingConfirmation, // 퍼즐 성공. 완료 화면을 닫기 전.
	Completed             // 해킹 완료 확정.
};

// UI가 최종 보드·횟수·진행 단계를 다시 읽도록 알리는 이벤트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLMHackingPuzzleChanged);

// 해킹 확정과 화면 정리 후 원래 요청자를 액터 BP에 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMHackingCompleted, AActor*, Interactor);

// 해킹 취소와 화면 정리가 끝났음을 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLMHackingCanceled);

UCLASS(ClassGroup = (LM), meta = (BlueprintSpawnableComponent))
class LUMIA_API ULMHackingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Tick을 사용하지 않도록 기본 설정한다.
	ULMHackingComponent();

	// 사전 상호작용 처리에서 NeedHacking으로 분기된 요청의 세션을 시작한다.
	// true는 퍼즐 성공이 아니라 해킹 화면 진입 성공을 의미한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	bool TryStartHacking(AActor* InInteractor);
	
	// 대상이 가진 상호작용 태그의 복사본을 반환한다.
	UFUNCTION(BlueprintGetter, Category="LM|Hacking")
	FGameplayTag GetHackingStateTag() const { return HackingStateTag; }
	// Playing 상태에서 클릭 한 번을 처리한다.
	// 입력 검사 → 전파 큐 실행 → 횟수 증가 → 성공/실패 판정 → UI 알림.
	// 실제 변경을 처리했으면 true. 동일 값과 잘못된 입력은 false.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	bool SubmitMove(int32 CellIndex, uint8 NewValue);

	// Failed 상태에서 초기 숫자와 횟수를 복원하고 Playing으로 돌아간다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void RetryPuzzle();

	// 진행·실패 화면을 취소하고 보드 초기화와 세션 정리를 요청한다.
	// 성공 화면의 사용자 닫기·ESC는 ConfirmHacking으로 연결한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void CancelHacking(ULMHackingWidget* SourceWidget);

	// AwaitingConfirmation에서 완료를 확정한다.
	// 태그 전환·세션 복원 후 외부 보상을 한 번 전달한다.
	UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
	void ConfirmHacking(ULMHackingWidget* SourceWidget);

	// UI가 보드 크기와 목표 등을 읽도록 설정 복사본을 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|Hacking")
	FLMHexPuzzleDefinition GetDefinition() const { return ActiveDefinition; }

	// 현재 숫자들을 행 우선 순서로 복사한다. 셀 포인터는 노출하지 않는다.
	UFUNCTION(BlueprintPure, Category = "LM|Hacking")
	TArray<uint8> GetCellValues() const;

	// 남은 조작 횟수를 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|Hacking")
	int32 GetRemainingMoves() const { return FMath::Max(0, ActiveDefinition.MaxMoves - UsedMoves); }

	// UI가 조작 가능 여부와 표시 화면을 결정하도록 현재 단계를 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|Hacking")
	ELMHackingPhase GetPhase() const { return Phase; }

	// 성공 화면에 표시할 힌트·결과 문구를 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|Hacking")
	FText GetResultText() const { return ResultText; }

	// 한 조작의 전파·횟수·판정을 모두 끝낸 뒤 한 번 알린다.
	UPROPERTY(BlueprintAssignable, Category = "LM|Hacking")
	FLMHackingPuzzleChanged OnPuzzleChanged;

	// 액터 BP가 원래 상호작용 기능을 이어서 실행하도록 알린다.
	UPROPERTY(BlueprintAssignable, Category = "LM|Hacking")
	FLMHackingCompleted OnHackingCompleted;
	
	UFUNCTION(BlueprintPure, Category = "LM|Hacking")
	bool IsInteractionReserved() const;

	// 액터 BP가 취소 이후의 처리를 연결한다.
	UPROPERTY(BlueprintAssignable, Category = "LM|Hacking")
	FLMHackingCanceled OnHackingCanceled;
protected:
	virtual void BeginPlay() override;
	// 소유 액터 종료 시 미확정 세션과 자신이 확보한 자원을 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 크기·배열 개수·숫자·최대 횟수·초기 완성 여부를 검사한다.
	// 통과하면 Cells 전체 확보 → 값 초기화 → 이웃 연결 순으로 구성한다.
	bool BuildCells();

	// 셀 배열을 재생성하지 않고 초기 숫자와 UsedMoves만 복원한다.
	void ResetPuzzle();

	// 모든 셀의 현재 숫자가 TargetValue인지 검사한다.
	bool IsSolved() const;

	// 요청자·컨트롤러가 유효하고 자신이 현재 활성 세션인지 검사한다.
	bool IsCurrentSessionValid() const;
	
	// 컨트롤러에 화면·입력·일시정지 복원을 요청하고 이 컴포넌트의 세션을 정리한다.
	void ReleaseSession();

	// 이 액터가 사용할 퍼즐 원본이다. 지정하지 않으면 해킹을 시작하지 않는다.
	UPROPERTY(EditAnywhere, Category = "LM|Hacking")
	TObjectPtr<ULMHackingPuzzleData> PuzzleData = nullptr;

	// 기존 BP·레벨의 작성값을 잃지 않도록 이름과 직렬화를 보존한다. 게임에서는 사용하지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "LM|Hacking|Migration", meta = (DisplayName = "이전 퍼즐 설정", ToolTip = "기존 값을 데이터 에셋으로 옮길 때만 참고합니다. 실행에는 Puzzle Data를 사용합니다."))
	FLMHexPuzzleDefinition Definition;

	// 시작할 때 복사한 설정으로 세션 중 원본 에셋의 변경 영향을 받지 않는다.
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "LM|Hacking|Runtime")
	FLMHexPuzzleDefinition ActiveDefinition;

	// R×C 논리 셀의 실제 저장소. 일반 C++ 타입이므로 UPROPERTY를 붙이지 않는다.
	// 이웃 연결 후에는 배열 재할당·복사·이동을 하지 않는다.
	TArray<FLMHackingCell> Cells;

	// 유효한 클릭을 처리한 횟수. 변경된 셀 개수와 관계없이 조작당 1 증가.
	int32 UsedMoves = 0;

	// 현재 해킹 진행 단계.
	ELMHackingPhase Phase = ELMHackingPhase::Idle;

	// 퍼즐 성공 후 표시할 작성된 힌트·결과 문구.
	UPROPERTY(EditAnywhere, Category = "LM|Hacking")
	FText ResultText;

	// 완료를 전달할 같은 레벨의 외부 액터 목록.
	// 시작 시 인터페이스를 검사하고 전달 시 중복 제거·유효성 재검사한다.
	UPROPERTY(EditInstanceOnly, Category = "LM|Hacking")
	TArray<TObjectPtr<AActor>> ResultReceivers;

	// 현재 해킹을 요청한 액터. 수명을 강제로 유지하지 않는다.
	TWeakObjectPtr<AActor> CurrentInteractor;

	// 세션을 시작한 원래 컨트롤러. 입력 복원 대상을 확인할 때 사용한다.
	TWeakObjectPtr<ALMPlayerController> SessionController;

	// 종료 처리 중 중복 정리와 새 요청을 막는다.
	bool bIsCleaningUp = false;
	
	UPROPERTY(EditAnywhere, BlueprintGetter = GetHackingStateTag, Category = "LM|Hacking", meta = (AllowPrivateAccess = "true", Categories = "LM.State.Hacking"))
	FGameplayTag HackingStateTag;
	// 현재 요청자의 실행 중 스캔 능력을 종료하고 시각 효과를 복원한다.
	bool StopScanForCurrentInteractor();

	// 시작 콜백이 끝나기 전에 새 시작이나 퍼즐 조작이 끼어드는 것을 막는다.
	bool bIsStartingSession = false;
};
