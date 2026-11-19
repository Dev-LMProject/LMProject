#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UAbilitySystemComponent;

// 현재 PIE 플레이어의 두 공격 해금 태그를 조작하는 화면이다.
class SLMPlayTestMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLMPlayTestMenu) {}
        SLATE_ARGUMENT(TWeakObjectPtr<UAbilitySystemComponent>, AbilitySystem)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    // 실제 플레이어 태그를 읽는 스위치 두 개와 닫기 버튼을 만든다.
    void Construct(const FArguments& InArgs);

    // 게임 입력 대신 메뉴가 키보드 포커스를 받을 수 있게 한다.
    virtual bool SupportsKeyboardFocus() const override { return true; }

    // 배경을 클릭해도 포커스를 메뉴 안에 유지한다.
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
    // 태그의 현재 상태를 표시하고 해당 태그의 수량을 0 또는 1로 바꾼다.
    TSharedRef<SWidget> MakeUnlockSwitch(const FText& Label, const FText& Help, FGameplayTag UnlockTag);

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    FSimpleDelegate OnClose;
};
