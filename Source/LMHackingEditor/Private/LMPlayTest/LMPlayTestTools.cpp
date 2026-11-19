#include "LMPlayTest/LMPlayTestTools.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "LMPlayTest/LMPlayTestMenu.h"
#include "Widgets/SViewport.h"

// 에디터 모듈의 수명에 맞춰 입력 처리와 PIE 종료 알림을 등록한다.
void FLMPlayTestTools::Start()
{
    // PIE가 뷰포트를 종료하거나 교체하기 전에 메뉴 입력을 돌려준다.
    PIEEndingHandle = FEditorDelegates::PrePIEEnded.AddSP(AsShared(), &FLMPlayTestTools::OnPIELeavingGameplay);
    ViewSwitchHandle = FEditorDelegates::OnPreSwitchBeginPIEAndSIE.AddSP(AsShared(), &FLMPlayTestTools::OnPIELeavingGameplay);
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().RegisterInputPreProcessor(AsShared());
}

// 모듈을 내릴 때 Slate가 입력 처리기를 계속 소유하지 않도록 먼저 등록을 해제한다.
void FLMPlayTestTools::Stop()
{
    // 등록된 외부 참조를 먼저 끊어 모듈 종료 이후 콜백을 받지 않게 한다.
    FEditorDelegates::PrePIEEnded.Remove(PIEEndingHandle);
    FEditorDelegates::OnPreSwitchBeginPIEAndSIE.Remove(ViewSwitchHandle);
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(AsShared());
    CloseMenu();
}

// PIE 종료 외의 플레이어 파괴나 맵 이동에서도 메뉴가 입력을 가둔 채 남지 않게 한다.
void FLMPlayTestTools::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
    // 이전 세션의 화면이 새 플레이어나 맵의 입력을 막지 않도록 정리한다.
    if (Menu && (!MenuPlayer.IsValid() || !MenuViewport.IsValid() || !GEditor || MenuPlayer->GetWorld() != GEditor->PlayWorld))
        CloseMenu();
}

// 메뉴 안의 버튼에 포커스가 있어도 같은 단축키로 닫을 수 있게 선처리한다.
bool FLMPlayTestTools::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
    // 조합 키는 에디터에 전달하고 메뉴의 F7 입력만 소비한다.
    if (InKeyEvent.GetKey() != EKeys::F7 || InKeyEvent.IsControlDown() || InKeyEvent.IsAltDown() || InKeyEvent.IsShiftDown())
    {
        return false;
    }

    if (Menu)
    {
        if (!Menu->HasKeyboardFocus() && !Menu->HasFocusedDescendants()) return false;
        if (!InKeyEvent.IsRepeat())
            CloseMenu();
        return true;
    }

    return !InKeyEvent.IsRepeat() && OpenMenu();
}

// 게임 입력을 사용하는 로컬 PIE 화면에서만 현재 플레이어의 해금 상태를 연다.
bool FLMPlayTestTools::OpenMenu()
{
    // 에디터 월드의 원본 액터에 테스트 상태를 적용하지 않도록 PIE 월드만 사용한다.
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        return false;
    }

    // 편집 시뮬레이션과 다른 UI가 소유한 입력을 가져오지 않게 진입을 제한한다.
    UGameViewportClient* Viewport = PlayWorld->GetGameViewport();
    APlayerController* GameController = PlayWorld->GetFirstPlayerController();
    if (!Viewport || Viewport->IsSimulateInEditorViewport() || Viewport->IgnoreInput() || !IsValid(GameController) || !GameController->IsLocalPlayerController())
    {
        return false;
    }

    const TSharedPtr<SViewport> GameView = Viewport->GetGameViewportWidget();
    if (!GameView || (!GameView->HasKeyboardFocus() && !GameView->HasFocusedDescendants()))
    {
        return false;
    }

    // 프로젝트의 기존 오버라이드를 호출해 눌린 키와 대기 중인 능력 입력도 함께 취소한다.
    const TWeakObjectPtr<APlayerController> OpeningPlayer(GameController);
    const TWeakObjectPtr<UGameViewportClient> OpeningViewport(Viewport);
    GameController->FlushPressedKeys();
    if (!OpeningPlayer.IsValid() || !OpeningViewport.IsValid() || !GEditor || OpeningPlayer->GetWorld() != GEditor->PlayWorld || OpeningViewport->IsSimulateInEditorViewport() || OpeningViewport->IgnoreInput())
    {
        return false;
    }

    // 몸체 대신 해금 태그를 지속 관리하는 PlayerState의 ASC를 읽는다.
    UAbilitySystemComponent* PlayerTags = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OpeningPlayer->PlayerState);
    if (!IsValid(PlayerTags)) return false;

    // 닫기와 PIE 종료에서 같은 플레이어와 뷰포트에 입력을 복원할 수 있게 연결한다.
    MenuPlayer = OpeningPlayer;
    MenuViewport = OpeningViewport;
    bMouseCursorBeforeMenu = OpeningPlayer->bShowMouseCursor;
    Menu = SNew(SLMPlayTestMenu).AbilitySystem(PlayerTags).OnClose(this, &FLMPlayTestTools::CloseMenu);
    OpeningViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 100);

    // UIOnly를 사용해 스위치를 누르는 클릭이나 이동 키가 게임에도 적용되는 것을 막는다.
    OpeningPlayer->bShowMouseCursor = true;
    FInputModeUIOnly MenuInput;
    MenuInput.SetWidgetToFocus(Menu);
    MenuInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    OpeningPlayer->SetInputMode(MenuInput);
    return true;
}

// 화면의 수명과 해금 태그를 분리해 닫고 다시 열어도 선택한 상태가 유지된다.
void FLMPlayTestTools::CloseMenu()
{
    if (!Menu) return;

    // 멤버를 먼저 비워 제거 중 다시 닫기 요청이 들어와도 중복 복원하지 않는다.
    const TSharedPtr<SWidget> ClosingMenu = MoveTemp(Menu);
    if (UGameViewportClient* Viewport = MenuViewport.Get())
    {
        Viewport->RemoveViewportWidgetContent(ClosingMenu.ToSharedRef());
        Viewport->SetIgnoreInput(false);
    }

    // 현재 화면을 연 플레이어의 커서와 일반 게임 입력을 돌려준다.
    if (APlayerController* GameController = MenuPlayer.Get())
    {
        GameController->bShowMouseCursor = bMouseCursorBeforeMenu;
        if (GameController->GetWorld())
            GameController->SetInputMode(FInputModeGameOnly());
    }

    MenuPlayer.Reset();
    MenuViewport.Reset();
}

// 플레이어와 뷰포트가 살아 있는 동안 입력을 돌려줘 종료와 F8 전환을 이어가게 한다.
void FLMPlayTestTools::OnPIELeavingGameplay(bool bIsSimulating)
{
    CloseMenu();
}
