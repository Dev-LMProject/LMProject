#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "LMHackingPuzzleDetails.h"

class FLMHackingEditorModule : public IModuleInterface
{
public:
    // 퍼즐 데이터 에셋에만 격자 상세 패널을 등록한다.
    virtual void StartupModule() override
    {
        FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyEditor.RegisterCustomClassLayout("LMHackingPuzzleData", FOnGetDetailCustomizationInstance::CreateStatic(&FLMHackingPuzzleDetails::MakeInstance));
        PropertyEditor.NotifyCustomizationModuleChanged();
    }

    // 종료 중 내려간 모듈을 다시 로드하지 않고 등록을 해제한다.
    virtual void ShutdownModule() override
    {
        if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
        {
            PropertyEditor->UnregisterCustomClassLayout("LMHackingPuzzleData");
            PropertyEditor->NotifyCustomizationModuleChanged();
        }
    }
};

IMPLEMENT_MODULE(FLMHackingEditorModule, LMHackingEditor)
