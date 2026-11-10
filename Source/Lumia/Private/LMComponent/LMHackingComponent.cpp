// Fill out your copyright notice in the Description page of Project Settings.


#include "LMComponent/LMHackingComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "LMPlayer/LMPlayerController.h"

ULMHackingComponent::ULMHackingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	HackingStateTag = LMGameplayTags::State_Hacking_NeedHacking;
}

/*
 Idle과 정상적인 Completed는 예약하지 않는다.
 다음 세션 구현에서는 시작 취소의 되돌리기가 끝날 때까지 Starting을 유지하고, 완료 처리에서는 정리와 완료 콜백 전달이 모두 끝날 때까지 보호 상태를 유지해야 한다. 
 Phase를 Completed로 바꿨다는 이유만으로 콜백 중 예약을 풀면 안 된다. 현재 TryStartHacking/ConfirmHacking/ReleaseSession은 미구현이므로 해당 조회만 추가해 실제 세션 수명까지 완성됐다고 판단하지 않는다.
 */
bool ULMHackingComponent::IsInteractionReserved() const
{
	return bIsCleaningUp
	   || Phase == ELMHackingPhase::Starting
	   || Phase == ELMHackingPhase::Playing
	   || Phase == ELMHackingPhase::Failed
	   || Phase == ELMHackingPhase::AwaitingConfirmation;
}
void ULMHackingComponent::BeginPlay()
{
	// BP BeginPlay에서도 설정된 태그에 맞는 진행 단계를 읽도록 먼저 초기화한다.
	Phase = HackingStateTag == LMGameplayTags::State_Hacking_HackedComplete ? ELMHackingPhase::Completed : ELMHackingPhase::Idle;

	if (HackingStateTag != LMGameplayTags::State_Hacking_NeedHacking && HackingStateTag != LMGameplayTags::State_Hacking_HackedComplete)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Unsupported HackingStateTag '%s'."), *GetPathName(), *HackingStateTag.ToString());
	}
	//super에서 BP의 Beginplay 실행 우려가 있으므로 먼저 Phase 설정 후 Super 호출
	Super::BeginPlay();
	
}

void ULMHackingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}


bool ULMHackingComponent::TryStartHacking(AActor* InInteractor)
{
	// 이미 진행 중인 퍼즐에 새 시작 요청이 들어오는 것을 막는다.
	if (bIsCleaningUp || Phase != ELMHackingPhase::Idle || !IsValid(InInteractor))
	{
		return false;
	}

	// 상호작용 요청자를 조종하는 컨트롤러를 찾는다.
	APawn* InteractorPawn = Cast<APawn>(InInteractor);
	ALMPlayerController* RequestController = IsValid(InteractorPawn) ? Cast<ALMPlayerController>(InteractorPawn->GetController()) : nullptr;

	if (!IsValid(RequestController))
	{
		return false;
	}

	// UI가 읽을 셀을 기존 Definition으로 초기화한다.
	if (!BuildCells())
	{
		return false;
	}

	// 화면 생성 중 실패했을 때 원래 컨트롤러에 정리를 요청하기 위한 참조다.
	const TWeakObjectPtr<ALMPlayerController> OpeningController(RequestController);

	CurrentInteractor = InInteractor;
	SessionController = RequestController;
	Phase = ELMHackingPhase::Starting;

	// 컨트롤러가 화면 생성·일시정지·UIOnly 전환을 수행한다.
	bool bOpened = RequestController->OpenHackingWidget(this) && IsCurrentSessionValid();

	if (bOpened)
	{
		Phase = ELMHackingPhase::Playing;
		OnPuzzleChanged.Broadcast();

		// 초기 화면 갱신 이후에도 이번 세션이 유지되는지 확인한다.
		bOpened = IsCurrentSessionValid();
	}

	if (!bOpened)
	{
		// 화면 열기에 실패하면 세션 참조와 화면을 정리한다.
		CurrentInteractor.Reset();
		SessionController.Reset();

		if (ALMPlayerController* Controller = OpeningController.Get())
		{
			Controller->CloseHackingWidget(this);
		}

		Phase = ELMHackingPhase::Idle;
	}

	return bOpened;
}

