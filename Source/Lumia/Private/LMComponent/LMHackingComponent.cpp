// Fill out your copyright notice in the Description page of Project Settings.


#include "LMComponent/LMHackingComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"

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
	// shortcut: 세션과 UI 연결 전에는 시작을 거절하며, 해당 개발 단계에서 실제 시작 처리로 교체한다.
	return false;
	
	
}

bool ULMHackingComponent::SubmitMove(int32 CellIndex, uint8 NewValue)
{
	if (Phase != ELMHackingPhase::Playing || bIsCleaningUp || !IsCurrentSessionValid())
	{
		return false;
	}

	if (!Cells.IsValidIndex(CellIndex) || NewValue > 15 || UsedMoves >= Definition.MaxMoves)
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

void ULMHackingComponent::CancelHacking()
{
}

void ULMHackingComponent::ConfirmHacking()
{
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

bool ULMHackingComponent::IsCurrentSessionValid() const
{
	return false;
}

void ULMHackingComponent::ReleaseSession()
{
}
