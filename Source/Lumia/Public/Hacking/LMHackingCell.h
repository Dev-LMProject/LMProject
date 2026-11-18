// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
/**
 * 
 */
// 화면의 버튼 한 칸에 대응하는 논리 셀.
// Component가 값으로 소유하므로 UObject/Actor 상속이 필요 없다.
class LUMIA_API FLMHackingCell
{
public:
	// 최초 생성 시 좌표와 초기 숫자를 설정하고 이웃 참조를 비운다.
	FORCEINLINE void Init(const int32 InRow,const int32 InColumn,const uint8 InValue){Row = InRow; Column = InColumn;OriginalValue=InValue;CurrentValue = OriginalValue;}

	// 모든 셀의 저장 공간을 확보한 뒤 상하좌우 이웃을 연결한다.
	FORCEINLINE void SetNeighbors(FLMHackingCell* InUp, FLMHackingCell* InDown, FLMHackingCell* InLeft, FLMHackingCell* InRight){Neighbors={InUp,InDown,InLeft,InRight};}

	// 현재 값이 OriginalValue이고 NewValue와 다를 때만 자기 값을 변경한다.
	// 변경 후 OriginalValue가 남아 있는 이웃을 PendingCells에 추가한다.
	// 실제 변경했으면 true. UI 알림이나 횟수 계산은 하지 않는다.
	bool ReceiveValue(uint8 InOriginalValue, uint8 NewValue, TArray<FLMHackingCell*>& PendingCells);

	// 재시도용 숫자 복원. 좌표와 이웃 참조는 유지한다.
	void ResetValue(){CurrentValue=OriginalValue;};

	// 현재 숫자를 읽는다.
	uint8 GetCurrentValue() const { return CurrentValue; }

	// 보드 내 행 위치를 읽는다.
	int32 GetRow() const { return Row; }

	// 보드 내 열 위치를 읽는다.
	int32 GetColumn() const { return Column; }

private:
	// 보드 내 행 위치.
	int32 Row = INDEX_NONE;

	// 보드 내 열 위치.
	int32 Column = INDEX_NONE;

	//최초 설정 값. 초기화시 사용
	uint8 OriginalValue = 0;
	
	// 현재 숫자의 원본. HUD는 이 값을 복사해 표시한다.
	uint8 CurrentValue = 0;

	// 위쪽 셀의 비소유 참조. 경계이면 nullptr. 
	TArray<FLMHackingCell*> Neighbors;

};