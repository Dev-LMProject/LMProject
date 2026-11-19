#include "LMPlayTest/LMPlayTestMenu.h"

#include "AbilitySystemComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LMPlayTestMenu"

// 게임에서 획득한 상태도 같은 스위치에 표시하도록 ASC를 직접 참조한다.
void SLMPlayTestMenu::Construct(const FArguments& InArgs)
{
    // 화면이 플레이어 수명을 늘리지 않도록 ASC는 약한 참조로 보관한다.
    AbilitySystem = InArgs._AbilitySystem;
    OnClose = InArgs._OnClose;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.55f))
        .Cursor(EMouseCursor::Default)
        .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(12)
        [
            SNew(SBox).WidthOverride(380)
            [
                SNew(SBorder).BorderImage(FAppStyle::GetBrush("Brushes.Panel")).Padding(16)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(LOCTEXT("Title", "PIE 테스트")).Font(FAppStyle::GetFontStyle("BoldFont"))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton).Text(LOCTEXT("Close", "닫기 (F7)"))
                            .OnClicked_Lambda([this]()
                            {
                                OnClose.ExecuteIfBound();
                                return FReply::Handled();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
                    [
                        MakeUnlockSwitch(LOCTEXT("Attack", "일반 공격 해금"), LOCTEXT("AttackHelp", "일반 공격·강공격·낙하 공격의 해금 상태를 바꿉니다."), LMGameplayTags::Unlock_Weapon_Sword)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
                    [
                        MakeUnlockSwitch(LOCTEXT("Special", "특수기 해금"), LOCTEXT("SpecialHelp", "특수기 해금 상태를 바꿉니다. 실제 발동에는 콤보 진행 상태가 필요합니다."), LMGameplayTags::Unlock_Attack_Special)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
                    [
                        SNew(STextBlock).Text(LOCTEXT("Session", "설정은 현재 PIE 세션에 적용됩니다."))
                    ]
                ]
            ]
        ]
    ];
}

// 자식 버튼이 처리하지 않은 배경 클릭은 게임 뷰포트까지 전달하지 않는다.
FReply SLMPlayTestMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

// 테스트 전용 가산분 대신 전체 수량을 설정해 실제 검 획득 뒤에도 OFF를 적용한다.
TSharedRef<SWidget> SLMPlayTestMenu::MakeUnlockSwitch(const FText& Label, const FText& Help, FGameplayTag UnlockTag)
{
    const TWeakObjectPtr<UAbilitySystemComponent> PlayerTags = AbilitySystem;
    return SNew(SCheckBox)
        .ToolTipText(Help)
        .IsEnabled_Lambda([PlayerTags]() { return PlayerTags.IsValid(); })
        .IsChecked_Lambda([PlayerTags, UnlockTag]()
        {
            return PlayerTags.IsValid() && PlayerTags->HasMatchingGameplayTag(UnlockTag) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([PlayerTags, UnlockTag](ECheckBoxState NewState)
        {
            if (!PlayerTags.IsValid()) return;

            const bool bEnabled = NewState == ECheckBoxState::Checked;
            PlayerTags->SetLooseGameplayTagCount(UnlockTag, bEnabled ? 1 : 0);

            // 진행 중인 콤보는 해금 조건을 다시 검사하지 않으므로 OFF에서 관련 실행을 끝낸다.
            if (!bEnabled && PlayerTags.IsValid() && (UnlockTag == LMGameplayTags::Unlock_Weapon_Sword || PlayerTags->HasMatchingGameplayTag(LMGameplayTags::State_Attack_Special)))
            {
                const FGameplayTagContainer AttackTags(LMGameplayTags::Ability_Attack);
                PlayerTags->CancelAbilities(&AttackTags);
            }
        })
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8, 4)
            [
                SNew(STextBlock).Text(Label)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 4)
            [
                SNew(STextBlock).Text_Lambda([PlayerTags, UnlockTag]()
                {
                    return PlayerTags.IsValid() && PlayerTags->HasMatchingGameplayTag(UnlockTag) ? LOCTEXT("On", "ON") : LOCTEXT("Off", "OFF");
                })
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
