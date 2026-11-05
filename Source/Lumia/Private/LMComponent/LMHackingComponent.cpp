// Fill out your copyright notice in the Description page of Project Settings.


#include "LMComponent/LMHackingComponent.h"


ULMHackingComponent::ULMHackingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool ULMHackingComponent::TryStartHacking(AActor* InInteractor)
{
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

void ULMHackingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
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
