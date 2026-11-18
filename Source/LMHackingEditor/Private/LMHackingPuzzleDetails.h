#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"

class FReply;
class IPropertyHandle;
class IPropertyUtilities;
struct FLMHexPuzzleDefinition;

// 에셋 원본은 속성 핸들로만 수정하며 배열 원소 주소를 보관하지 않는다.
class FLMHackingPuzzleDetails : public IDetailCustomization, public FSelfRegisteringEditorUndoClient
{
public:
    // 상세 패널 인스턴스를 생성하고 Undo 수명은 엔진 기본 클라이언트에 맡긴다.
    static TSharedRef<IDetailCustomization> MakeInstance();

    // 한 에셋의 설정을 크기 조절 입력과 실제 R×C 격자로 배치한다.
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

    // Undo로 복구한 크기에 맞게 새 속성 핸들과 격자를 만든다.
    virtual void PostUndo(bool bSuccess) override;

    // Redo로 복구한 크기에 맞게 새 속성 핸들과 격자를 만든다.
    virtual void PostRedo(bool bSuccess) override;

private:
    // 현재 속성 주소는 이 호출 안에서만 사용하고 값 복사본을 돌려준다.
    bool ReadDefinition(FLMHexPuzzleDefinition& OutDefinition) const;

    // 상세 패널과 속성 자체가 수정 가능한 상태인지 검사한다.
    bool CanEdit() const;

    // 입력된 크기가 유효하고 격자 생성이나 크기 변경이 필요한지 검사한다.
    bool CanApplySize() const;

    // 구조체 전체를 한 번 설정해 크기와 숫자 배열을 같은 Undo 단위로 저장한다.
    FReply ApplySize();

    // 한 칸의 확정된 입력만 속성 시스템에 전달한다.
    void CommitCell(const FText& Text, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> CellHandle);

    // 다음 틱에 다시 구성해 현재 콜백의 핸들이 도중에 파괴되지 않도록 한다.
    void RequestRefresh();

    // 에셋의 Definition을 가리키며 직접 UObject 수명을 관리하지 않는다.
    TSharedPtr<IPropertyHandle> DefinitionHandle;
    TWeakPtr<IPropertyUtilities> PropertyUtilities;

    // 적용 버튼을 누르기 전까지 원본과 분리된 크기 입력이다.
    FIntPoint DisplayedSize = FIntPoint(1, 1);
    int32 PendingRows = 1;
    int32 PendingCols = 1;

    // 숫자 배열이 비어 있으면 같은 크기로도 새 격자를 만들 수 있다.
    bool bNeedsGridCreation = false;
    FText LastError;
};
