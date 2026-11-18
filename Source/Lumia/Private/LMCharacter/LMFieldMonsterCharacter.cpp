#include "LMCharacter/LMFieldMonsterCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMUI/LMMonsterHealthWidget.h"
#include "TimerManager.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMCombat/LMCombatArea.h"
#include "AIController.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpec.h"
#include "LMCombat/LMAttackAbility.h"

/** 몬스터의 실제 체력과 표시 객체를 각 인스턴스에 독립적으로 생성한다. */
ALMFieldMonsterCharacter::ALMFieldMonsterCharacter()
{
    // 모든 일반 몬스터가 플레이어의 공통 적대·접근·피해 판정에 포함되도록 한다.
    CombatTeam = ELMCombatTeam::Enemy;
    
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

// 한 몬스터가 서로 다른 두 영역에 동시에 등록되지 않도록 한다.
bool ALMFieldMonsterCharacter::RegisterCombatArea(ALMCombatArea* _CombatArea)
{
    if (!IsValid(_CombatArea)||bIsDead||bEndingPlay||IsActorBeingDestroyed())
    {
        return false;
    }
    if (IsValid(CombatArea)&&CombatArea!=_CombatArea)return false;
    //파괴된 이전 영역의 대상이 새 영역등록에 남지 않도록 함
    if (CombatArea!=_CombatArea)
    {
        TargetPlayer.Reset();
        UpdateCombatFocus();
    }

    CombatArea=_CombatArea;
    return true;
}

// 종료 중인 영역도 실제 발신자가 맞으면 자신의 참조를 정리할 수 있다.
void ALMFieldMonsterCharacter::UnregisterCombatArea(ALMCombatArea* _CombatArea)
{
    if (CombatArea!=_CombatArea) return;

    // 영역 종료도 일반 대상 해제와 동일한 복귀 규칙을 사용한다.
    SetTargetPlayer(_CombatArea, nullptr);
    CombatArea = nullptr;
}
void ALMFieldMonsterCharacter::UpdateCombatFocus()
{
    AAIController* LMAIController = Cast<AAIController>(GetController());
    if (!IsValid(LMAIController)) return;

    ALMPlayerCharacter* FocusPlayer = GetTargetPlayer();
    if (IsValid(FocusPlayer) && !FocusPlayer->IsActorBeingDestroyed() && !bIsDead && !bEndingPlay)
    {
        LMAIController->SetFocus(FocusPlayer, EAIFocusPriority::Gameplay);
        return;
    }

    LMAIController->ClearFocus(EAIFocusPriority::Gameplay);
}
// 소속 영역의 대상 변경을 추격 또는 복귀 상태에 반영한다.
bool ALMFieldMonsterCharacter::SetTargetPlayer(ALMCombatArea* SourceArea, ALMPlayerCharacter* _TargetPlayer)
{
    if (!SourceArea || CombatArea != SourceArea) return false;

    // 중복 해제는 최초 대기를 복귀로 바꾸지 않는다.
    if (!_TargetPlayer)
    {
        if (!TargetPlayer.IsExplicitlyNull() && !bIsDead && !bEndingPlay)
            bReturning = true;

        TargetPlayer.Reset();
        UpdateCombatFocus();
        StopAttack();
        return true;
    }

    if (!IsValid(_TargetPlayer) || !_TargetPlayer->IsPlayerControlled() || bIsDead || bEndingPlay || IsActorBeingDestroyed())
    {
        return false;
    }

    // 복귀 중이라도 새 대상을 전달받으면 추격 상태로 전환한다.
    TargetPlayer = _TargetPlayer;
    UpdateCombatFocus();
    bReturning = false;
    return true;
}

/** 기존 Base 초기화 후 피해 결과를 연결하고 초기 체력을 표시한다. */
void ALMFieldMonsterCharacter::BeginPlay()
{
    // BP 시작 이벤트에서 이동을 요청하기 전에 최초 위치와 방향을 확정한다.
    StartLocation = GetActorLocation();
    StartRotation = GetActorRotation();
    Super::BeginPlay();
    InitializeAbilitySystem(this, AbilitySystemComponent.Get());

    const bool bReady = IsValid(AbilitySystemComponent.Get()) && IsValid(CombatAttributes.Get()) && GetAbilitySystemComponent() == AbilitySystemComponent.Get() && AbilitySystemComponent->GetSet<ULMCombatAttributeSet>() == CombatAttributes.Get();

    if (!ensureMsgf(bReady, TEXT("%s: Combat ASC or AttributeSet initialization failed."), *GetName()))
    {
        return;
    }

    // ASC 초기화 뒤 이 몬스터가 소유할 기본공격을 한 번 부여한다.
    if (HasAuthority() && AttackAbilityClass)
        AttackAbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AttackAbilityClass, 1, INDEX_NONE, this));
    
    UE_LOG(LogTemp, Display, TEXT("[LMCombat] %s Health=%.1f MaxHealth=%.1f"), *GetName(), CombatAttributes->GetHealth(), CombatAttributes->GetMaxHealth());

    // UI 설정이 빠져도 피해 반응 경로는 별도로 준비한다.
    DamageAppliedHandle = CombatAttributes->OnDamageApplied.AddUObject(this, &ALMFieldMonsterCharacter::HandleDamageApplied);

    // 전투 영역이 먼저 대상을 전달했다면 ASC 준비가 끝난 지금 빈 정원을 다시 채운다.
    if (!bEndingPlay && !bIsDead && IsValid(CombatArea.Get()))
        CombatArea->FillAttackSlots();

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
    
    StopAttack();
    if (_bIsDead)
    {
        // 재진입 전에 상태를 확정하고 피격 제한을 풀기 전에 사망 제한을 세운다.
        bIsDead = true;
        UpdateCombatFocus();
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

    // 피격 제한이 모두 해제된 뒤 현재 준비 상태로 다음 공격 차례를 판단한다.
    if (!bEndingPlay && !bIsDead && IsValid(CombatArea.Get()))
        CombatArea->FillAttackSlots();
}

