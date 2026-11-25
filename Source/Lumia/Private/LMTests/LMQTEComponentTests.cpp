#include "LMQTE/LMQTEComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMPlayer/LMPlayerController.h"
#include "LMPlayer/LMPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// 화면 생성만 건너뛰고 실제 컴포넌트의 입력 창과 판정 함수를 사용하는 테스트 경계다.
struct FLMQTETestAccess
{
    // 위젯 없는 단위 테스트에서 실제 설정 검사와 입력 초기화로 요청을 연다.
    static bool OpenRules(ULMQTEComponent& Component, AActor* OwnerActor, const FLMQTEConfig& RuleConfig, int32 RequestId)
    {
        ALMPlayerController* PlayerOwner = Cast<ALMPlayerController>(Component.GetOwner());
        if (Component.HasActiveQTE() || !Component.bAppActive || !Component.IsConfigValid(RuleConfig) || !PlayerOwner || !PlayerOwner->CanStartQTE())
        {
            return false;
        }
        Component.Config = RuleConfig;
        Component.RequestOwner = OwnerActor;
        Component.InputOwner = PlayerOwner;
        Component.ActiveRequestId = RequestId;
        Component.BeginInput(FPlatformTime::Seconds());
        return true;
    }

    // 실제 시간 만료 경로를 대기 없이 검사하도록 마감 시각만 과거로 이동한다.
    static void Expire(ULMQTEComponent& Component)
    {
        Component.Deadline = FPlatformTime::Seconds() - 0.01;
    }

    // 화면을 전환하지 않고 실제 앱 활성화 처리에 동일한 알림을 전달한다.
    static void SetAppActive(ULMQTEComponent& Component, bool bAppActive)
    {
        Component.HandleAppActive(bAppActive);
    }
};

// 게임 입력이나 에디터 창을 열지 않는 임시 월드와 요청자를 소유한다.
struct FLMQTETestWorld
{
    UWorld* TestWorld = nullptr;
    AActor* OwnerActor = nullptr;
    ALMPlayerController* PlayerOwner = nullptr;
    ALMPlayerState* PlayerState = nullptr;
    ALMPlayerCharacter* PlayerBody = nullptr;
    TStrongObjectPtr<ULMQTEComponent> Component;

