#include "LMCharacter/LMFieldMonsterCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMUI/LMMonsterHealthWidget.h"
#include "TimerManager.h"

/** 몬스터의 실제 체력과 표시 객체를 각 인스턴스에 독립적으로 생성한다. */
ALMFieldMonsterCharacter::ALMFieldMonsterCharacter()
{
    // 첫 전투 연결에서는 고정 표적으로 사용한다.
    AutoPossessAI = EAutoPossessAI::Disabled;

    AbilitySystemComponent = CreateDefaultSubobject<ULMAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(false);
    CombatAttributes = CreateDefaultSubobject<ULMCombatAttributeSet>(TEXT("CombatAttributes"));

    HealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComponent"));
    HealthWidgetComponent->SetupAttachment(GetRootComponent());

    // shortcut: 화면 공간 HP바는 벽에 가려지지 않는다. 최종 표시 정책에서 조정한다.
    HealthWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
    HealthWidgetComponent->SetDrawSize(FVector2D(200.0f, 32.0f));
    HealthWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
    HealthWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));

    // 표시 컴포넌트가 무기 판정의 표적이나 장애물이 되지 않도록 한다.
    HealthWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

/** 기존 Base 초기화 후 피해 결과를 연결하고 초기 체력을 표시한다. */
void ALMFieldMonsterCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeAbilitySystem(this, AbilitySystemComponent.Get());

    const bool bReady = IsValid(AbilitySystemComponent.Get()) && IsValid(CombatAttributes.Get()) && GetAbilitySystemComponent() == AbilitySystemComponent.Get() && AbilitySystemComponent->GetSet<ULMCombatAttributeSet>() == CombatAttributes.Get();

    if (!ensureMsgf(bReady, TEXT("%s: Combat ASC or AttributeSet initialization failed."), *GetName()))
    {
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("[LMCombat] %s Health=%.1f MaxHealth=%.1f"), *GetName(), CombatAttributes->GetHealth(), CombatAttributes->GetMaxHealth());

    // UI 설정이 빠져도 피해 반응 경로는 별도로 준비한다.
    DamageAppliedHandle = CombatAttributes->OnDamageApplied.AddUObject(this, &ALMFieldMonsterCharacter::HandleDamageApplied);

    HealthWidgetComponent->InitWidget();

    if (!ensureMsgf(Cast<ULMMonsterHealthWidget>(HealthWidgetComponent->GetUserWidgetObject()) != nullptr, TEXT("%s: Assign WBP_LM_MonsterHealth."), *GetName()))
    {
        return;
    }

    // 자신의 수치만 구독하고 시작 체력도 즉시 한 번 표시한다.
    HealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetHealthAttribute()).AddUObject(this, &ALMFieldMonsterCharacter::HandleHealthChanged);
    MaxHealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ALMFieldMonsterCharacter::HandleHealthChanged);

    RefreshHealthWidget();
}

/** 체력 관련 변경을 받으면 두 표시값을 함께 갱신한다. */
void ALMFieldMonsterCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    RefreshHealthWidget();
}

/** 실제 AttributeSet 값을 표시 전용 위젯에 전달한다. */
void ALMFieldMonsterCharacter::RefreshHealthWidget()
{
    if (bEndingPlay || !IsValid(CombatAttributes.Get()) || !IsValid(HealthWidgetComponent.Get()))
    {
        return;
    }

    if (ULMMonsterHealthWidget* Widget = Cast<ULMMonsterHealthWidget>(HealthWidgetComponent->GetUserWidgetObject()))
    {
        Widget->UpdateHealth(CombatAttributes->GetHealth(), CombatAttributes->GetMaxHealth());
    }
}

/** 일반 몬스터의 소진은 즉시 제한하고 비치명 피해는 한 번 반응한다. */
void ALMFieldMonsterCharacter::HandleDamageApplied(float ActualDamage, bool _bIsDead)
{
    if (bEndingPlay || bIsDead || !FMath::IsFinite(ActualDamage) || ActualDamage <= 0.0f)
    {
        return;
    }

    if (_bIsDead)
    {
        // 재진입 전에 상태를 확정하고 피격 제한을 풀기 전에 사망 제한을 세운다.
        bIsDead = true;
        bOwnsDeadTag = true;
        AbilitySystemComponent->AddLooseGameplayTag(LMGameplayTags::State_Dead);

        FinishHitReaction();

        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        AbilitySystemComponent->ClearAbilityInput(this);
        AbilitySystemComponent->CancelAbilities();

        // 즉시 파괴하지 않고 AnimBP가 Death_A와 마지막 자세를 표시하게 한다.
        return;
    }

    // 재피격은 HP만 반영하고 이미 진행 중인 반응을 재시작하거나 연장하지 않는다.
    if (bIsHitReacting)
    {
        return;
    }

    if (!IsValid(HitReactionAnimation.Get()) || !IsValid(GetMesh()->GetAnimInstance()) || !FMath::IsFinite(HitReactionAnimation->RateScale) || HitReactionAnimation->RateScale <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Check HitReactionAnimation and Crunch AnimBP."), *GetName());
        return;
    }

    const float Duration = HitReactionAnimation->GetPlayLength() / HitReactionAnimation->RateScale;
    if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
    {
        return;
    }

    // 애니메이션 Notify 누락과 무관하게 제한이 풀리도록 종료 타이머를 먼저 준비한다.
    bIsHitReacting = true;
    GetWorldTimerManager().SetTimer(HitReactionTimer, this, &ALMFieldMonsterCharacter::FinishHitReaction, Duration, false);

    bOwnsStunTag = true;
    AbilitySystemComponent->AddLooseGameplayTag(LMGameplayTags::State_Stunned);

    if (bEndingPlay || bIsDead)
    {
        return;
    }

    GetCharacterMovement()->StopMovementImmediately();
    AbilitySystemComponent->ClearAbilityInput(this);
    AbilitySystemComponent->CancelAbilities();
}

/** 피격 타이머와 자신이 추가한 기절 태그만 해제한다. */
void ALMFieldMonsterCharacter::FinishHitReaction()
{
    GetWorldTimerManager().ClearTimer(HitReactionTimer);
    bIsHitReacting = false;

    if (bOwnsStunTag)
    {
        // 외부 알림이 재진입하더라도 중복 제거하지 않도록 소유 기록부터 비운다.
        bOwnsStunTag = false;

        if (IsValid(AbilitySystemComponent.Get()))
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(LMGameplayTags::State_Stunned);
        }
    }
}

/** 자신의 구독과 태그를 정리하고 기존 Base의 GAS 연결 해제를 실행한다. */
void ALMFieldMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;

    if (IsValid(CombatAttributes.Get()))
    {
        CombatAttributes->OnDamageApplied.Remove(DamageAppliedHandle);
    }

    if (IsValid(AbilitySystemComponent.Get()))
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
    }

    DamageAppliedHandle.Reset();
    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();

    FinishHitReaction();

    // 기존 Avatar 실행을 먼저 정리한 뒤 이 몸체가 추가한 사망 태그를 회수한다.
    Super::EndPlay(EndPlayReason);

    if (bOwnsDeadTag)
    {
        bOwnsDeadTag = false;

        if (IsValid(AbilitySystemComponent.Get()))
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(LMGameplayTags::State_Dead);
        }
    }
}