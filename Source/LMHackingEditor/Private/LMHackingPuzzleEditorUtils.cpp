#include "LMHackingPuzzleEditorUtils.h"
#include "Hacking/LMHackingDefinition.h"

// 편집 화면의 축별 크기와 전체 위젯 수를 제한한다.
bool FLMHackingPuzzleEditorUtils::IsGridSizeSupported(int32 Rows, int32 Cols)
{
    return Rows > 0 && Rows <= MaxAxis && Cols > 0 && Cols <= MaxAxis && static_cast<int64>(Rows) * Cols <= MaxGridCells;
}

// 잘못된 원본을 크기 변경으로 조용히 잘라내지 않도록 검사한다.
bool FLMHackingPuzzleEditorUtils::HasRectangularValues(const FLMHexPuzzleDefinition& Definition)
{
    const int64 Count = static_cast<int64>(Definition.Rows) * Definition.Cols;
    return Definition.Rows > 0 && Definition.Cols > 0 && Count < MAX_int32 && Count == Definition.InitialValues.Num();
}

// 열 수가 달라져도 좌표가 같은 기존 셀의 값을 유지한다.
bool FLMHackingPuzzleEditorUtils::Resize(const FLMHexPuzzleDefinition& Source, int32 Rows, int32 Cols, FLMHexPuzzleDefinition& OutDefinition)
{
    if (!IsGridSizeSupported(Rows, Cols) || !HasRectangularValues(Source))
    {
        return false;
    }

    TArray<uint8> Values;
    Values.Init(0, Rows * Cols);
    for (int32 Row = 0; Row < FMath::Min(Source.Rows, Rows); ++Row)
    {
        for (int32 Col = 0; Col < FMath::Min(Source.Cols, Cols); ++Col)
        {
            Values[Row * Cols + Col] = Source.InitialValues[Row * Source.Cols + Col];
        }
    }

    // 유효한 결과가 준비된 뒤 한 번에 교체해 실패 경로에서 원본을 보존한다.
    FLMHexPuzzleDefinition Resized = Source;
    Resized.Rows = Rows;
    Resized.Cols = Cols;
    Resized.InitialValues = MoveTemp(Values);
    OutDefinition = MoveTemp(Resized);
    return true;
}

// 다자리·빈 문자열·16진수 범위 밖 문자를 저장하지 않는다.
bool FLMHackingPuzzleEditorUtils::TryParseHex(const FString& Text, uint8& OutValue)
{
    const FString Trimmed = Text.TrimStartAndEnd();
    if (Trimmed.Len() != 1)
    {
        return false;
    }

    const TCHAR Digit = FChar::ToUpper(Trimmed[0]);
    if (Digit >= TEXT('0') && Digit <= TEXT('9'))
    {
        OutValue = static_cast<uint8>(Digit - TEXT('0'));
        return true;
    }
    if (Digit >= TEXT('A') && Digit <= TEXT('F'))
    {
        OutValue = static_cast<uint8>(Digit - TEXT('A') + 10);
        return true;
    }
    return false;
}
