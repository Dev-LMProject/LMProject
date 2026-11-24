#include "LMCharacter/LMFieldMonsterCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
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
        SetSlowFollow(false);
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
    // 패링 후퇴 중에는 발이 뒤로 움직이도록 시작 시점의 시선을 유지한다.
    if (ParryRetreatSourceId != 0 && !bIsDead && !bEndingPlay)
        return;

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

    // 새 대상에게는 이전 추적 속도를 이어받지 않는다.
    if (TargetPlayer.Get() != _TargetPlayer)
        SetSlowFollow(false);

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

// 체력이 소진되면 공격과 이동을 끝내고 사망 상태로 전환한다.
void ALMFieldMonsterCharacter::HandleDamageApplied(float ActualDamage, bool _bIsDead)
{
    if (!_bIsDead || bEndingPlay || bIsDead || !FMath::IsFinite(ActualDamage) || ActualDamage <= 0.0f)
    {
        return;
    }

    StopAttack();

    // 피격 제한을 풀기 전에 사망 상태와 태그를 적용한다.
    bIsDead = true;
    UpdateCombatFocus();
    bOwnsDeadTag = true;
    AbilitySystemComponent->AddLooseGameplayTag(LMGameplayTags::State_Dead);

    FinishHitReaction();

    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    AbilitySystemComponent->ClearAbilityInput(this);
    AbilitySystemComponent->CancelAbilities();
}

// 패링 경직을 시작하고 피격 애니메이션 길이만큼 기다린 뒤 그로기로 전환한다.
bool ALMFieldMonsterCharacter::BeginParryHitReaction()
{
    if (bEndingPlay || bIsDead || bIsHitReacting || bIsGroggy || !IsAliveForCombat())
        return false;

    if (!IsValid(HitReactionAnimation.Get()) || !IsValid(GetMesh()->GetAnimInstance()) || !FMath::IsFinite(HitReactionAnimation->RateScale) || HitReactionAnimation->RateScale <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Check HitReactionAnimation and Crunch AnimBP."), *GetName());
        return false;
    }

    const float Duration = HitReactionAnimation->GetPlayLength() / HitReactionAnimation->RateScale;
    if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
    {
        return false;
    }

    bIsHitReacting = true;
    GetWorldTimerManager().SetTimer(HitReactionTimer, this, &ALMFieldMonsterCharacter::BeginParryGroggy, Duration, false);

    bOwnsStunTag = true;
    AbilitySystemComponent->AddLooseGameplayTag(LMGameplayTags::State_Stunned);

    return !bEndingPlay && !bIsDead && bIsHitReacting && IsAliveForCombat();
}

// 다음 공격권이 배정되기 전에 패링 경직을 걸고 현재 공격을 중단한다.
void ALMFieldMonsterCharacter::ReceiveParryReaction(ALMBaseCharacter* Parrier)
{
    if (!HasAuthority() || !IsValid(Parrier)) return;

    FVector BackDirection = (GetActorLocation() - Parrier->GetActorLocation()).GetSafeNormal2D();
    if (BackDirection.IsNearlyZero())
        BackDirection = Parrier->GetActorForwardVector().GetSafeNormal2D();

    if (!BeginParryHitReaction()) return;

    StopAttack();
    if (AAIController* AIController = Cast<AAIController>(GetController()))
        AIController->StopMovement();

    if (bEndingPlay || !bIsHitReacting || !IsAliveForCombat())
        return;

    GetCharacterMovement()->StopMovementImmediately();
    AbilitySystemComponent->ClearAbilityInput(this);

    // 동기 공격 종료가 여기까지 끝났으면 즉시, 지연됐으면 FinishAttack에서 시작한다.
    PendingParryRetreatDirection = BackDirection;
    TryStartParryRetreat();
}

