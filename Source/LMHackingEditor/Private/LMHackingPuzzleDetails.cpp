#include "LMHackingPuzzleDetails.h"
#include "LMHackingPuzzleEditorUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Hacking/LMHackingPuzzleData.h"
#include "IPropertyUtilities.h"
#include "Input/Reply.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LMHackingPuzzleDetails"

// 에셋을 열 때 필요한 상세 패널을 생성한다.
TSharedRef<IDetailCustomization> FLMHackingPuzzleDetails::MakeInstance()
{
    return MakeShared<FLMHackingPuzzleDetails>();
}

// 현재 값의 주소가 재할당 이후까지 남지 않도록 즉시 복사한다.
bool FLMHackingPuzzleDetails::ReadDefinition(FLMHexPuzzleDefinition& OutDefinition) const
{
    void* Value = nullptr;
    if (!DefinitionHandle.IsValid() || !DefinitionHandle->IsValidHandle() || DefinitionHandle->GetValueData(Value) != FPropertyAccess::Success || Value == nullptr)
    {
        return false;
    }
    OutDefinition = *static_cast<const FLMHexPuzzleDefinition*>(Value);
    return true;
}

// 읽기 전용 에디터 상태에서도 사용자 정의 입력이 변경을 우회하지 못하게 한다.
bool FLMHackingPuzzleDetails::CanEdit() const
{
    const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin();
    return Utilities.IsValid() && Utilities->IsPropertyEditingEnabled() && DefinitionHandle.IsValid() && DefinitionHandle->IsValidHandle() && !DefinitionHandle->IsEditConst();
}

// 실제 크기가 바뀌는 유효한 입력에만 적용 버튼을 활성화한다.
bool FLMHackingPuzzleDetails::CanApplySize() const
{
    return CanEdit() && FLMHackingPuzzleEditorUtils::IsGridSizeSupported(PendingRows, PendingCols) && (DisplayedSize.X != PendingRows || DisplayedSize.Y != PendingCols);
}