bool ULMHackingComponent::SubmitMove(int32 CellIndex, uint8 NewValue)
{
	if (Phase != ELMHackingPhase::Playing || bIsCleaningUp || !IsCurrentSessionValid())
	{
		return false;
	}

	if (!Cells.IsValidIndex(CellIndex) || NewValue > 15 || UsedMoves >= Definition.MaxMoves || !Definition.AvailableValues.Contains(NewValue))
	{
		return false;
	}

	// 이번 조작의 전파 기준을 한 번만 고정한다.
	const uint8 OriginalValue = Cells[CellIndex].GetCurrentValue();

	if (OriginalValue == NewValue)
	{
		return false;
	}

	TArray<FLMHackingCell*> PendingCells;
	PendingCells.Add(&Cells[CellIndex]);

	// 처리 중 추가된 이웃까지 읽기 위해 매번 현재 Num()을 확인한다.
	for (int32 ReadIndex = 0; ReadIndex < PendingCells.Num(); ++ReadIndex)
	{
		// PendingCells가 늘어나도 안전하도록 포인터 값을 복사한다.
		FLMHackingCell* PendingCell = PendingCells[ReadIndex];

		PendingCell->ReceiveValue(OriginalValue, NewValue, PendingCells);
	}

	// 전파된 셀 수와 관계없이 클릭 한 번으로 계산한다.
	++UsedMoves;

	// 마지막 횟수에 완성한 경우도 성공하도록 먼저 검사한다.
	if (IsSolved())
	{
		Phase = ELMHackingPhase::AwaitingConfirmation;
	}
	else if (UsedMoves >= Definition.MaxMoves)
	{
		Phase = ELMHackingPhase::Failed;
	}

	// 전파와 판정을 모두 끝낸 뒤 UI에 한 번 알린다.
	OnPuzzleChanged.Broadcast();
	return true;
}

void ULMHackingComponent::RetryPuzzle()
{
	// 실패한 퍼즐의 활성 세션에서만 재시도를 받는다.
	//TODO:추후에 초기화 버튼으로 재구현
	if (Phase != ELMHackingPhase::Failed || bIsCleaningUp || !IsCurrentSessionValid())
	{
		return;
	}

	ResetPuzzle();
	Phase = ELMHackingPhase::Playing;

	// HUD가 초기 숫자, 회복된 횟수, Playing 상태를 한 번에 읽도록 마지막에 알린다.
	OnPuzzleChanged.Broadcast();
}

void ULMHackingComponent::CancelHacking(ULMHackingWidget* SourceWidget)//SourceWidget=>어느 화면이 이 요청을 보냈는가
{
	if (bIsCleaningUp || (Phase != ELMHackingPhase::Starting && Phase != ELMHackingPhase::Playing && Phase != ELMHackingPhase::Failed))
	{
		return;
	}

	ALMPlayerController* Controller = SessionController.Get();

	// 현재 화면에서 보낸 취소 요청인지 확인한다.
	if (!IsValid(Controller) || !Controller->IsHackingWidgetSource(this, SourceWidget))
	{
		return;
	}

	// 정리와 BP 이벤트 처리 중 중복 요청을 막는다.
	TGuardValue<bool> CleanupGuard(bIsCleaningUp, true);
	const bool bWasStarting = Phase == ELMHackingPhase::Starting;

	ReleaseSession();

	// 생성 중 취소는 기존 TryStartHacking의 실패 처리가 마무리한다.
	if (bWasStarting)
	{
		return;
	}

	ResetPuzzle();
	Phase = ELMHackingPhase::Idle;

	// 정리를 마친 뒤 액터 BP에 취소 완료를 알린다.
	OnHackingCanceled.Broadcast();
}

void ULMHackingComponent::ConfirmHacking(ULMHackingWidget* SourceWidget)
{
	// 성공 화면에서 아직 확정하지 않은 세션만 처리한다.
	if (bIsCleaningUp || Phase != ELMHackingPhase::AwaitingConfirmation)
	{
		return;
	}

	ALMPlayerController* Controller = SessionController.Get();

	// 현재 세션의 화면에서 보낸 확인 요청인지 검사한다.
	if (!IsValid(Controller) || !Controller->IsHackingWidgetSource(this, SourceWidget))
	{
		return;
	}

	// 화면 정리와 완료 이벤트 실행 중 중복 요청을 막는다.
	TGuardValue<bool> CleanupGuard(bIsCleaningUp, true);

	// ReleaseSession이 멤버를 비우므로 원래 요청자를 잠시 보관한다.
	const TWeakObjectPtr<AActor> CompletedInteractor = CurrentInteractor;

	// 정리 콜백보다 먼저 완료를 확정해 중복 처리를 막는다.
	HackingStateTag = LMGameplayTags::State_Hacking_HackedComplete;
	Phase = ELMHackingPhase::Completed;

	// 기존 컨트롤러를 통해 UI 제거와 입력·일시정지 복원을 처리한다.
	ReleaseSession();

	// 화면 정리가 끝난 뒤 액터 BP에 원래 기능 실행을 맡긴다.
	OnHackingCompleted.Broadcast(CompletedInteractor.Get());
}

TArray<uint8> ULMHackingComponent::GetCellValues() const
{
	TArray<uint8> Values;
	Values.Reserve(Cells.Num());

	for (const FLMHackingCell& Cell : Cells)
	{
		Values.Add(Cell.GetCurrentValue());
	}

	return Values;
}