// 남은 경직 시간과 지정 거리로 이 몬스터의 후퇴 이동을 한 번 적용한다.
void ALMFieldMonsterCharacter::TryStartParryRetreat()
{
    if (bAttackRunning || PendingParryRetreatDirection.IsNearlyZero())
        return;

    // 같은 요청을 공격 종료 분기와 직접 호출 양쪽에서 적용하지 않도록 소비한다.
    const FVector BackDirection = PendingParryRetreatDirection;
    PendingParryRetreatDirection = FVector::ZeroVector;
    if (bEndingPlay || !bIsHitReacting || ParryRetreatSourceId != 0 || !IsAliveForCombat())
        return;

    UCharacterMovementComponent* MoveComponent = GetCharacterMovement();
    if (!MoveComponent->IsMovingOnGround() || IsMovementControlledByAbilities() || MoveComponent->HasRootMotionSources())
        return;

    const float TimeLeft = GetWorldTimerManager().GetTimerRemaining(HitReactionTimer);
    const float WalkSpeed = MoveComponent->MaxWalkSpeed;
    if (!FMath::IsFinite(TimeLeft) || TimeLeft <= 0.0f || !FMath::IsFinite(ParryRetreatDistance) || ParryRetreatDistance <= 0.0f || !FMath::IsFinite(WalkSpeed) || WalkSpeed <= 0.0f)
        return;

    // 공격 종료가 늦어져도 보행 속도 안에서 발을 디디며 물러난다.
    const float BackSpeed = FMath::Min(ParryRetreatDistance / TimeLeft, WalkSpeed);
    const TSharedPtr<FRootMotionSource_ConstantForce> Retreat = MakeShared<FRootMotionSource_ConstantForce>();
    Retreat->InstanceName = FName(TEXT("ParryRetreat"));
    Retreat->AccumulateMode = ERootMotionAccumulateMode::Additive;
    Retreat->Force = BackDirection * BackSpeed;
    Retreat->Duration = TimeLeft;

    // 마지막 틱은 남은 시간만 적용하고 종료 시 수평 잔속도를 없앤다.
    Retreat->Settings.UnSetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
    Retreat->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
    Retreat->FinishVelocityParams.ClampVelocity = 0.0f;
    ParryRetreatSourceId = MoveComponent->ApplyRootMotionSource(Retreat);
    if (ParryRetreatSourceId == 0) return;

    // 몸체 기준 이동 방향이 뒤쪽으로 계산되도록 패링한 쪽을 바라본다.
    SetActorRotation((-BackDirection).Rotation());
    if (AAIController* AIController = Cast<AAIController>(GetController()))
        AIController->SetFocalPoint(GetActorLocation() - BackDirection * 100.0f, EAIFocusPriority::Gameplay);
}

// 이 반응이 등록한 이동 소스의 수명이 남아 있는 동안만 뒷걸음 자세를 사용한다.
bool ALMFieldMonsterCharacter::IsParryRetreating() const
{
    if (ParryRetreatSourceId == 0) return false;

    const TSharedPtr<FRootMotionSource> Retreat = GetCharacterMovement()->GetRootMotionSourceByID(ParryRetreatSourceId);
    return Retreat.IsValid() && !Retreat->Status.HasFlag(ERootMotionSourceStatusFlags::Finished) && !Retreat->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval);
}

// AnimBP에 그로기 모션을 선택할 상태를 제공한다.
bool ALMFieldMonsterCharacter::IsGroggy() const
{
    return bIsGroggy;
}

// 후퇴 요청과 이 반응이 등록한 이동만 제거하고 원래 시선을 복구한다.
void ALMFieldMonsterCharacter::StopParryRetreat()
{
    // 남은 공격 종료 알림이 지난 패링의 이동을 시작하지 않도록 요청부터 비운다.
    PendingParryRetreatDirection = FVector::ZeroVector;
    const uint16 RetreatSourceId = ParryRetreatSourceId;
    ParryRetreatSourceId = 0;
    if (RetreatSourceId != 0)
    {
        GetCharacterMovement()->RemoveRootMotionSourceByID(RetreatSourceId);
        UpdateCombatFocus();
    }
}

// 피격 모션에서 그로기로 전환하고 같은 핸들로 최종 경직 해제를 예약한다.
void ALMFieldMonsterCharacter::BeginParryGroggy()
{
    if (bEndingPlay || bIsDead || !bIsHitReacting || !IsAliveForCombat() || !FMath::IsFinite(ParryGroggyDuration) || ParryGroggyDuration <= 0.0f)
    {
        FinishHitReaction();
        return;
    }

    bIsHitReacting = false;
    bIsGroggy = true;

    // 새 공격권이 열리지 않도록 지금 소유한 기절 태그를 그로기 끝까지 유지한다.
    GetWorldTimerManager().SetTimer(HitReactionTimer, this, &ALMFieldMonsterCharacter::FinishHitReaction, ParryGroggyDuration, false);
    StopParryRetreat();
}