// 다중 선택·잘못된 데이터는 원본을 바꾸지 않고 표준 속성 편집기로 보여준다.
void FLMHackingPuzzleDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    PropertyUtilities = DetailBuilder.GetPropertyUtilities();
    DefinitionHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULMHackingPuzzleData, Definition));
    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("LM|Hacking");
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    if (Objects.Num() != 1)
    {
        Category.AddCustomRow(LOCTEXT("Selection", "격자 편집"))
        .WholeRowContent()[SNew(STextBlock).Text(LOCTEXT("SelectOne", "격자 편집은 데이터 에셋 하나를 선택했을 때 사용할 수 있습니다."))];
        return;
    }

    FLMHexPuzzleDefinition Current;
    if (!ReadDefinition(Current))
    {
        return;
    }

    // 배열이나 구조체를 붙여넣거나 되돌린 경우에도 새 크기를 다시 읽는다.
    const FSimpleDelegate Refresh = FSimpleDelegate::CreateSP(this, &FLMHackingPuzzleDetails::RequestRefresh);
    DefinitionHandle->SetOnPropertyValueChanged(Refresh);
    const TSharedPtr<IPropertyHandle> RowsHandle = DefinitionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLMHexPuzzleDefinition, Rows));
    const TSharedPtr<IPropertyHandle> ColsHandle = DefinitionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLMHexPuzzleDefinition, Cols));
    const TSharedPtr<IPropertyHandle> ValuesHandle = DefinitionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLMHexPuzzleDefinition, InitialValues));
    if (!RowsHandle.IsValid() || !ColsHandle.IsValid() || !ValuesHandle.IsValid() || !ValuesHandle->AsArray().IsValid())
    {
        return;
    }
    RowsHandle->SetOnPropertyValueChanged(Refresh);
    ColsHandle->SetOnPropertyValueChanged(Refresh);
    const TSharedPtr<IPropertyHandleArray> ValuesArray = ValuesHandle->AsArray();
    ValuesArray->SetOnNumElementsChanged(Refresh);

    if (!FLMHackingPuzzleEditorUtils::HasRectangularValues(Current) || !FLMHackingPuzzleEditorUtils::IsGridSizeSupported(Current.Rows, Current.Cols))
    {
        Category.AddCustomRow(LOCTEXT("GridUnavailable", "격자 편집 범위"))
        .WholeRowContent()[SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("GridFallback", "격자 편집은 축당 1~64, 전체 1024칸 이하이며 숫자 개수가 Rows×Cols와 같아야 합니다. 원본은 유지됩니다. 아래 기본 속성에서 데이터를 확인하세요."))];
        return;
    }

    DetailBuilder.HideProperty(DefinitionHandle);
    Category.AddProperty(DefinitionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLMHexPuzzleDefinition, AvailableValues)));
    Category.AddProperty(DefinitionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLMHexPuzzleDefinition, TargetValue)));
    Category.AddProperty(DefinitionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLMHexPuzzleDefinition, MaxMoves)));
    // Slate 활성화 조건 평가마다 전체 숫자 배열을 복사하지 않도록 표시 크기만 기억한다.
    DisplayedSize = FIntPoint(Current.Rows, Current.Cols);
    PendingRows = Current.Rows;
    PendingCols = Current.Cols;
    const TWeakPtr<FLMHackingPuzzleDetails> WeakThis = StaticCastSharedRef<FLMHackingPuzzleDetails>(AsShared());

    Category.AddCustomRow(LOCTEXT("Size", "격자 크기"))
    .NameContent()[SNew(STextBlock).Text(LOCTEXT("RowsCols", "행 × 열"))]
    .ValueContent().MinDesiredWidth(320.f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f)
        [
            SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(FLMHackingPuzzleEditorUtils::MaxAxis).AllowSpin(false)
            .IsEnabled(this, &FLMHackingPuzzleDetails::CanEdit)
            .Value_Lambda([WeakThis]() -> TOptional<int32> { const auto Self = WeakThis.Pin(); return Self.IsValid() ? TOptional<int32>(Self->PendingRows) : TOptional<int32>(); })
            .OnValueChanged_Lambda([WeakThis](int32 Value) { if (const auto Self = WeakThis.Pin()) { Self->PendingRows = Value; } })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
        [SNew(STextBlock).Text(FText::FromString(TEXT("×")))]
        + SHorizontalBox::Slot().FillWidth(1.f)
        [
            SNew(SNumericEntryBox<int32>).MinValue(1).MaxValue(FLMHackingPuzzleEditorUtils::MaxAxis).AllowSpin(false)
            .IsEnabled(this, &FLMHackingPuzzleDetails::CanEdit)
            .Value_Lambda([WeakThis]() -> TOptional<int32> { const auto Self = WeakThis.Pin(); return Self.IsValid() ? TOptional<int32>(Self->PendingCols) : TOptional<int32>(); })
            .OnValueChanged_Lambda([WeakThis](int32 Value) { if (const auto Self = WeakThis.Pin()) { Self->PendingCols = Value; } })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
        [SNew(SButton).Text(LOCTEXT("ApplySize", "크기 적용")).IsEnabled(this, &FLMHackingPuzzleDetails::CanApplySize).OnClicked(this, &FLMHackingPuzzleDetails::ApplySize)]
    ];

    Category.AddCustomRow(LOCTEXT("GridHelp", "격자 입력 안내")).WholeRowContent()
    [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("GridInstructions", "각 칸에 0~F를 입력합니다. 행·열은 0부터 시작합니다. 축당 최대 64, 전체 최대 1024칸입니다. 크기를 줄이면 영역 밖 값이 제거되며 Ctrl+Z로 복원할 수 있습니다."))];

    const TSharedRef<SGridPanel> Grid = SNew(SGridPanel);
    for (int32 Col = 0; Col < Current.Cols; ++Col)
    {
        Grid->AddSlot(Col + 1, 0).Padding(4.f)[SNew(STextBlock).Text(FText::AsNumber(Col))];
    }
    for (int32 Row = 0; Row < Current.Rows; ++Row)
    {
        Grid->AddSlot(0, Row + 1).Padding(4.f)[SNew(STextBlock).Text(FText::AsNumber(Row))];
        for (int32 Col = 0; Col < Current.Cols; ++Col)
        {
            const TSharedRef<IPropertyHandle> CellHandle = ValuesArray->GetElement(Row * Current.Cols + Col);
            Grid->AddSlot(Col + 1, Row + 1).Padding(2.f)
            [
                SNew(SBox).WidthOverride(40.f)
                [
                    SNew(SEditableTextBox).Justification(ETextJustify::Center)
                    .IsEnabled(this, &FLMHackingPuzzleDetails::CanEdit)
                    .ToolTipText(FText::Format(LOCTEXT("CellCoordinate", "행 {0}, 열 {1} · 0~F"), FText::AsNumber(Row), FText::AsNumber(Col)))
                    .Text_Lambda([CellHandle]() { uint8 Value = 0; return CellHandle->IsValidHandle() && CellHandle->GetValue(Value) == FPropertyAccess::Success ? FText::FromString(FString::Printf(TEXT("%X"), Value)) : FText::GetEmpty(); })
                    .OnVerifyTextChanged_Lambda([](const FText& Text, FText& Error) { uint8 Value = 0; const bool bValid = Text.IsEmpty() || FLMHackingPuzzleEditorUtils::TryParseHex(Text.ToString(), Value); Error = bValid ? FText::GetEmpty() : LOCTEXT("InvalidHex", "0~F 한 자리만 입력하세요."); return bValid; })
                    .OnTextCommitted(this, &FLMHackingPuzzleDetails::CommitCell, CellHandle)
                ]
            ];
        }
    }

    Category.AddCustomRow(LOCTEXT("InitialGrid", "초기 숫자 격자")).WholeRowContent()
    [SNew(SBox).MaxDesiredHeight(450.f)[SNew(SScrollBox) + SScrollBox::Slot()[SNew(SScrollBox).Orientation(Orient_Horizontal) + SScrollBox::Slot()[Grid]]]];
    Category.AddCustomRow(LOCTEXT("Error", "입력 오류")).WholeRowContent()
    [SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakThis]() { const auto Self = WeakThis.Pin(); return Self.IsValid() ? Self->LastError : FText::GetEmpty(); })];
}

