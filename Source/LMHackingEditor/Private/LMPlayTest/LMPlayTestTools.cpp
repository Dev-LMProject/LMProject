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
    PIEEndingHandle = FEditorDelegates::PrePIEEnded.AddSP(AsShared(), &FLMPlayTestTools::OnPIELeavingGameplay);
    ViewSwitchHandle = FEditorDelegates::OnPreSwitchBeginPIEAndSIE.AddSP(AsShared(), &FLMPlayTestTools::OnPIELeavingGameplay);
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().RegisterInputPreProcessor(AsShared());
}

// 모듈을 내릴 때 Slate가 입력 처리기를 계속 소유하지 않도록 먼저 등록을 해제한다.
void FLMPlayTestTools::Stop()
{
    FEditorDelegates::PrePIEEnded.Remove(PIEEndingHandle);
    FEditorDelegates::OnPreSwitchBeginPIEAndSIE.Remove(ViewSwitchHandle);
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(AsShared());
    CloseMenu();
}

// PIE 종료 외의 플레이어 파괴나 맵 이동에서도 메뉴가 입력을 가둔 채 남지 않게 한다.
void FLMPlayTestTools::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
    if (Menu && (!MenuPlayer.IsValid() || !MenuViewport.IsValid() || !GEditor || MenuPlayer->GetWorld() != GEditor->PlayWorld))
        CloseMenu();
}

// 메뉴 안의 버튼에 포커스가 있어도 같은 단축키로 닫을 수 있게 선처리한다.
bool FLMPlayTestTools::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
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
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        return false;
    }

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

    UAbilitySystemComponent* PlayerTags = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OpeningPlayer->PlayerState);
    if (!IsValid(PlayerTags)) return false;

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

    const TSharedPtr<SWidget> ClosingMenu = MoveTemp(Menu);
    if (UGameViewportClient* Viewport = MenuViewport.Get())
    {
        Viewport->RemoveViewportWidgetContent(ClosingMenu.ToSharedRef());
        Viewport->SetIgnoreInput(false);
    }

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