// 반응 종료나 사망 때 후퇴와 그로기 상태를 정리하고 소유한 경직만 해제한다.
void ALMFieldMonsterCharacter::FinishHitReaction()
{
    StopParryRetreat();
    GetWorldTimerManager().ClearTimer(HitReactionTimer);
    bIsHitReacting = false;
    bIsGroggy = false;

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

// 공격을 허용하는 수평 반경을 실제로 벗어났는지 확인한다.
bool ALMFieldMonsterCharacter::IsOutsideAttackDistance() const
{
    const ALMPlayerCharacter* Player = GetTargetPlayer();
    if (!IsValid(Player) || Player->IsActorBeingDestroyed() || !FMath::IsFinite(AttackDistance) || AttackDistance <= 0.0f)
        return false;

    const FVector SelfCenter = GetCapsuleComponent()->GetComponentLocation();
    const FVector PlayerCenter = Player->GetCapsuleComponent()->GetComponentLocation();
    return FVector::DistSquared2D(SelfCenter, PlayerCenter) > FMath::Square(AttackDistance);
}

// 대기 이동에 들어갈 때의 속도를 기억해 이후 공격이나 복귀에 남기지 않는다.
void ALMFieldMonsterCharacter::SetSlowFollow(bool _bSlowFollowing)
{
    if (!_bSlowFollowing)
    {
        // 다음 대기에서 이전 교전이나 공격 직전의 원호를 이어 쓰지 않는다.
        bClosingWaitDistance = false;
        bWaitArcRight = true;
        WaitArcOffset = FVector::ZeroVector;
    }

    if (bSlowFollowing == _bSlowFollowing) return;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!IsValid(Movement)) return;

    if (_bSlowFollowing)
    {
        // 대기 이동 진입 시의 속도 상한을 저장해 Reader가 속도 배율을 누적하지 않게 한다.
        SavedWalkSpeed = Movement->MaxWalkSpeed;
        Movement->MaxWalkSpeed = SavedWalkSpeed * FMath::Clamp(FollowSpeedScale, 0.01f, 1.0f);
    }
    else
        Movement->MaxWalkSpeed = SavedWalkSpeed;

    bSlowFollowing = _bSlowFollowing;
}

// 멀어지면 저속 추적하고 가까워지면 플레이어를 중심으로 원호를 왕복한다.
bool ALMFieldMonsterCharacter::ReadWaitMoveLocation(const ALMCombatArea& QueueArea, const ALMPlayerCharacter& CombatPlayer, FVector& Location)
{
    const FVector PlayerCenter = CombatPlayer.GetCapsuleComponent()->GetComponentLocation();
    const FVector Offset = GetCapsuleComponent()->GetComponentLocation() - PlayerCenter;
    const double Distance = Offset.Size2D();
    const float MinimumRadius = AttackDistance + RetreatClearance;
    const float StopDistance = FMath::Max(QueueArea.WaitDistance, MinimumRadius);
    const float StartDistance = FMath::Max(QueueArea.FollowDistance, StopDistance);

    // 거리 보정과 원호 이동이 서로 다른 두 임계값을 사용해 경계에서 반복 전환하지 않는다.
    bClosingWaitDistance = Distance > (bClosingWaitDistance ? StopDistance : StartDistance);
    if (bClosingWaitDistance)
    {
        WaitArcOffset = FVector::ZeroVector;
        Location = CombatPlayer.GetNavAgentLocation();
        return true;
    }

    // 플레이어가 접근해 원호를 유지할 수 없으면 공격 반경 바깥부터 확보한다.
    if (Distance <= AttackDistance)
    {
        WaitArcOffset = FVector::ZeroVector;
        return QueueArea.ReadRetreatLocation(*this, CombatPlayer, AttackDistance, RetreatClearance, Location);
    }

    // 반경과 기준 방향을 저장해 옆걸음마다 안쪽으로 말려들거나 무한히 한 바퀴 돌지 않게 한다.
    if (WaitArcOffset.IsNearlyZero())
    {
        const double Radius = FMath::Clamp(Distance, static_cast<double>(MinimumRadius), static_cast<double>(StopDistance));
        WaitArcOffset = Offset.GetSafeNormal2D() * Radius;
    }

    if (QueueArea.ReadWaitArcLocation(*this, PlayerCenter, WaitArcOffset, AttackDistance, bWaitArcRight, Location))
        return true;

    return Distance < MinimumRadius && QueueArea.ReadRetreatLocation(*this, CombatPlayer, AttackDistance, RetreatClearance, Location);
}