// 실제 활성화 전에 현재 공격권을 보관하고 정확한 GA 종료 관찰을 연결한다.
bool ALMFieldMonsterCharacter::PrepareAttack()
{
    if (bEndingPlay || bAttackRunning || !IsValid(AbilitySystemComponent.Get()))
        return false;

    ALMCombatArea* Area = GetCombatArea();
    int64 Combat = 0;
    int64 Turn = 0;
    int64 Wait = 0;
    bool bStop = false;

    if (!IsValid(Area) || !Area->ReadAttackTurn(this, Combat, Turn, bStop, Wait))
        return false;

    // BP가 실행할 능력과 이번 공격권을 종료 알림보다 먼저 확정한다.
    const TWeakObjectPtr<ALMCombatArea> SavedArea(Area);
    const FGameplayAbilitySpecHandle SavedHandle = AttackAbilityHandle;

    RunArea = SavedArea;
    RunCombatId = Combat;
    RunAttackId = Turn;
    bAttackRunning = true;

    const auto OnEnded = [this, SavedArea, SavedHandle, Combat, Turn](const FAbilityEndedData& Data)
    {
        if (Data.AbilitySpecHandle == SavedHandle)
            FinishAttack(SavedArea, Combat, Turn);
    };

    AttackEndedHandle = AbilitySystemComponent->OnAbilityEnded.AddWeakLambda(this, OnEnded);

    // 이동 중단 이벤트 이후의 현재 상태를 다시 검사한다.
    const ALMPlayerCharacter* Player = GetTargetPlayer();
    const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(SavedHandle);
    const bool bReady = !bIsDead && !bIsHitReacting && !bReturning && IsAliveForCombat() && !IsMovementBlockedByAbilities() && !IsMovementControlledByAbilities();

    if (bStop || !bReady || !IsValid(Player) || !Player->IsAliveForCombat() || !Spec || Spec->IsActive())
    {
        StopAttack();
        return false;
    }

    return true;
}
// 중단 의도를 기록하고 이동을 멈춘 뒤 실제 GA 종료를 기다린다.
void ALMFieldMonsterCharacter::StopAttack()
{
    const bool bHadRun = bAttackRunning;
    const TWeakObjectPtr<ALMCombatArea> SavedArea = bHadRun ? RunArea : TWeakObjectPtr<ALMCombatArea>(GetCombatArea());

    int64 Combat = bHadRun ? RunCombatId : 0;
    int64 Turn = bHadRun ? RunAttackId : 0;
    int64 Wait = 0;
    bool bStop = false;

    if (!bHadRun && (!SavedArea.IsValid() || !SavedArea->ReadAttackTurn(this, Combat, Turn, bStop, Wait)))
        return;

    if (SavedArea.IsValid())
        SavedArea->RequestStopAttackTurn(this, Combat, Turn);

    if (AAIController* LMAIController = Cast<AAIController>(GetController()))
        LMAIController->StopMovement();

    GetCharacterMovement()->StopMovementImmediately();

    // GA 실행 전의 접근만 진행 중이었다면 이동 중단 후 반납한다.
    if (!bHadRun)
    {
        if (SavedArea.IsValid())
            SavedArea->FinishAttackTurn(this, Combat, Turn, !bEndingPlay && !bIsDead && IsAliveForCombat());

        return;
    }

    // 이동 종료 이벤트에서 이미 정리된 실행을 다시 취소하지 않는다.
    if (!bAttackRunning || RunArea != SavedArea || RunCombatId != Combat || RunAttackId != Turn)
        return;

    const FGameplayAbilitySpec* Spec = IsValid(AbilitySystemComponent.Get()) ? AbilitySystemComponent->FindAbilitySpecFromHandle(AttackAbilityHandle) : nullptr;

    if (Spec && Spec->IsActive())
    {
        // 취소 요청만으로 반납하지 않고 실제 종료 알림을 기다린다.
        AbilitySystemComponent->CancelAbilityHandle(AttackAbilityHandle);
        return;
    }

    FinishAttack(SavedArea, Combat, Turn);
}