    // BeginPlay 없이 규칙 검사용 월드와 컴포넌트를 준비한다.
    FLMQTETestWorld()
    {
        TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
        if (!TestWorld) return;
        OwnerActor = TestWorld->SpawnActor<AActor>();
        if (!OwnerActor) return;
        // 실제 플레이어와 ASC 연결을 만들어 수명 검사를 우회하지 않는다.
        PlayerOwner = TestWorld->SpawnActor<ALMPlayerController>();
        PlayerState = TestWorld->SpawnActor<ALMPlayerState>();
        PlayerBody = TestWorld->SpawnActor<ALMPlayerCharacter>();
        if (!PlayerOwner || !PlayerState || !PlayerBody) return;
        // 화면 없는 테스트 월드에서도 실제 로컬 컨트롤러 경로를 명시한다.
        PlayerOwner->SetAsLocalPlayerController();
        PlayerOwner->SetPlayerState(PlayerState);
        // BeginPlay 없는 월드에서도 실제 전투 속성과 Avatar 연결을 완성한다.
        ULMAbilitySystemComponent* PlayerSystem = CastChecked<ULMAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent());
        if (!PlayerSystem->GetSet<ULMCombatAttributeSet>())
            PlayerSystem->AddAttributeSetSubobject(NewObject<ULMCombatAttributeSet>(PlayerState));
        PlayerOwner->Possess(PlayerBody);
        if (!PlayerOwner->CanStartQTE()) return;
        Component.Reset(PlayerOwner->GetQTEComponent());
    }

    // 요청을 취소한 뒤 임시 월드와 강한 참조를 정리한다.
    ~FLMQTETestWorld()
    {
        if (Component.IsValid())
            Component->ResetInputState();
        Component.Reset();
        if (TestWorld)
            TestWorld->DestroyWorld(false);
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMQTEPressWindowTest, "Lumia.QTE.PressWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 이전 홀드와 지난 요청의 취소가 다음 클릭 창을 통과시키지 않는지 검사한다.
bool FLMQTEPressWindowTest::RunTest(const FString& Parameters)
{
    FLMQTETestWorld Fixture;
    if (!TestNotNull(TEXT("QTE test component"), Fixture.Component.Get()))
    {
        return false;
    }
    ULMQTEComponent& Component = *Fixture.Component;
    FLMQTEConfig RuleConfig;
    RuleConfig.InteractionTag = LMGameplayTags::QTE_Interaction_Press;
    RuleConfig.InputTag = LMGameplayTags::InputTag_Attack;
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestTrue(TEXT("Open first window"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 10));
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestTrue(TEXT("Pre-held input cannot finish"), Component.HasActiveQTE());
    Component.SubmitButtonRelease(RuleConfig.InputTag);
    Component.SubmitButtonPress(LMGameplayTags::InputTag_Special);
    TestTrue(TEXT("Wrong input cannot finish"), Component.HasActiveQTE());
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestFalse(TEXT("Fresh correct press finishes"), Component.HasActiveQTE());
    TestTrue(TEXT("Open next click window"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 11));
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestTrue(TEXT("Previous click cannot advance next hit"), Component.HasActiveQTE());
    Component.CancelQTE(10);
    TestEqual(TEXT("Stale cancel preserves new request"), Component.GetActiveRequestId(), 11);
    Component.SubmitButtonRelease(RuleConfig.InputTag);
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestFalse(TEXT("Second fresh click completes second window"), Component.HasActiveQTE());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMQTEMashAndTimeoutTest, "Lumia.QTE.MashAndTimeout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 연타를 새 누름으로만 세고 만료 뒤의 입력을 성공으로 받지 않는지 검사한다.
bool FLMQTEMashAndTimeoutTest::RunTest(const FString& Parameters)
{
    FLMQTETestWorld Fixture;
    if (!TestNotNull(TEXT("QTE test component"), Fixture.Component.Get()))
    {
        return false;
    }
    ULMQTEComponent& Component = *Fixture.Component;
    FLMQTEConfig RuleConfig;
    RuleConfig.InteractionTag = LMGameplayTags::QTE_Interaction_Mash;
    RuleConfig.InputTag = LMGameplayTags::InputTag_Attack;
    RuleConfig.TargetPresses = 2;
    TestTrue(TEXT("Open mash"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 20));
    Component.SubmitButtonPress(RuleConfig.InputTag);
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestEqual(TEXT("Held repeats count once"), Component.GetInputProgress(), 0.5f);
    Component.SubmitButtonRelease(RuleConfig.InputTag);
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestFalse(TEXT("Second distinct press completes mash"), Component.HasActiveQTE());
    Component.SubmitButtonRelease(RuleConfig.InputTag);
    TestTrue(TEXT("Open expiring request"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 21));
    FLMQTETestAccess::Expire(Component);
    TestEqual(TEXT("UI sees expired same clock"), Component.GetRemainingRatio(), 0.0f);
    Component.SubmitButtonPress(RuleConfig.InputTag);
    TestFalse(TEXT("Input after deadline closes request"), Component.HasActiveQTE());
    TestEqual(TEXT("Closed id is cleared"), Component.GetActiveRequestId(), INDEX_NONE);
    RuleConfig.InteractionTag = LMGameplayTags::InputTag_Attack;
    TestFalse(TEXT("Unknown interaction is rejected"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 22));
    RuleConfig.InteractionTag = LMGameplayTags::QTE_Interaction_Mash;
    RuleConfig.TargetPresses = 0;
    TestFalse(TEXT("Zero mash target is rejected"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 23));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMQTEDragAndResetTest, "Lumia.QTE.DragAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 시작 영역과 유지된 드래그만 성공시키고 조기 해제와 포커스 초기화를 처리한다.
bool FLMQTEDragAndResetTest::RunTest(const FString& Parameters)
{
    FLMQTETestWorld Fixture;
    if (!TestNotNull(TEXT("QTE test component"), Fixture.Component.Get()))
    {
        return false;
    }
    ULMQTEComponent& Component = *Fixture.Component;
    FLMQTEConfig RuleConfig;
    RuleConfig.InteractionTag = LMGameplayTags::QTE_Interaction_Drag;
    RuleConfig.InputTag = LMGameplayTags::InputTag_Attack;
    TestTrue(TEXT("Open drag"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 30));
    Component.MoveDrag(30, RuleConfig.DragGoal);
    TestTrue(TEXT("Hover alone cannot finish"), Component.HasActiveQTE());
    TestFalse(TEXT("Wrong start is rejected"), Component.BeginDrag(30, FVector2D(0.5, 0.1)));
    Component.EndDrag(30);
    TestTrue(TEXT("Valid start begins drag"), Component.BeginDrag(30, RuleConfig.DragStart));
    Component.MoveDrag(30, FVector2D(0.8, 0.1));
    TestTrue(TEXT("Rightward but outside goal cannot finish"), Component.HasActiveQTE());
    Component.EndDrag(30);
    Component.MoveDrag(30, RuleConfig.DragGoal);
    TestTrue(TEXT("Released pointer cannot finish"), Component.HasActiveQTE());
    TestTrue(TEXT("Retry can grab start"), Component.BeginDrag(30, RuleConfig.DragStart));
    Component.MoveDrag(29, RuleConfig.DragGoal);
    TestTrue(TEXT("Stale drag id ignored"), Component.HasActiveQTE());
    Component.MoveDrag(30, RuleConfig.DragGoal);
    TestTrue(TEXT("Goal arrival waits for physical release"), Component.HasActiveQTE());
    Component.EndDrag(30);
    TestFalse(TEXT("Release inside goal completes"), Component.HasActiveQTE());
    Component.SubmitButtonRelease(RuleConfig.InputTag);
    TestTrue(TEXT("Open cancelled request"), FLMQTETestAccess::OpenRules(Component, Fixture.OwnerActor, RuleConfig, 31));
    Component.ResetInputState();
    TestFalse(TEXT("Focus or pause reset cancels"), Component.HasActiveQTE());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMQTEAppFocusTest, "Lumia.QTE.AppFocus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 실제 앱 전환 처리로 요청을 취소하고 재활성화 전 재시작을 거부하는지 검사한다.
bool FLMQTEAppFocusTest::RunTest(const FString& Parameters)
{
    FLMQTETestWorld Fixture;
    if (!TestNotNull(TEXT("QTE test component"), Fixture.Component.Get()))
    {
        return false;
    }
    FLMQTEConfig RuleConfig;
    RuleConfig.InteractionTag = LMGameplayTags::QTE_Interaction_Drag;
    RuleConfig.InputTag = LMGameplayTags::InputTag_Attack;
    TestTrue(TEXT("Open drag before any pointer capture"), FLMQTETestAccess::OpenRules(*Fixture.Component, Fixture.OwnerActor, RuleConfig, 50));
    FLMQTETestAccess::SetAppActive(*Fixture.Component, false);
    TestFalse(TEXT("App deactivation cancels without pointer capture"), Fixture.Component->HasActiveQTE());
    TestFalse(TEXT("Inactive app rejects another request"), FLMQTETestAccess::OpenRules(*Fixture.Component, Fixture.OwnerActor, RuleConfig, 51));
    FLMQTETestAccess::SetAppActive(*Fixture.Component, true);
    TestTrue(TEXT("Reactivation permits a fresh request"), FLMQTETestAccess::OpenRules(*Fixture.Component, Fixture.OwnerActor, RuleConfig, 52));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMQTEPlayerDeathTest, "Lumia.QTE.PlayerDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 실제 플레이어 사망 상태에서는 올바른 입력도 현재 요청을 통과시키지 않는다.
bool FLMQTEPlayerDeathTest::RunTest(const FString& Parameters)
{
    FLMQTETestWorld Fixture;
    if (!TestNotNull(TEXT("QTE test component"), Fixture.Component.Get()))
    {
        return false;
    }
    FLMQTEConfig RuleConfig;
    RuleConfig.InteractionTag = LMGameplayTags::QTE_Interaction_Press;
    RuleConfig.InputTag = LMGameplayTags::InputTag_Attack;
    TestTrue(TEXT("Open living player request"), FLMQTETestAccess::OpenRules(*Fixture.Component, Fixture.OwnerActor, RuleConfig, 40));
    UAbilitySystemComponent* PlayerASC = Fixture.PlayerBody->GetAbilitySystemComponent();
    if (!TestNotNull(TEXT("Real player ASC"), PlayerASC))
    {
        return false;
    }
    PlayerASC->AddLooseGameplayTag(LMGameplayTags::State_Dead);
    TestFalse(TEXT("Dead player cannot use QTE"), Fixture.PlayerOwner->CanStartQTE());
    Fixture.Component->SubmitButtonPress(RuleConfig.InputTag);
    TestFalse(TEXT("Death closes active request before judging input"), Fixture.Component->HasActiveQTE());
    return true;
}

#endif