// 캡슐 중심 거리와 발 높이를 확인하고 몸체 사이를 막는 구조물이 있는지 검사한다.
bool ALMFieldMonsterCharacter::CanStartAttack() const
{
    const ALMPlayerCharacter* AttackPlayer = GetTargetPlayer();
    const UWorld* AttackWorld = GetWorld();
    if (!IsValid(AttackPlayer) || AttackPlayer->IsActorBeingDestroyed() || !AttackPlayer->IsAliveForCombat() || !AttackWorld)
        return false;
    if (!FMath::IsFinite(AttackDistance) || AttackDistance <= 0.0f || !FMath::IsFinite(AttackHeight) || AttackHeight < 0.0f)
        return false;

    const UCapsuleComponent* SelfCapsule = GetCapsuleComponent();
    const UCapsuleComponent* PlayerCapsule = AttackPlayer->GetCapsuleComponent();
    if (!IsValid(SelfCapsule) || !IsValid(PlayerCapsule))
        return false;

    const FVector SelfCenter = SelfCapsule->GetComponentLocation();
    const FVector PlayerCenter = PlayerCapsule->GetComponentLocation();

    // 공격 거리는 수평으로 비교하고 키가 다른 캐릭터의 높이는 발 위치로 비교한다.
    if (FVector::DistSquared2D(SelfCenter, PlayerCenter) > FMath::Square(AttackDistance))
        return false;

    const double SelfFoot = SelfCenter.Z - SelfCapsule->GetScaledCapsuleHalfHeight();
    const double PlayerFoot = PlayerCenter.Z - PlayerCapsule->GetScaledCapsuleHalfHeight();
    if (FMath::Abs(SelfFoot - PlayerFoot) > AttackHeight)
        return false;

    // 기존 지상 평타와 같은 LMCombatHit 채널로 검사하고 양쪽 무기·첨부물은 무시한다.
    FCollisionQueryParams WallQuery(SCENE_QUERY_STAT(LMMonsterAttackStart), false, this);
    WallQuery.AddIgnoredActor(AttackPlayer);

    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors, true, true);
    WallQuery.AddIgnoredActors(AttachedActors);
    AttachedActors.Reset();
    AttackPlayer->GetAttachedActors(AttachedActors, true, true);
    WallQuery.AddIgnoredActors(AttachedActors);

    return !AttackWorld->LineTraceTestByChannel(SelfCenter, PlayerCenter, ECC_GameTraceChannel3, WallQuery);
}

// 현재 공격권과 사거리를 확인하고 정확한 GA 종료 관찰을 연결한다.
bool ALMFieldMonsterCharacter::PrepareAttack()
{
    // 이미 실행 중인 공격은 기존 종료 콜백이 정리한다.
    if (bAttackRunning) return false;

    // 실행을 준비할 수 없으면 접근 중에 받은 공격권도 정리한다.
    if (bEndingPlay || !IsValid(AbilitySystemComponent.Get()))
    {
        StopAttack();
        return false;
    }
    
    ALMCombatArea* Area = GetCombatArea();
    int64 Combat = 0;
    int64 Turn = 0;
    int64 Wait = 0;
    bool bStop = false;

    if (!IsValid(Area) || !Area->ReadAttackTurn(this, Combat, Turn, bStop, Wait))
        return false;

    const FGameplayAbilitySpecHandle SavedHandle = AttackAbilityHandle;
    const ALMPlayerCharacter* Player = GetTargetPlayer();
    const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(SavedHandle);
    const bool bReady = !bIsDead && !bIsHitReacting && !bReturning && IsAliveForCombat()
        && !IsMovementBlockedByAbilities() && !IsMovementControlledByAbilities();
    const bool bCanPrepare = !bStop && Combat == Area->GetCombatId() && bReady
        && IsValid(Player) && Player->IsAliveForCombat() && Spec && !Spec->IsActive();

    // 시전 직전에 사거리가 벌어지면 현재 공격권으로 다시 접근한다.
    if (bCanPrepare && IsOutsideAttackDistance())
        return false;

    // BP가 실행할 능력과 이번 공격권을 종료 알림보다 먼저 확정한다.
    SetSlowFollow(false);
    const TWeakObjectPtr<ALMCombatArea> SavedArea(Area);

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

    // 나머지 시작 조건을 만족하지 못하면 기록해 둔 실행과 공격권을 정리한다.
    if (!bCanPrepare || !CanStartAttack())
    {
        StopAttack();
        return false;
    }

    // 기본 몽타주 Notify가 전달되는 메시의 주 애님 인스턴스를 기억한다.
    const FGameplayAbilityActorInfo* AttackActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
    USkeletalMeshComponent* AttackMesh = AttackActorInfo ? AttackActorInfo->SkeletalMeshComponent.Get() : nullptr;
    UAnimInstance* Anim = AttackMesh ? AttackMesh->GetAnimInstance() : nullptr;
    if (IsValid(Anim))
    {
        AttackNotifyAnim = Anim;
        Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::HandleAttackSectionNotify);
    }

    return true;
}

