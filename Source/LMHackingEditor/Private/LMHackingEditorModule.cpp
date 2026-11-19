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

        PlayTestTools = MakeShared<FLMPlayTestTools>();
        PlayTestTools->Start();
    }

    // 종료 중 내려간 모듈을 다시 로드하지 않고 등록을 해제한다.
    virtual void ShutdownModule() override
    {
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
