#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"

class APlayerController;
class UGameViewportClient;
class SWidget;

// 플레이어 코드를 수정하지 않고 PIE 뷰포트의 테스트 메뉴와 단축키를 연결한다.
class FLMPlayTestTools : public IInputProcessor, public TSharedFromThis<FLMPlayTestTools>
{
public:
    // 에디터 입력 처리와 PIE 종료 알림을 등록한다.
    void Start();

    // 등록을 해제하고 열려 있는 메뉴의 입력 상태를 복원한다.
    void Stop();

    // 메뉴가 연결된 플레이어가 사라지면 화면과 입력 상태를 정리한다.
    virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;

    // PIE 게임 화면에 포커스가 있을 때 F7로 메뉴를 열거나 닫는다.
    virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

private:
    // 현재 PIE 플레이어의 ASC로 메뉴를 만들고 UI 입력으로 전환한다.
    bool OpenMenu();

    // 메뉴를 제거하고 원래 커서와 게임 입력을 복원한다.
    void CloseMenu();

    // PIE 종료 또는 편집 시뮬레이션 전환 전에 메뉴와 입력 연결을 해제한다.
    void OnPIELeavingGameplay(bool bIsSimulating);

    TSharedPtr<SWidget> Menu;
    TWeakObjectPtr<APlayerController> MenuPlayer;
    TWeakObjectPtr<UGameViewportClient> MenuViewport;
    FDelegateHandle PIEEndingHandle;
    FDelegateHandle ViewSwitchHandle;
    bool bMouseCursorBeforeMenu = false;
};