// 이번 평타 몽타주의 섹션 종료 직전에 다음 타를 이어갈 수 있는지 확인한다.
void ALMFieldMonsterCharacter::HandleAttackSectionNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
    static const FName CheckNextAttack(TEXT("LM_CheckNextAttack"));
    if (NotifyName != CheckNextAttack || !bAttackRunning || !IsValid(AbilitySystemComponent.Get()))
        return;

    const UGameplayAbility* Ability = AbilitySystemComponent->GetAnimatingAbility();
    if (!IsValid(Ability) || !Ability->IsActive() || Ability->GetCurrentAbilitySpecHandle() != AttackAbilityHandle)
        return;

    // 현재 몽타주를 재생한 애님 인스턴스와 이번 재생 ID가 모두 일치해야 한다.
    const FGameplayAbilityActorInfo* AttackActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
    const UAnimInstance* Anim = AttackActorInfo ? AttackActorInfo->GetAnimInstance() : nullptr;
    const UAnimMontage* Montage = AbilitySystemComponent->GetCurrentMontage();
    if (!IsValid(Anim) || !IsValid(Montage) || Payload.SkelMeshComponent != Anim->GetSkelMeshComponent() || Payload.SequenceAsset != Montage)
        return;

    const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
    if (!Instance || !Instance->IsActive() || Instance->GetInstanceID() != Payload.MontageInstanceID)
        return;

    const ALMPlayerCharacter* Player = GetTargetPlayer();
    if (IsValid(Player) && Player->IsAliveForCombat() && !IsOutsideAttackDistance())
        return;

    // 현재 섹션의 남은 동작을 재생한 뒤 몽타주를 끝낸다.
    const FName Section = Instance->GetCurrentSection();
    if (!Section.IsNone())
        AbilitySystemComponent->CurrentMontageSetNextSectionName(Section, NAME_None);
}

// 중단 의도를 기록하고 이동을 멈춘 뒤 실제 GA 종료를 기다린다.
void ALMFieldMonsterCharacter::StopAttack()
{
    SetSlowFollow(false);
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

    // 이번 평타에서 등록한 Notify를 공격권 반환 전에 해제한다.
    if (UAnimInstance* Anim = AttackNotifyAnim.Get())
        Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::HandleAttackSectionNotify);
    AttackNotifyAnim.Reset();

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

    // 실제 공격 종료 알림을 받은 시점에 기다리던 패링 후퇴를 시작한다.
    TryStartParryRetreat();
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

    // Reader가 이동 출력을 읽기 전에 공격이나 복귀에는 이전 추적 속도를 복원한다.
    if (bHasAttackTurn || !_bCanMove || bReturning || !IsValid(_TargetPlayer))
        SetSlowFollow(false);

    // BP에서는 여러 조건을 다시 조합하지 않고 현재 판정 결과만 읽는다.
    bAttackAllowed = bHasAttackTurn && !bAttackStopping && AttackCombatId == CombatArea->GetCombatId();

    bCanWork = (!bAttackRunning && _bCanMove)
        || (bAttackRunning && bAttackAllowed && IsValid(_TargetPlayer)
            && !bReturning && !bIsDead && !bIsHitReacting
            && _TargetPlayer->IsAliveForCombat());

    return IsValid(_TargetPlayer);
}