// 같은 실행의 종료만 반영하고 해당 공격권을 한 번 반납한다.
void ALMFieldMonsterCharacter::FinishAttack(TWeakObjectPtr<ALMCombatArea> _RunArea, int64 _RunCombatId, int64 _RunAttackId)
{
    if (!bAttackRunning || !RunArea.HasSameIndexAndSerialNumber(_RunArea) || RunCombatId != _RunCombatId || RunAttackId != _RunAttackId)
        return;

    if (IsValid(AbilitySystemComponent.Get()))
        AbilitySystemComponent->OnAbilityEnded.Remove(AttackEndedHandle);

    AttackEndedHandle.Reset();

    // 영역이 다음 공격권을 배정하기 전에 이전 실행 기록부터 비운다.
    bAttackRunning = false;
    RunArea.Reset();
    RunCombatId = 0;
    RunAttackId = 0;

    if (ALMCombatArea* Area = _RunArea.Get())
        Area->FinishAttackTurn(this, _RunCombatId, _RunAttackId, !bEndingPlay && !bIsDead && IsAliveForCombat());
}
// 이동과 행동 유지 판정을 함께 제공하고 대상의 자연 소멸도 복귀로 처리한다.
bool ALMFieldMonsterCharacter::ReadMoveData(ALMPlayerCharacter*& _TargetPlayer, FVector& _StartLocation, bool& _bReturning, bool& _bCanMove)
{
    // 명시적으로 해제되기 전에 대상이 파괴되어도 복귀를 시작한다.
    if (!TargetPlayer.IsExplicitlyNull() && (!IsValid(TargetPlayer.Get()) || TargetPlayer->IsActorBeingDestroyed()))
    {
        TargetPlayer.Reset();
        bReturning = !bIsDead && !bEndingPlay;
        UpdateCombatFocus();
        StopAttack();
    }

    // BP와 StateTree가 몬스터의 같은 대상 저장소를 읽도록 한다.
    _TargetPlayer = GetTargetPlayer();
    _StartLocation = StartLocation;
    _bReturning = bReturning;

    // ASC 준비 전이나 사망·경직·능력의 이동 제어 중에는 경로 이동을 막는다.
    _bCanMove = !bEndingPlay && !bIsDead && !bIsHitReacting && IsAliveForCombat() && !IsMovementBlockedByAbilities() && !IsMovementControlledByAbilities();

    // 기존 데이터 갱신 한 번으로 허가 정보도 읽으며 소속이 없으면 이전 값을 남기지 않는다.
    AttackCombatId = 0;
    AttackId = 0;
    bAttackStopping = false;
    WaitId = 0;
    bHasAttackTurn = IsValid(CombatArea.Get()) && CombatArea->ReadAttackTurn(this, AttackCombatId, AttackId, bAttackStopping, WaitId);

    // BP에서는 여러 조건을 다시 조합하지 않고 현재 판정 결과만 읽는다.
    bAttackAllowed = bHasAttackTurn && !bAttackStopping && AttackCombatId == CombatArea->GetCombatId();
    bCanWork = (!bAttackRunning && _bCanMove)
        || (bAttackRunning && bAttackAllowed && IsValid(_TargetPlayer) && !bReturning && !bIsDead && !bIsHitReacting && _TargetPlayer->IsAliveForCombat());

    return IsValid(_TargetPlayer);
}

// 복귀 이동에 성공했고 여전히 대상이 없을 때만 최초 방향과 대기를 복원한다.
void ALMFieldMonsterCharacter::FinishReturn()
{
    if (bEndingPlay || bIsDead || !bReturning) return;

    // 복귀 성공 직후 새 대상이 전달되었다면 새 추격 상태를 보존한다.
    if (IsValid(GetTargetPlayer())) return;

    // Controller의 Yaw가 다음 프레임에 복원한 몸체 방향을 덮지 않도록 함께 맞춘다.
    if (AController* MonsterController = GetController())
        MonsterController->SetControlRotation(StartRotation);

    SetActorRotation(StartRotation);
    bReturning = false;
}

/** 자신의 구독과 태그를 정리하고 기존 Base의 GAS 연결 해제를 실행한다. */
void ALMFieldMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    UpdateCombatFocus();
    StopAttack();
    
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

    // Base가 능력을 취소하는 동안 종료 관찰을 유지한다.
    const TWeakObjectPtr<ALMCombatArea> SavedArea = RunArea;
    const int64 SavedCombat = RunCombatId;
    const int64 SavedTurn = RunAttackId;

    Super::EndPlay(EndPlayReason);

    // Avatar 연결 해제 이후 남은 실행 기록을 정리한다.
    FinishAttack(SavedArea, SavedCombat, SavedTurn);

    if (IsValid(AbilitySystemComponent.Get()))
    {
        AbilitySystemComponent->OnAbilityEnded.Remove(AttackEndedHandle);

        if (AttackAbilityHandle.IsValid())
            AbilitySystemComponent->ClearAbility(AttackAbilityHandle);
    }

    AttackEndedHandle.Reset();
    AttackAbilityHandle = FGameplayAbilitySpecHandle();
    
    if (bOwnsDeadTag)
    {
        bOwnsDeadTag = false;

        if (IsValid(AbilitySystemComponent.Get()))
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(LMGameplayTags::State_Dead);
        }
    }
}
