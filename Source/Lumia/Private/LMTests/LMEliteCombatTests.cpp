#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMCombat/LMAttackData.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMPlayer/LMPlayerState.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMEliteCombatDamageTest, "Lumia.Combat.EliteDamageAndAwakening", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 실제 GE와 속성 계산으로 지정 공격자, 생존 하한, 시연 순서와 재진입을 검사한다.
bool FLMEliteCombatDamageTest::RunTest(const FString& Parameters)
{
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("검사 월드"), TestWorld)) return false;
    ON_SCOPE_EXIT { TestWorld->DestroyWorld(false); };
    TestWorld->InitializeActorsForPlay(FURL());

    // 콘텐츠 애셋 없이 같은 Instant GE 경로를 만들고 검사 종료 때 기본 객체를 복원한다.
    UGameplayEffect* DamageDefaults = GetMutableDefault<UGameplayEffect>();
    const EGameplayEffectDurationType SavedDuration = DamageDefaults->DurationPolicy;
    const TArray<FGameplayModifierInfo> SavedModifiers = DamageDefaults->Modifiers;
    ON_SCOPE_EXIT { DamageDefaults->DurationPolicy = SavedDuration; DamageDefaults->Modifiers = SavedModifiers; };
    DamageDefaults->DurationPolicy = EGameplayEffectDurationType::Instant;
    DamageDefaults->Modifiers.Reset();
    FGameplayModifierInfo DamageModifier;
    DamageModifier.Attribute = ULMCombatAttributeSet::GetIncomingDamageAttribute();
    DamageModifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat DamageValue;
    DamageValue.DataTag = LMGameplayTags::Data_Damage;
    DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageValue);
    DamageDefaults->Modifiers.Add(DamageModifier);

    ALMPlayerCharacter* Player = TestWorld->SpawnActor<ALMPlayerCharacter>();
    ALMPlayerCharacter* OtherPlayer = TestWorld->SpawnActor<ALMPlayerCharacter>();
    ALMFieldMonsterCharacter* Elite = TestWorld->SpawnActor<ALMFieldMonsterCharacter>();
    ALMFieldMonsterCharacter* OtherMob = TestWorld->SpawnActor<ALMFieldMonsterCharacter>();
    if (!TestTrue(TEXT("검사 캐릭터"), Player && OtherPlayer && Elite && OtherMob)) return false;

    // UI나 AI BeginPlay에 의존하지 않고 실제 캐릭터와 ASC의 전투 연결만 준비한다.
    const auto PreparePlayer = [TestWorld](ALMPlayerCharacter* TestPlayer)
    {
        ALMPlayerState* TestState = TestWorld->SpawnActor<ALMPlayerState>();
        ULMAbilitySystemComponent* PlayerSystem = CastChecked<ULMAbilitySystemComponent>(TestState->GetAbilitySystemComponent());
        if (!PlayerSystem->GetSet<ULMCombatAttributeSet>())
            PlayerSystem->AddAttributeSetSubobject(NewObject<ULMCombatAttributeSet>(TestState));
        TestPlayer->InitializeAbilitySystem(TestState, PlayerSystem);
        TestPlayer->DamageEffectClass = UGameplayEffect::StaticClass();
        return const_cast<ULMCombatAttributeSet*>(PlayerSystem->GetSet<ULMCombatAttributeSet>());
    };
    ULMCombatAttributeSet* PlayerHealth = PreparePlayer(Player);
    PreparePlayer(OtherPlayer);
    for (ALMFieldMonsterCharacter* TestMob : {Elite, OtherMob})
    {
        TestMob->AbilitySystemComponent->AddAttributeSetSubobject(TestMob->CombatAttributes.Get());
        TestMob->InitializeAbilitySystem(TestMob, TestMob->AbilitySystemComponent.Get());
        TestMob->DamageEffectClass = UGameplayEffect::StaticClass();
        TestMob->DamageAppliedHandle = TestMob->CombatAttributes->OnDamageApplied.AddUObject(TestMob, &ALMFieldMonsterCharacter::HandleDamageApplied);
    }
    ULMCombatAttributeSet* EliteHealth = Elite->CombatAttributes.Get();
    const TStrongObjectPtr<ULMAttackData> TestAttack(NewObject<ULMAttackData>());

    // 일반 몬스터의 원래 피해와 지정 엘리트의 타격당 피해를 비교한다.
    TestAttack->BaseDamage = 15.0f;
    OtherMob->ReceiveCombatDamage(Player, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("일반 몬스터의 피해는 유지"), OtherMob->CombatAttributes->GetHealth(), 85.0f);
    Elite->SetEliteFightRules(true);
    Elite->ReceiveCombatDamage(Player, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("엘리트는 일반 유효 타격당 1 피해"), EliteHealth->GetHealth(), 99.0f);
    TestEqual(TEXT("공유 공격 데이터는 수정하지 않음"), TestAttack->BaseDamage, 15.0f);
    EliteHealth->SetHealth(1.5f);
    Elite->ReceiveCombatDamage(Player, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("마지막 일반 피해는 1 HP에서 멈춤"), EliteHealth->GetHealth(), 1.0f);
    EliteHealth->SetHealth(0.5f);
    Elite->ReceiveCombatDamage(Player, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("기존 1 미만 체력을 회복시키지 않음"), EliteHealth->GetHealth(), 0.5f);
    EliteHealth->SetHealth(100.0f);

    // 이미 낮은 HP라도 지정 엘리트의 다음 유효피격에서만 각성을 시작한다.
    TestFalse(TEXT("빈 각성 대상 거절"), Player->WatchAwakening(nullptr));
    PlayerHealth->SetHealth(40.0f);
    TestTrue(TEXT("지정 엘리트 감지 시작"), Player->WatchAwakening(Elite));
    TestFalse(TEXT("감지 중 지정 엘리트 변경 거절"), Player->WatchAwakening(OtherMob));
    Player->ReceiveCombatDamage(OtherMob, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("다른 몬스터의 피해는 정상 적용"), PlayerHealth->GetHealth(), 25.0f);
    TestTrue(TEXT("다른 몬스터 피격은 각성을 소비하지 않음"), Player->WatchAwakening(Elite));
    TestAttack->BaseDamage = 5.0f;
    Player->ReceiveCombatDamage(Elite, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("이미 30 퍼센트 이하에서 지정 엘리트 피격 허용"), PlayerHealth->GetHealth(), 20.0f);
    TestFalse(TEXT("진행 중 각성을 다시 시작할 수 없음"), Player->WatchAwakening(Elite));
    Player->ReceiveCombatDamage(OtherMob, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("각성 예약 직후 추가 피해 차단"), PlayerHealth->GetHealth(), 20.0f);
    TestWorld->GetTimerManager().Tick(0.02f);
    Player->StopAwakeningWatch();

    // 절대 30 HP가 아닌 최대 HP 비율과 치명 피해 보호를 검사한다.
    PlayerHealth->SetMaxHealth(200.0f);
    PlayerHealth->SetHealth(70.0f);
    Player->WatchAwakening(Elite);
    TestAttack->BaseDamage = 10.0f;
    Player->ReceiveCombatDamage(Elite, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("최대 HP 200의 30 퍼센트 경계"), PlayerHealth->GetHealth(), 60.0f);
    TestFalse(TEXT("경계값도 각성 처리"), Player->WatchAwakening(Elite));
    Player->StopAwakeningWatch();
    PlayerHealth->SetMaxHealth(100.0f);
    PlayerHealth->SetHealth(40.0f);
    Player->WatchAwakening(Elite);
    TestAttack->BaseDamage = 500.0f;
    Player->ReceiveCombatDamage(Elite, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("치명 피해도 HP 1 보호"), PlayerHealth->GetHealth(), 1.0f);
    Player->StopAwakeningWatch();
    PlayerHealth->SetHealth(0.5f);
    Player->WatchAwakening(Elite);
    Player->ReceiveCombatDamage(Elite, TestAttack.Get(), FHitResult());
    TestEqual(TEXT("보호가 저체력 회복을 만들지 않음"), PlayerHealth->GetHealth(), 0.5f);
    TestFalse(TEXT("보호 후 실제 감소가 0이어도 각성은 성립"), Player->WatchAwakening(Elite));
    Player->StopAwakeningWatch();
    PlayerHealth->SetHealth(100.0f);

    // 시연 태그가 붙은 그 Spec만 예외가 되며 재진입한 일반 공격은 통과하지 않는다.
    TestFalse(TEXT("시연 준비 전 타격 거절"), Elite->ApplySceneHit(Player, false));
    TestTrue(TEXT("시연 대상 준비"), Elite->SetSceneFight(Player, true));
    TestFalse(TEXT("다른 플레이어의 시연 거절"), Elite->ApplySceneHit(OtherPlayer, false));
    TestFalse(TEXT("첫타 전 결정타 거절"), Elite->ApplySceneHit(Player, true));
    const FDelegateHandle ReentryHandle = EliteHealth->OnDamageApplied.AddLambda([Elite, Player, &TestAttack](float, bool)
    {
        // 피해 결과 수신 중 들어온 일반 공격이 시연 예외를 공유하면 잘못된 사망이 발생한다.
        Elite->ReceiveCombatDamage(Player, TestAttack.Get(), FHitResult());
    });
    TestTrue(TEXT("최대 HP 절반의 시연 첫타"), Elite->ApplySceneHit(Player, false));
    TestEqual(TEXT("첫타와 재진입 뒤에도 HP 50"), EliteHealth->GetHealth(), 50.0f);
    TestTrue(TEXT("같은 시연 활성 요청은 허용"), Elite->SetSceneFight(Player, true));
    TestFalse(TEXT("같은 활성 요청으로 첫타 기록이 초기화되지 않음"), Elite->ApplySceneHit(Player, false));
    EliteHealth->OnDamageApplied.Remove(ReentryHandle);
    TestTrue(TEXT("결정타는 남은 HP를 소진"), Elite->ApplySceneHit(Player, true));
    TestEqual(TEXT("처형 HP"), EliteHealth->GetHealth(), 0.0f);
    TestTrue(TEXT("기존 몬스터 사망 태그 경로 재사용"), Elite->GetAbilitySystemComponent()->HasMatchingGameplayTag(LMGameplayTags::State_Dead));
    TestFalse(TEXT("결정타 중복 거절"), Elite->ApplySceneHit(Player, true));
    OtherMob->CombatAttributes->SetHealth(0.5f);
    TestTrue(TEXT("저체력 시연 대상 준비"), OtherMob->SetSceneFight(Player, true));
    TestTrue(TEXT("감소가 0이어도 시연 첫타는 완료"), OtherMob->ApplySceneHit(Player, false));
    TestEqual(TEXT("시연 첫타도 저체력을 회복시키지 않음"), OtherMob->CombatAttributes->GetHealth(), 0.5f);
    TestTrue(TEXT("저체력 첫타 뒤 결정타 허용"), OtherMob->ApplySceneHit(Player, true));
    return true;
}

#endif
