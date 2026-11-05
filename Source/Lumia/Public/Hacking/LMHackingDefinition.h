// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LMHackingDefinition.generated.h"
/**
 * 
 */
USTRUCT(BlueprintType)
struct LUMIA_API FLMHexPuzzleDefinition
{
	GENERATED_BODY()

	// 보드의 행 수.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Hacking", meta = (ClampMin = "1"))
	int32 Rows = 1;

	// 보드의 열 수.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Hacking", meta = (ClampMin = "1"))
	int32 Cols = 1;

	// 초기 숫자. 개수는 Rows * Columns, 인덱스는 Row * Columns + Column.
	// 각 값의 허용 범위는 0~15이며 시작 시 컴포넌트가 검사한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Hacking")
	TArray<uint8> InitialValues = { 0 };

	// 모든 셀이 이 숫자가 되면 퍼즐 성공.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Hacking", meta = (ClampMin = "0", ClampMax = "15"))
	uint8 TargetValue = 15;

	// 유효한 영역 치환을 실행할 수 있는 최대 횟수.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Hacking", meta = (ClampMin = "1"))
	int32 MaxMoves = 1;
};