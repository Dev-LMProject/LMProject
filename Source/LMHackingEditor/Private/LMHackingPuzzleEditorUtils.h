#pragma once

#include "CoreMinimal.h"

struct FLMHexPuzzleDefinition;

// 격자 편집의 크기·좌표·입력 규칙이며 런타임 퍼즐 제한과는 별개다.
struct FLMHackingPuzzleEditorUtils
{
    static constexpr int32 MaxAxis = 64;
    static constexpr int32 MaxGridCells = 1024;

    // 과도한 Slate 위젯 생성을 막는 직접 편집 범위를 검사한다.
    static bool IsGridSizeSupported(int32 Rows, int32 Cols);

    // 행·열과 실제 저장된 숫자 개수가 일치하는지 오버플로 없이 검사한다.
    static bool HasRectangularValues(const FLMHexPuzzleDefinition& Definition);

    // 겹치는 좌표를 보존하고 새 칸을 0으로 채운다. 실패하면 출력은 변경하지 않는다.
    static bool Resize(const FLMHexPuzzleDefinition& Source, int32 Rows, int32 Cols, FLMHexPuzzleDefinition& OutDefinition);

    // 공백을 제외한 한 자리 0~F만 숫자로 변환하며 실패 시 출력을 유지한다.
    static bool TryParseHex(const FString& Text, uint8& OutValue);
};
