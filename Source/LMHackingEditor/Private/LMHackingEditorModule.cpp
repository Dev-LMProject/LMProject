#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "LMHackingPuzzleDetails.h"
#include "LMPlayTest/LMPlayTestTools.h"

class FLMHackingEditorModule : public IModuleInterface
{
public:
    // 퍼즐 상세 패널과 독립적인 PIE 테스트 입력 처리기를 등록한다.
    virtual void StartupModule() override
    {
        FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyEditor.RegisterCustomClassLayout("LMHackingPuzzleData", FOnGetDetailCustomizationInstance::CreateStatic(&FLMHackingPuzzleDetails::MakeInstance));
        PropertyEditor.NotifyCustomizationModuleChanged();

        // 모듈이 도구를 소유해 종료 시 입력 등록까지 함께 정리한다.
        PlayTestTools = MakeShared<FLMPlayTestTools>();
        PlayTestTools->Start();
    }

    // 종료 중 내려간 모듈을 다시 로드하지 않고 등록을 해제한다.
    virtual void ShutdownModule() override
    {
        // Slate의 소유 참조와 델리게이트를 먼저 해제한 뒤 도구를 내려놓는다.
        PlayTestTools->Stop();
        PlayTestTools.Reset();

        if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
        {
            PropertyEditor->UnregisterCustomClassLayout("LMHackingPuzzleData");
            PropertyEditor->NotifyCustomizationModuleChanged();
        }
    }

private:
    TSharedPtr<FLMPlayTestTools> PlayTestTools;
};

IMPLEMENT_MODULE(FLMHackingEditorModule, LMHackingEditor)
