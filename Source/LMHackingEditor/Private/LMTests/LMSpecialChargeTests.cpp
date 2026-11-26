#include "LMPlayer/LMPlayerState.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "GameplayEffect.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMCombat/LMAttackData.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMCombat/LMAttackAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMCombat/LMANSDodgeParry.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMSpecialChargeResourceTest, "Lumia.Combat.SpecialCharge.Resource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 실제 PlayerState의 해금·충전·소비를 실행하여 재해금 무료 충전과 중복 소비를 막는다.
bool FLMSpecialChargeResourceTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues WorldValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldValues);
    TestWorld->InitializeActorsForPlay(FURL());
    ALMPlayerState* State = TestWorld->SpawnActor<ALMPlayerState>();
    UAbilitySystemComponent* ASC = State->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(State, State);

    // BP가 사용하는 공개 함수 계약으로 호출하여 아직 없는 구현도 정상적인 실패로 기록한다.
    auto Call = [this, State](const FName Name, float Amount = 0.0f)
    {
        UFunction* Function = State->FindFunction(Name);
        if (!TestNotNull(*Name.ToString(), Function)) return false;
        FStructOnScope Arguments(Function);
        if (FFloatProperty* AmountProperty = FindFProperty<FFloatProperty>(Function, TEXT("ChargeAmount")))
            AmountProperty->SetPropertyValue_InContainer(Arguments.GetStructMemory(), Amount);
        State->ProcessEvent(Function, Arguments.GetStructMemory());
        const FBoolProperty* ResultProperty = CastField<FBoolProperty>(Function->GetReturnProperty());
        return ResultProperty && ResultProperty->GetPropertyValue_InContainer(Arguments.GetStructMemory());
    };
    auto Charge = [ASC]()
    {
        FProperty* ChargeProperty = FindFProperty<FProperty>(ULMCombatAttributeSet::StaticClass(), TEXT("SpecialCharge"));
        return ChargeProperty ? ASC->GetNumericAttribute(FGameplayAttribute(ChargeProperty)) : -1.0f;
    };

    TestFalse(TEXT("Locked attacks cannot charge"), Call(TEXT("AddSpecialCharge"), 10.0f));
    TestTrue(TEXT("First unlock fills the gauge"), Call(TEXT("UnlockSpecialAttack")));
    TestEqual(TEXT("First unlock grants 100"), Charge(), 100.0f);
    TestTrue(TEXT("Full gauge can be spent"), Call(TEXT("TrySpendSpecialCharge")));
    TestEqual(TEXT("First strike consumes all charge"), Charge(), 0.0f);
    TestFalse(TEXT("Repeated spend cannot succeed"), Call(TEXT("TrySpendSpecialCharge")));
    Call(TEXT("UnlockSpecialAttack"));
    TestEqual(TEXT("Repeated unlock cannot refill"), Charge(), 0.0f);
    TestFalse(TEXT("Negative gain is rejected"), Call(TEXT("AddSpecialCharge"), -10.0f));
    TestTrue(TEXT("Normal hit adds charge"), Call(TEXT("AddSpecialCharge"), 10.0f));
    TestEqual(TEXT("A normal hit grants ten"), Charge(), 10.0f);
    TestFalse(TEXT("Partial gauge cannot be spent"), Call(TEXT("TrySpendSpecialCharge")));
    TestEqual(TEXT("Rejected spend preserves charge"), Charge(), 10.0f);
    TestTrue(TEXT("Parry uses the shared resource"), Call(TEXT("AddParryCharge")));
    TestEqual(TEXT("Parry adds twenty, not thirty"), Charge(), 30.0f);
    TestTrue(TEXT("Gain is capped"), Call(TEXT("AddSpecialCharge"), 200.0f));
    TestEqual(TEXT("Charge never exceeds maximum"), Charge(), 100.0f);
    Call(TEXT("TrySpendSpecialCharge"));
    TestTrue(TEXT("Room completion reveals and fills once"), Call(TEXT("ShowSpecialGauge")));
    TestEqual(TEXT("Elite completion grants a full gauge"), Charge(), 100.0f);
    Call(TEXT("TrySpendSpecialCharge"));
    Call(TEXT("ShowSpecialGauge"));
    TestEqual(TEXT("Repeated room completion never refills"), Charge(), 0.0f);
    Call(TEXT("AddSpecialCharge"), 50.0f);
    ASC->SetNumericAttributeBase(ULMCombatAttributeSet::GetHealthAttribute(), 0.0f);
    TestEqual(TEXT("Death clears charge"), Charge(), 0.0f);
    TestFalse(TEXT("Dead players cannot charge"), Call(TEXT("AddSpecialCharge"), 10.0f));

    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMSpecialChargeHitTest, "Lumia.Combat.SpecialCharge.ConfirmedDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 피해량 1에도 고정 보상을 주되 무효 피해·시체·특수 타격에서는 충전하지 않는다.
bool FLMSpecialChargeHitTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues WorldValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldValues);
    TestWorld->InitializeActorsForPlay(FURL());
    ALMPlayerState* State = TestWorld->SpawnActor<ALMPlayerState>();
    ALMPlayerCharacter* PlayerBody = TestWorld->SpawnActor<ALMPlayerCharacter>();
    ALMFieldMonsterCharacter* TargetBody = TestWorld->SpawnActor<ALMFieldMonsterCharacter>();
    // UI 초기화와 무관하게 실제 ASC 소유자·Avatar 계약으로 피해 속성을 실행한다.
    ALMPlayerState* TargetState = TestWorld->SpawnActor<ALMPlayerState>();
    UAbilitySystemComponent* SourceASC = State->GetAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = TargetState->GetAbilitySystemComponent();
    SourceASC->InitAbilityActorInfo(State, PlayerBody);
    TargetASC->InitAbilityActorInfo(TargetState, TargetBody);
    State->UnlockSpecialAttack();
    State->TrySpendSpecialCharge();

    ULMAttackData* HitData = NewObject<ULMAttackData>();
    FFloatProperty* GainProperty = FindFProperty<FFloatProperty>(ULMAttackData::StaticClass(), TEXT("SpecialChargeGain"));
    TestNotNull(TEXT("Attack data exposes a separate hit reward"), GainProperty);
    if (GainProperty)
        GainProperty->SetPropertyValue_InContainer(HitData, 10.0f);
    auto Charge = [SourceASC]() { return SourceASC->GetNumericAttribute(ULMCombatAttributeSet::GetSpecialChargeAttribute()); };
    auto Hit = [SourceASC, TargetASC, HitData](float Damage)
    {
        UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>();
        DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
        FGameplayModifierInfo& Modifier = DamageEffect->Modifiers.AddDefaulted_GetRef();
        Modifier.Attribute = ULMCombatAttributeSet::GetIncomingDamageAttribute();
        Modifier.ModifierOp = EGameplayModOp::Additive;
        Modifier.ModifierMagnitude = FScalableFloat(Damage);
        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddSourceObject(HitData);
        FGameplayEffectSpec Spec(DamageEffect, Context, 1.0f);
        TargetASC->ApplyGameplayEffectSpecToSelf(Spec);
    };

    Hit(1.0f);
    TestEqual(TEXT("One damage gives the full ten-point hit reward"), Charge(), 10.0f);
    Hit(0.0f);
    TestEqual(TEXT("Zero damage cannot charge"), Charge(), 10.0f);
    if (GainProperty)
        GainProperty->SetPropertyValue_InContainer(HitData, 0.0f);
    Hit(10.0f);
    TestEqual(TEXT("Special attack data cannot charge itself"), Charge(), 10.0f);
    if (GainProperty)
        GainProperty->SetPropertyValue_InContainer(HitData, 10.0f);
    Hit(500.0f);
    TestEqual(TEXT("A killing blow still charges once"), Charge(), 20.0f);
    Hit(10.0f);
    TestEqual(TEXT("A corpse cannot charge"), Charge(), 20.0f);
    TestWorld->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLMSpecialFirstStrikeOrderTest, "Lumia.Combat.SpecialCharge.FirstStrikeOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 소비 알림의 동기 취소가 첫 타격 시작 전에 자원만 없애는 일을 막는다.
bool FLMSpecialFirstStrikeOrderTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues WorldValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldValues);
    TestWorld->InitializeActorsForPlay(FURL());
    UClass* PlayerClass = LoadClass<ALMPlayerCharacter>(nullptr, TEXT("/Game/LMCharacter/LMPlayer/BP_LM_PlayerCharacter.BP_LM_PlayerCharacter_C"));
    UClass* ComboClass = LoadClass<ULMAttackAbility>(nullptr, TEXT("/Game/GAS/GA/GA_LM_PlayerCombo.GA_LM_PlayerCombo_C"));
    if (!TestNotNull(TEXT("Player fixture class"), PlayerClass) || !TestNotNull(TEXT("Combo fixture class"), ComboClass))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }
    ALMPlayerCharacter* PlayerBody = TestWorld->SpawnActor<ALMPlayerCharacter>(PlayerClass);
    ALMPlayerState* State = TestWorld->SpawnActor<ALMPlayerState>();
    UAbilitySystemComponent* ASC = State->GetAbilitySystemComponent();
    PlayerBody->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    PlayerBody->GetMesh()->InitAnim(true);
    ASC->InitAbilityActorInfo(State, PlayerBody);
    ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("LM.Unlock.Weapon.Sword")));
    State->UnlockSpecialAttack();
    const FGameplayAbilitySpecHandle ComboHandle = ASC->GiveAbility(FGameplayAbilitySpec(ComboClass, 1));
    const bool bActivated = ASC->TryActivateAbility(ComboHandle);
    TestTrue(TEXT("Real combo fixture can activate"), bActivated);
    ULMAttackAbility* ComboAbility = Cast<ULMAttackAbility>(ASC->FindAbilitySpecFromHandle(ComboHandle)->GetPrimaryInstance());
    if (!bActivated || !TestNotNull(TEXT("Instanced combo"), ComboAbility))
    {
        TestWorld->DestroyWorld(false);
        return false;
    }

    // 실제 몽타주 실행의 패링 창에서 성공한 첫 타격만 20을 받는다.
    State->TrySpendSpecialCharge();
    UAnimMontage* PlayingMontage = ComboAbility->GetCurrentMontage();
    FAnimMontageInstance* PlayingInstance = PlayerBody->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(PlayingMontage);
    check(PlayingInstance);
    ULMANSDodgeParry* ParryWindow = NewObject<ULMANSDodgeParry>();
    FBranchingPointNotifyPayload ParryPayload(PlayerBody->GetMesh(), PlayingMontage, nullptr, PlayingInstance->GetInstanceID());
    TestFalse(TEXT("A closed parry window cannot grant charge"), ComboAbility->TryParryHit());
    ComboAbility->SetParryWindow(ParryPayload, *ParryWindow, true);
    TestTrue(TEXT("The first valid parry succeeds"), ComboAbility->TryParryHit());
    TestEqual(TEXT("Actual parry success grants exactly twenty"), ASC->GetNumericAttribute(ULMCombatAttributeSet::GetSpecialChargeAttribute()), 20.0f);
    TestFalse(TEXT("The same parry window cannot reward twice"), ComboAbility->TryParryHit());
    TestEqual(TEXT("Rejected parry preserves twenty"), ASC->GetNumericAttribute(ULMCombatAttributeSet::GetSpecialChargeAttribute()), 20.0f);
    ComboAbility->SetParryWindow(ParryPayload, *ParryWindow, false);
    State->AddSpecialCharge(100.0f);

    // UI와 동일한 변경 알림에서 즉시 취소하여 타격 시작·소비 순서를 관찰한다.
    FName SectionOnSpend;
    const FDelegateHandle CancelHandle = State->OnSpecialGaugeChanged.AddLambda([ASC, PlayerBody, ComboAbility, ComboHandle, &SectionOnSpend]()
    {
        if (ASC->GetNumericAttribute(ULMCombatAttributeSet::GetSpecialChargeAttribute()) != 0.0f || !SectionOnSpend.IsNone()) return;
        SectionOnSpend = PlayerBody->GetMesh()->GetAnimInstance()->Montage_GetCurrentSection(ComboAbility->GetCurrentMontage());
        ASC->CancelAbilityHandle(ComboHandle);
    });
    UFunction* StartFunction = ComboAbility->FindFunction(TEXT("TryStartSpecialAttack"));
    FStructOnScope StartArguments(StartFunction);
    FNameProperty* SectionProperty = FindFProperty<FNameProperty>(StartFunction, TEXT("SectionName"));
    check(SectionProperty);
    SectionProperty->SetPropertyValue_InContainer(StartArguments.GetStructMemory(), TEXT("SpecialAttack02"));
    ComboAbility->ProcessEvent(StartFunction, StartArguments.GetStructMemory());
    State->OnSpecialGaugeChanged.Remove(CancelHandle);
    TestEqual(TEXT("First strike has started before spend observers can cancel"), SectionOnSpend, FName(TEXT("SpecialAttack02")));
    TestEqual(TEXT("Cancellation after start keeps the cost"), ASC->GetNumericAttribute(ULMCombatAttributeSet::GetSpecialChargeAttribute()), 0.0f);
    TestFalse(TEXT("Observer cancellation is respected"), ComboAbility->IsActive());
    TestWorld->DestroyWorld(false);
    return true;
}

#endif