bool ULMHackingComponent::BuildCells()
{
	if (Phase!=ELMHackingPhase::Idle&&Phase!=ELMHackingPhase::Starting)
	{
		return false;
	}
	if (Definition.Rows<=0||Definition.Cols<=0||Definition.MaxMoves<=0||Definition.TargetValue>15)
	{
		return false;
	}
	
	// 선택 목록은 1~16개의 서로 다른 숫자이며 목표값을 포함해야 한다.
	if (Definition.AvailableValues.IsEmpty() || Definition.AvailableValues.Num() > 16 || !Definition.AvailableValues.Contains(Definition.TargetValue))
	{
		return false;
	}

	bool SeenValues[16] = {};

	for (uint8 Value : Definition.AvailableValues)
	{
		// 범위 검사에 통과한 값만 배열 인덱스로 사용한다.
		if (Value > 15 || SeenValues[Value])
		{
			return false;
		}

		SeenValues[Value] = true;
	}
	//오버플로우 방지
	const int64 CellCount = static_cast<int64>(Definition.Rows)*Definition.Cols;
	if (CellCount>=MAX_int32||CellCount!=Definition.InitialValues.Num())
	{
		return false;
	}
	bool bInitSolved=true;
	
	for (uint8 Val:Definition.InitialValues)
	{
		if (Val>15)return false;
		if (Val!=Definition.TargetValue)
		{
			bInitSolved=false;
		}
	}
	// 시작부터 완성된 보드는 퍼즐 설정 오류로 처리
	if (bInitSolved)return false;
	
	Cells.SetNum(static_cast<int32>(CellCount));
	
	for (auto i=0;i<Cells.Num();++i)
	{
		const int32 Row = i / Definition.Cols;
		const int32 Col = i % Definition.Cols;
		Cells[i].Init(Row,Col,Definition.InitialValues[i]);
	}
	
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		const int32 Row = i / Definition.Cols;
		const int32 Column = i % Definition.Cols;
		//좌측 상단이 0,0
		FLMHackingCell* Up = Row > 0 ? &Cells[i - Definition.Cols] : nullptr;
		FLMHackingCell* Down = Row < Definition.Rows - 1 ? &Cells[i + Definition.Cols] : nullptr;
		FLMHackingCell* Left = Column > 0 ? &Cells[i - 1] : nullptr;
		FLMHackingCell* Right = Column < Definition.Cols - 1 ? &Cells[i + 1] : nullptr;

		Cells[i].SetNeighbors(Up, Down, Left, Right);
	}

	UsedMoves = 0;
	return true;
}

void ULMHackingComponent::ResetPuzzle()
{
	// 각 셀을 제자리에서 복원해 이미 연결한 이웃 포인터를 유지한다.
	for (FLMHackingCell& Cell : Cells)
	{
		Cell.ResetValue();
	}
	UsedMoves = 0;
}

bool ULMHackingComponent::IsSolved() const
{
	if (Cells.IsEmpty())
	{
		return false;
	}

	for (const FLMHackingCell& Cell : Cells)
	{
		if (Cell.GetCurrentValue() != Definition.TargetValue)
		{
			return false;
		}
	}

	return true;
}
//퍼즐을 연 뒤 클릭하거나 재시도할 때 실행되므로, 그동안 대상이 파괴되거나 조종 캐릭터가 바뀌거나 화면이 제거됐는지 확인
bool ULMHackingComponent::IsCurrentSessionValid() const
{
	// 이 컴포넌트가 붙어 있는 해킹 대상 액터.
	const AActor* TargetActor = GetOwner();
	// 기존 멤버에 기록된 요청자를 플레이어 Pawn으로 읽는다.
	const APawn* RequestPawn = Cast<APawn>(CurrentInteractor.Get());
	// 기존 멤버에 기록된 시작 당시 컨트롤러.
	const ALMPlayerController* Controller = SessionController.Get();

	if (!IsValid(TargetActor) || !IsValid(RequestPawn) || !IsValid(Controller))
	{
		return false;
	}
	// UObject가 아직 유효하더라도 액터 파괴가 시작됐으면 입력을 받지 않는다.
	if (TargetActor->IsActorBeingDestroyed() || RequestPawn->IsActorBeingDestroyed() || Controller->IsActorBeingDestroyed())
	{
		return false;
	}
	// 조종 대상은 컴포넌트에서 확인하고, 현재 화면의 세션 여부는 컨트롤러에 묻는다.
	return Controller->GetPawn() == RequestPawn && Controller->IsHackingSessionActive(this);
}

void ULMHackingComponent::ReleaseSession()
{
	// 멤버를 비운 뒤에도 원래 컨트롤러에 닫기를 요청할 수 있게 보관한다.
	const TWeakObjectPtr<ALMPlayerController> ControllerToClose = SessionController;

	CurrentInteractor.Reset();
	SessionController.Reset();

	if (ALMPlayerController* Controller = ControllerToClose.Get())
	{
		Controller->CloseHackingWidget(this);
	}
}
