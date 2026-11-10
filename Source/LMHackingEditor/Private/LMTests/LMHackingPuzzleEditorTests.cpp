#include "LMHackingPuzzleEditorUtils.h"
#include "Hacking/LMHackingDefinition.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMHackingGridResizeTest, "Lumia.Hacking.Definition.GridResize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 열 수 변경 시 좌표 보존·새 칸 초기화·실패 시 출력 보존을 검사한다.
bool FLMHackingGridResizeTest::RunTest(const FString& Parameters)
{
    FLMHexPuzzleDefinition Source;
    Source.Rows = 2;
    Source.Cols = 3;
    Source.InitialValues = { 0, 1, 2, 3, 4, 5 };
    Source.AvailableValues = { 2, 7, 15 };
    Source.MaxMoves = 7;
    FLMHexPuzzleDefinition Output;
    TestTrue(TEXT("Grow grid"), FLMHackingPuzzleEditorUtils::Resize(Source, 3, 4, Output));
    TestTrue(TEXT("Coordinates preserved, new cells zero"), Output.InitialValues == TArray<uint8>({ 0, 1, 2, 0, 3, 4, 5, 0, 0, 0, 0, 0 }));
    TestEqual(TEXT("Palette preserved"), Output.AvailableValues.Num(), 3);
    TestEqual(TEXT("Move limit preserved"), Output.MaxMoves, 7);
    TestEqual(TEXT("Source still has six cells"), Source.InitialValues.Num(), 6);
    TestTrue(TEXT("Shrink columns"), FLMHackingPuzzleEditorUtils::Resize(Source, 2, 2, Output));
    TestTrue(TEXT("Shrink keeps coordinates, not flat prefix"), Output.InitialValues == TArray<uint8>({ 0, 1, 3, 4 }));

    const TArray<uint8> Previous = Output.InitialValues;
    Source.InitialValues.Pop();
    TestFalse(TEXT("Reject malformed source"), FLMHackingPuzzleEditorUtils::Resize(Source, 2, 2, Output));
    TestTrue(TEXT("Failure preserves output"), Output.InitialValues == Previous);
    TestFalse(TEXT("Reject zero dimension"), FLMHackingPuzzleEditorUtils::IsGridSizeSupported(0, 1));
    TestFalse(TEXT("Reject overflowing-sized request"), FLMHackingPuzzleEditorUtils::IsGridSizeSupported(MAX_int32, MAX_int32));
    TestFalse(TEXT("Reject excessive preview"), FLMHackingPuzzleEditorUtils::IsGridSizeSupported(64, 64));
    TestTrue(TEXT("Accept preview boundary"), FLMHackingPuzzleEditorUtils::IsGridSizeSupported(64, 16));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMHackingHexInputTest, "Lumia.Hacking.Definition.HexInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 16개 숫자·소문자·잘못된 입력의 값 보존을 검사한다.
bool FLMHackingHexInputTest::RunTest(const FString& Parameters)
{
    uint8 Value = 0;
    for (uint8 Expected = 0; Expected < 16; ++Expected)
    {
        TestTrue(TEXT("Accept hexadecimal digit"), FLMHackingPuzzleEditorUtils::TryParseHex(FString::Printf(TEXT("%X"), Expected), Value));
        TestEqual(TEXT("Parsed digit"), Value, Expected);
    }
    TestTrue(TEXT("Accept lowercase and surrounding whitespace"), FLMHackingPuzzleEditorUtils::TryParseHex(TEXT(" f "), Value));
    TestEqual(TEXT("F is 15"), Value, static_cast<uint8>(15));
    for (const TCHAR* Invalid : { TEXT(""), TEXT(" "), TEXT("G"), TEXT("10"), TEXT("-1"), TEXT("0xF") })
    {
        Value = 9;
        TestFalse(TEXT("Reject non-digit input"), FLMHackingPuzzleEditorUtils::TryParseHex(Invalid, Value));
        TestEqual(TEXT("Invalid input does not change value"), Value, static_cast<uint8>(9));
    }
    return true;
}

#endif
