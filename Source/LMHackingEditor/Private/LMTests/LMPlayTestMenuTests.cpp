#include "LMPlayTest/LMPlayTestMenu.h"

#include "AbilitySystemComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SCheckBox.h"

#if WITH_DEV_AUTOMATION_TESTS

// 실제 화면의 체크박스를 찾아 공개 클릭 동작으로 연결된 태그 변경을 검사한다.
static void FindSwitches(const TSharedRef<SWidget>& Widget, TArray<TSharedRef<SCheckBox>>& Switches)
{
    if (Widget->GetType() == FName(TEXT("SCheckBox")))
    {
        Switches.Add(StaticCastSharedRef<SCheckBox>(Widget));
        return;
    }

    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Index < Children->Num(); ++Index)
        FindSwitches(Children->GetChildAt(Index), Switches);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMPlayTestUnlockMenuTest, "Lumia.PlayTest.AttackUnlockMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 실제 획득 뒤 OFF·두 스위치 독립성·외부 지급 반영·다시 열기를 함께 검사한다.
bool FLMPlayTestUnlockMenuTest::RunTest(const FString& Parameters)
{
    const TStrongObjectPtr<UAbilitySystemComponent> PlayerTags(NewObject<UAbilitySystemComponent>());
    const FGameplayTag SwordTag = LMGameplayTags::Unlock_Weapon_Sword;
    const FGameplayTag SpecialTag = LMGameplayTags::Unlock_Attack_Special;
    PlayerTags->AddLooseGameplayTag(SwordTag, 2);
    PlayerTags->AddLooseGameplayTag(SpecialTag);

    TSharedRef<SLMPlayTestMenu> Menu = SNew(SLMPlayTestMenu).AbilitySystem(PlayerTags.Get());
    TArray<TSharedRef<SCheckBox>> Switches;
    FindSwitches(Menu, Switches);
    if (!TestEqual(TEXT("두 개의 해금 스위치"), Switches.Num(), 2)) return false;

    TestTrue(TEXT("게임에서 획득한 일반 공격을 ON으로 표시"), Switches[0]->IsChecked());
    TestTrue(TEXT("특수기 해금도 ON으로 표시"), Switches[1]->IsChecked());
    Switches[0]->ToggleCheckedState();
    TestEqual(TEXT("실제 획득 수량까지 모두 OFF"), PlayerTags->GetTagCount(SwordTag), 0);
    TestEqual(TEXT("일반 공격 OFF가 특수기에는 영향 없음"), PlayerTags->GetTagCount(SpecialTag), 1);

    Switches[0]->ToggleCheckedState();
    TestEqual(TEXT("일반 공격을 다시 ON"), PlayerTags->GetTagCount(SwordTag), 1);
    Switches[1]->ToggleCheckedState();
    TestEqual(TEXT("특수기만 OFF"), PlayerTags->GetTagCount(SpecialTag), 0);
    TestEqual(TEXT("특수기 OFF가 일반 공격에는 영향 없음"), PlayerTags->GetTagCount(SwordTag), 1);

    // 메뉴 밖의 실제 보상 지급도 별도 상태 복사 없이 현재 스위치에 반영되어야 한다.
    PlayerTags->AddLooseGameplayTag(SpecialTag);
    TestTrue(TEXT("외부에서 지급한 특수기 해금을 즉시 표시"), Switches[1]->IsChecked());
    Switches[1]->ToggleCheckedState();
    TestEqual(TEXT("외부 지급 이후에도 OFF"), PlayerTags->GetTagCount(SpecialTag), 0);

    Switches.Reset();
    Menu = SNew(SLMPlayTestMenu).AbilitySystem(PlayerTags.Get());
    FindSwitches(Menu, Switches);
    TestTrue(TEXT("메뉴를 다시 열어도 일반 공격 ON 유지"), Switches[0]->IsChecked());
    TestFalse(TEXT("메뉴를 다시 열어도 특수기 OFF 유지"), Switches[1]->IsChecked());

    // 다음 PIE의 새 플레이어에 이전 세션의 UI 상태를 전파하지 않는다.
    const TStrongObjectPtr<UAbilitySystemComponent> NextPlayerTags(NewObject<UAbilitySystemComponent>());
    Switches.Reset();
    Menu = SNew(SLMPlayTestMenu).AbilitySystem(NextPlayerTags.Get());
    FindSwitches(Menu, Switches);
    TestFalse(TEXT("새 플레이어의 일반 공격은 현재 태그를 사용"), Switches[0]->IsChecked());
    TestFalse(TEXT("새 플레이어의 특수기도 현재 태그를 사용"), Switches[1]->IsChecked());
    return true;
}

#endif
