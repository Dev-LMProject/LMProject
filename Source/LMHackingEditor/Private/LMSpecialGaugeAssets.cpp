#include "Animation/AnimMontage.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LMCombat/LMAnimNotifyStateAttackHitWindow.h"
#include "LMCombat/LMAttackData.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
// Python에 노출되지 않은 인터페이스·몽타주 편집만 명시적 에디터 명령으로 수행한다.
DEFINE_LOG_CATEGORY_STATIC(LogLMSpecialGaugeAssets, Log, All);

// 지정된 작업 애셋 하나만 저장하며 전체 맵이나 다른 패키지는 저장하지 않는다.
bool SaveGaugeAsset(UObject& Asset)
{
    UPackage* Package = Asset.GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    const bool bSaved = UPackage::SavePackage(Package, &Asset, *Filename, SaveArgs);
    UE_CLOG(!bSaved, LogLMSpecialGaugeAssets, Error, TEXT("Cannot save %s"), *Filename);
    return bSaved;
}

// 임시 위젯은 표시 인터페이스만 구현하므로 팀원 위젯도 같은 계약으로 교체할 수 있다.
void LinkGaugeView()
{
    UBlueprint* ViewInterface = LoadObject<UBlueprint>(nullptr, TEXT("/Game/UI/LMGauge/BI_LM_SpecialGaugeView.BI_LM_SpecialGaugeView"));
    UWidgetBlueprint* View = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/LMGauge/WBP_LM_SpecialGauge.WBP_LM_SpecialGauge"));
    if (!ViewInterface || !ViewInterface->GeneratedClass || !View || !View->GeneratedClass)
    {
        UE_LOG(LogLMSpecialGaugeAssets, Error, TEXT("Create the gauge interface and widget first."));
        return;
    }
    if (!View->GeneratedClass->ImplementsInterface(ViewInterface->GeneratedClass))
    {
        View->Modify();
        if (!FBlueprintEditorUtils::ImplementNewInterface(View, FTopLevelAssetPath(ViewInterface->GeneratedClass)))
        {
            UE_LOG(LogLMSpecialGaugeAssets, Error, TEXT("Could not implement the gauge view interface."));
            return;
        }
        FKismetEditorUtilities::CompileBlueprint(View);
        if (View->Status == BS_Error || !SaveGaugeAsset(*View)) return;
    }
    UE_LOG(LogLMSpecialGaugeAssets, Display, TEXT("LM_SPECIAL_VIEW_LINKED"));
}

// 섹션 이름과 타격창 시간을 대조하여 특수 두 타격만 충전 없는 데이터로 연결한다.
void LinkSpecialHitData()
{
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/LMCharacter/LMPlayer/LMAnimation/LMCombat/AM_LM_PlayerCombo.AM_LM_PlayerCombo"));
    ULMAttackData* SpecialData = LoadObject<ULMAttackData>(nullptr, TEXT("/Game/LMCharacter/LMPlayer/Data/LMAttackData/DA_LM_PlayerSpecialHit.DA_LM_PlayerSpecialHit"));
    if (!Montage || !SpecialData || SpecialData->SpecialChargeGain != 0.0f)
    {
        UE_LOG(LogLMSpecialGaugeAssets, Error, TEXT("Create the zero-charge special hit data first."));
        return;
    }
    TArray<ULMAnimNotifyStateAttackHitWindow*> SpecialWindows;
    TSet<FName> HitSections;
    for (const FAnimNotifyEvent& Notify : Montage->Notifies)
    {
        const FName SectionName = Montage->GetSectionName(Montage->GetSectionIndexFromPosition(Notify.GetTime()));
        ULMAnimNotifyStateAttackHitWindow* HitWindow = Cast<ULMAnimNotifyStateAttackHitWindow>(Notify.NotifyStateClass);
        if (HitWindow && (SectionName == TEXT("SpecialAttack02") || SectionName == TEXT("SpecialAttack03")))
        {
            if (HitWindow->AttackDataOverride && HitWindow->AttackDataOverride != SpecialData)
            {
                UE_LOG(LogLMSpecialGaugeAssets, Error, TEXT("Preserving unexpected override on %s"), *HitWindow->GetName());
                return;
            }
            SpecialWindows.Add(HitWindow);
            HitSections.Add(SectionName);
        }
    }
    if (SpecialWindows.Num() != 2 || HitSections.Num() != 2)
    {
        UE_LOG(LogLMSpecialGaugeAssets, Error, TEXT("Expected one hit window in each special section; found %d."), SpecialWindows.Num());
        return;
    }
    Montage->Modify();
    for (ULMAnimNotifyStateAttackHitWindow* HitWindow : SpecialWindows)
    {
        HitWindow->Modify();
        HitWindow->AttackDataOverride = SpecialData;
    }
    if (SaveGaugeAsset(*Montage))
        UE_LOG(LogLMSpecialGaugeAssets, Display, TEXT("LM_SPECIAL_HIT_DATA_LINKED"));
}

FAutoConsoleCommand LinkGaugeViewCommand(TEXT("LM.SpecialGauge.LinkView"), TEXT("Implement the gauge view interface on the temporary gauge widget."), FConsoleCommandDelegate::CreateStatic(&LinkGaugeView));
FAutoConsoleCommand LinkSpecialHitDataCommand(TEXT("LM.SpecialGauge.LinkHitData"), TEXT("Assign zero-charge attack data to the two existing special hit windows."), FConsoleCommandDelegate::CreateStatic(&LinkSpecialHitData));
}