// 크기와 배열을 하나의 구조체 변경으로 기록해 Undo 중 불일치 상태를 만들지 않는다.
FReply FLMHackingPuzzleDetails::ApplySize()
{
    FLMHexPuzzleDefinition Current;
    FLMHexPuzzleDefinition Resized;
    if (!CanEdit() || !ReadDefinition(Current) || !FLMHackingPuzzleEditorUtils::Resize(Current, PendingRows, PendingCols, Resized))
    {
        LastError = LOCTEXT("ResizeFailed", "크기나 기존 숫자 개수를 확인하세요. 원본은 변경하지 않았습니다.");
        return FReply::Handled();
    }

    FString Serialized;
    FLMHexPuzzleDefinition::StaticStruct()->ExportText(Serialized, &Resized, nullptr, nullptr, PPF_None, nullptr);
    if (DefinitionHandle->SetValueFromFormattedString(Serialized) != FPropertyAccess::Success)
    {
        LastError = LOCTEXT("WriteFailed", "변경을 적용하지 못했습니다. 에셋의 편집 가능 상태를 확인하세요.");
        return FReply::Handled();
    }

    // 속성 핸들이 트랜잭션과 변경 알림을 처리한다. 갱신 요청 뒤 기존 핸들을 읽지 않는다.
    RequestRefresh();
    return FReply::Handled();
}

// 입력 취소·잘못된 값은 저장하지 않고 기존 숫자를 다시 표시한다.
void FLMHackingPuzzleDetails::CommitCell(const FText& Text, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> CellHandle)
{
    uint8 Value = 0;
    if (CommitType == ETextCommit::OnCleared || !CanEdit() || !CellHandle->IsValidHandle() || CellHandle->IsEditConst() || !FLMHackingPuzzleEditorUtils::TryParseHex(Text.ToString(), Value))
    {
        RequestRefresh();
        return;
    }

    uint8 CurrentValue = 0;
    if (CellHandle->GetValue(CurrentValue) != FPropertyAccess::Success)
    {
        RequestRefresh();
        return;
    }
    // 동일 값의 확정은 불필요한 Undo 기록을 만들지 않는다.
    if (CurrentValue != Value && CellHandle->SetValue(Value) != FPropertyAccess::Success)
    {
        LastError = LOCTEXT("CellWriteFailed", "숫자를 저장하지 못했습니다. 에셋의 편집 가능 상태를 확인하세요.");
    }
}

// 현재 Slate 콜백이 끝난 뒤 새 속성 핸들로 화면을 다시 구성한다.
void FLMHackingPuzzleDetails::RequestRefresh()
{
    if (const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
    {
        Utilities->RequestForceRefresh();
    }
}

// Undo 결과를 새 격자 구조에 반영한다.
void FLMHackingPuzzleDetails::PostUndo(bool bSuccess)
{
    if (bSuccess)
    {
        RequestRefresh();
    }
}

// Redo 결과를 새 격자 구조에 반영한다.
void FLMHackingPuzzleDetails::PostRedo(bool bSuccess)
{
    if (bSuccess)
    {
        RequestRefresh();
    }
}

#undef LOCTEXT_NAMESPACE