// 공격 접근, 반납 직후의 이격, 저속 추적과 원호 이동을 기존 Reader 출력으로 전달한다.
bool ALMFieldMonsterCharacter::ReadCombatMoveData(FVector& CombatLocation, double& CombatDistance, bool& bAtCombatLocation, bool& bNeedMove)
{
    // 조회 실패 시 이전 위치와 도착 결과가 남지 않도록 초기화한다.
    CombatLocation = FVector::ZeroVector;
    CombatDistance = 0.0;
    bAtCombatLocation = false;
    bNeedMove = false;

    ALMCombatArea* QueueArea = GetCombatArea();
    if (!IsValid(QueueArea))
    {
        SetSlowFollow(false);
        return false;
    }

    // 보고 중 새 공격권이 배정되어도 이번 조회의 판정과 번호는 유지한다.
    const int64 SavedCombat = QueueArea->GetCombatId();
    const int64 SavedWait = WaitId;
    const bool bHasTurn = bHasAttackTurn;
    const bool bAllowed = bAttackAllowed;
    const bool bMoveReady = !bAttackRunning && bCanWork;
    const ALMPlayerCharacter* CombatPlayer = GetTargetPlayer();
    const bool bHasTarget = IsValid(CombatPlayer) && !CombatPlayer->IsActorBeingDestroyed();

    // 공격 실행 중이거나 중단 중인 공격권을 가진 경우에는 이동 위치를 요청하지 않는다.
    const bool bCanReadPlace = bMoveReady && bHasTarget && !bReturning && !IsActorBeingDestroyed() && (!bHasTurn || bAllowed);
    if (!bCanReadPlace)
    {
        SetSlowFollow(false);
        return false;
    }

    FVector MoveLocation = FVector::ZeroVector;
    if (!QueueArea->ReadCombatLocation(this, bAllowed, MoveLocation))
    {
        SetSlowFollow(false);
        // 접근할 대상을 잃은 공격권은 기존 공격 종료 경로로 반납한다.
        if (bAllowed)
            StopAttack();
        return false;
    }

    const FVector SelfLocation = GetNavAgentLocation();
    bool bWaiting = false;
    bool bRetreated = false;
    if (bAllowed)
    {
        // 공격권이 있으면 1단위의 사거리·발 높이·차폐 판정으로 공격 진입을 결정한다.
        bAtCombatLocation = CanStartAttack();
    }
    else if (QueueArea->NeedsRetreat(this, SavedWait))
    {
        // 공격권을 반납한 위치에서 실제로 이동하고 간격을 확보했는지 확인한다.
        bRetreated = QueueArea->CanFinishRetreat(*this, SavedWait);
        if (bRetreated)
        {
            // 실제 후퇴가 끝나면 도착 오차를 보정하려고 잡았던 목표를 해제한다.
            MoveLocation = SelfLocation;
            bAtCombatLocation = true;
        }
        else if (!QueueArea->ReadAttackRetreatLocation(*this, *CombatPlayer, AttackDistance, RetreatClearance, SavedWait, MoveLocation))
        {
            SetSlowFollow(false);
            return false;
        }
    }
    else
    {
        bWaiting = true;
        if (!ReadWaitMoveLocation(*QueueArea, *CombatPlayer, MoveLocation))
        {
            // 양옆이 막히면 기존 이동을 끝내고 다음 Reader 갱신에서 다시 길을 읽는다.
            MoveLocation = SelfLocation;
            bAtCombatLocation = true;
        }
    }

    SetSlowFollow(bWaiting);
    CombatLocation = MoveLocation;
    CombatDistance = FVector::Dist2D(SelfLocation, MoveLocation);
    bNeedMove = !bAtCombatLocation;

    // 출력 확정 뒤 이격을 보고해 재발급된 공격권이 이번 대기 출력에 섞이지 않게 한다.
    if (bRetreated)
        QueueArea->ReportWaitReady(this, SavedCombat, SavedWait, true);
    else
        QueueArea->FillAttackSlots();

    return true;
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
