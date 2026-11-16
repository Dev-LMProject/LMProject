#include "LMCharacter/LMPlayerMeleeMovementComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "LMComponent/LMTargetComponent.h"
#include "LMPlayer/LMPlayerController.h"

namespace LMMelee
{
    // shortcut: 지상 단일 캡슐형 적 기준이다. 다중 몸체·비행 보스를 지원할 때 표적 형상 조회를 확장한다.
    const FName WarpTarget(TEXT("LM_AttackApproach"));
    const FName HeavyWindupTarget(TEXT("LM_HeavyWindup"));
    const FName HeavyOutTarget(TEXT("LM_HeavyOut"));
    const FName HeavyBackTarget(TEXT("LM_HeavyBack"));
}

// 평타와 왕복 모두 현재 몸체·능력·몽타주가 이번 이동의 소유자인지 확인한다.
bool ULMPlayerMeleeMovementComponent::OwnsAttackMove() const
{
    const UGameplayAbility* OwnerAbility = MoveOwner.Get();
    const UAbilitySystemComponent* ASC = OwnerAbility ? OwnerAbility->GetAbilitySystemComponentFromActorInfo() : nullptr;

    return OwnerAbility && OwnerAbility->IsActive() && MoveMontage.IsValid() && OwnerAbility->GetAvatarActorFromActorInfo() == CharacterOwner && ASC && ASC->GetAvatarActor() == CharacterOwner && ASC->GetAnimatingAbility() == OwnerAbility && OwnerAbility->GetCurrentMontage() == MoveMontage.Get();
}

// 전달된 적이 보정 가능한 대상인지 확인하고 수평 방향만 계산한다.
bool ULMPlayerMeleeMovementComponent::EvaluateCandidate(ALMBaseCharacter* Candidate, FVector& OutDirection) const
{
    OutDirection = FVector::ZeroVector;

    const ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(CharacterOwner);
    const UCapsuleComponent* SelfCapsule = Attacker ? Attacker->GetCapsuleComponent() : nullptr;
    const UCapsuleComponent* OtherCapsule = IsValid(Candidate) ? Candidate->GetCapsuleComponent() : nullptr;
    const ULMTargetComponent* Point = IsValid(Candidate) ? Candidate->FindComponentByClass<ULMTargetComponent>() : nullptr;

    // 죽은 적, 비활성 표적, 캡슐이 없는 대상에는 현재 지상 평타 보정을 적용하지 않는다.
    if (!SelfCapsule || !OtherCapsule || !Point || !Point->CanFocus() || !Candidate->CanBeAttackTargetFor(Attacker) || !GetWorld())
    {
        return false;
    }

    const FVector Start = SelfCapsule->GetComponentLocation();
    const FVector Other = OtherCapsule->GetComponentLocation();

    // 큰 몬스터도 같은 바닥에 서 있으면 허용하도록 캡슐 중심 높이 대신 발 높이를 비교한다.
    const float FootDifference = (Other.Z - OtherCapsule->GetScaledCapsuleHalfHeight()) - (Start.Z - SelfCapsule->GetScaledCapsuleHalfHeight());

    if (FMath::Abs(FootDifference) > MaxFootHeightDifference)
    {
        return false;
    }

    const FVector FlatDelta(Other.X - Start.X, Other.Y - Start.Y, 0.0f);
    const float Distance = FlatDelta.Size();

    if (Distance <= UE_KINDA_SMALL_NUMBER) return false;

    // 몸은 적의 캡슐을 향한다. 머리 등에 둔 카메라 표적점을 이동 목적지로 사용하지 않는다.
    OutDirection = FlatDelta / Distance;

    // 카메라에서는 보여도 플레이어 몸과 적 사이에 벽이 있으면 공격 보정을 허용하지 않는다.
    FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(LMMeleeVisibility), false, CharacterOwner);
    VisibilityQuery.AddIgnoredActor(Candidate);

    TArray<AActor*> AttachedActors;
    CharacterOwner->GetAttachedActors(AttachedActors, true, true);
    VisibilityQuery.AddIgnoredActors(AttachedActors);

    AttachedActors.Reset();
    Candidate->GetAttachedActors(AttachedActors, true, true);
    VisibilityQuery.AddIgnoredActors(AttachedActors);

    if (GetWorld()->LineTraceTestByChannel(Start, Other, ECC_GameTraceChannel3, VisibilityQuery))
    {
        return false;
    }

    return true;
}

// 이번 타의 소유자를 등록하고 선택된 적을 바라볼 회전 목표만 전달한다.
bool ULMPlayerMeleeMovementComponent::BeginMeleeApproach(UGameplayAbility* _MoveOwner, UAnimMontage* _MoveMontage, ALMBaseCharacter* _ApproachTarget)
{
    if (!IsValid(_MoveOwner) || !_MoveOwner->IsActive() || !IsValid(_MoveMontage) || _MoveOwner->GetAvatarActorFromActorInfo() != CharacterOwner)
    {
        return false;
    }

    UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    UMotionWarpingComponent* Warping = CharacterOwner ? CharacterOwner->FindComponentByClass<UMotionWarpingComponent>() : nullptr;

    // 기존 피벗용 MotionWarping 컴포넌트를 재사용한다.
    if (!IsMovingOnGround() || !Capsule || !GetWorld() || !ensureMsgf(Warping, TEXT("LM melee approach requires the existing MotionWarping component.")))
    {
        EndAttackMove(_MoveOwner);
        return false;
    }

    // 직전 이동의 목표·임시 충돌을 정리한 뒤 이번 타의 소유자를 등록한다.
    ResetAttackMove();
    MoveOwner = _MoveOwner;
    MoveMontage = _MoveMontage;
    Warping->RemoveWarpTarget(LMMelee::WarpTarget);

    /* 대상이 없어도 공격 소유권은 유지하고 원본 애니메이션의 전진을 사용한다.
     * 이동 컴포넌트에서 다른 적을 검색하거나 고정 전진 거리를 만들지 않는다. */
    FVector Direction;
    if (!EvaluateCandidate(_ApproachTarget, Direction))
        return false;

    ApproachTarget = _ApproachTarget;
    const FVector WarpLocation = Capsule->GetComponentLocation() - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());

    // 몽타주의 Warp Translation은 끄고 Warp Rotation만 켜서 이 목표의 방향만 사용한다.
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::WarpTarget, WarpLocation, Direction.Rotation());

    return true;
}

// 이전 능력의 늦은 종료가 새 능력의 이동을 지우지 않도록 요청자를 확인한다.
void ULMPlayerMeleeMovementComponent::EndAttackMove(const UGameplayAbility* Requester)
{
    if (!Requester || MoveOwner.Get() != Requester) return;
    ResetAttackMove();
}

// 지상 공격이 구조물이나 적에 막혔을 때 이동 방식에 맞게 접선 이동을 제한한다.
float ULMPlayerMeleeMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
    if (OwnsAttackMove() && IsMovingOnGround() && HasAnimRootMotion() && Hit.IsValidBlockingHit() && !IsWalkable(Hit))
    {
        // 벽 접촉 프레임의 잔여 이동만 버려 다음 프레임의 복귀는 허용한다.
        if (bRoundTripMove) return 0.0f;

        const ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(CharacterOwner);
        const ALMBaseCharacter* Other = Cast<ALMBaseCharacter>(Hit.GetActor());
        if (Other && Other->IsHostileTo(Attacker) && Hit.GetComponent() == Other->GetCapsuleComponent())
        {
            // 이번 충돌의 잔여 이동만 막고 다음 물리 단계의 후퇴는 허용한다.
            Velocity.X = 0.0f;
            Velocity.Y = 0.0f;
            return 0.0f;
        }
    }

    // 일반 보행·벽·경사·공중 이동과 초기 침투 복구는 기존 엔진 처리를 사용한다.
    return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

// 물리 단계의 이동 선분이 적 앞 정지 경계를 통과하면 최초 접점까지 수평 속도를 줄인다.
void ULMPlayerMeleeMovementComponent::ApplyRootMotionToVelocity(float DeltaTime)
{
    // 애니메이션과 엔진이 정한 루트 속도를 먼저 받아 원본 이동량과 Z축 처리를 유지한다.
    Super::ApplyRootMotionToVelocity(DeltaTime);

    // 평타의 정지 간격이 관통·왕복 이동을 막지 않도록 평타 소유 구간에만 적용한다.
    if (DeltaTime <= 0.0f || bRoundTripMove || bPassMove || !HasAnimRootMotion() || !IsMovingOnGround() || !OwnsAttackMove())
        return;

    if (ApproachTarget.IsExplicitlyNull()) return;

    ALMBaseCharacter* Target = ApproachTarget.Get();
    const ALMPlayerController* Controller = CharacterOwner ? Cast<ALMPlayerController>(CharacterOwner->GetController()) : nullptr;
    if (!Target || !Controller || Controller->GetAttackFocusTarget() != Target)
    {
        StopMeleeTargetAssist();
        return;
    }

    const UCapsuleComponent* SelfCapsule = CharacterOwner->GetCapsuleComponent();
    const UCapsuleComponent* OtherCapsule = Target->GetCapsuleComponent();
    if (!SelfCapsule || !OtherCapsule) return;

    const FVector Start = SelfCapsule->GetComponentLocation();
    const FVector Other = OtherCapsule->GetComponentLocation();
    const double FootDifference = (Other.Z - OtherCapsule->GetScaledCapsuleHalfHeight()) - (Start.Z - SelfCapsule->GetScaledCapsuleHalfHeight());
    if (FMath::Abs(FootDifference) > MaxFootHeightDifference)
        return;

    // 실제 스케일이 반영된 캡슐 반경을 사용해 크기가 다른 지상 몬스터도 같은 규칙을 따른다.
    const double StopDistance = static_cast<double>(SelfCapsule->GetScaledCapsuleRadius()) + OtherCapsule->GetScaledCapsuleRadius() + FMath::Max(ContactGap, 0.0f);
    const FVector Move(Velocity.X * DeltaTime, Velocity.Y * DeltaTime, 0.0f);
    const FVector ToTarget(Other.X - Start.X, Other.Y - Start.Y, 0.0f);
    const double MoveLengthSquared = Move.SizeSquared();
    const double Toward = FVector::DotProduct(Move, ToTarget);

    // 이동이 없거나 적에게서 멀어지는 방향이면 후퇴·옆걸음을 그대로 허용한다.
    if (MoveLengthSquared <= 0.0 || Toward <= 0.0) return;

    const double Outside = ToTarget.SizeSquared() - FMath::Square(StopDistance);
    double MoveScale = 0.0;
    if (Outside > 0.0)
    {
        /* 이동 선분과 정지 경계 원의 첫 교점을 구한다.
         * 프레임이 길어 적 반대편까지 넘어가는 이동도 끝점 검사만으로 놓치지 않는다. */
        const double Discriminant = FMath::Square(Toward) - MoveLengthSquared * Outside;
        if (Discriminant <= 0.0) return;
        MoveScale = FMath::Clamp(Outside / (Toward + FMath::Sqrt(Discriminant)), 0.0, 1.0);
    }

    // 이미 간격 안이면 안쪽 이동만 막는다. 영구 정지 플래그를 남기지 않는다.
    Velocity.X *= MoveScale;
    Velocity.Y *= MoveScale;
}

// 기존 이동 처리 전에 공통 공격 대상이 사라졌는지 확인한다.
void ULMPlayerMeleeMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // 종료 알림이 누락되어도 소유권을 잃은 워프와 임시 충돌 응답을 남기지 않는다.
    if (!OwnsAttackMove())
        ResetAttackMove();
    /* 적을 잃으면 평타 보정만 해제한다. 관통·왕복은 시작할 때 정한 목적지를 유지한다.
     * 파괴된 weak 참조도 검사하도록 명시적 null과 구분한다. */
    else if (!bRoundTripMove && !bPassMove && !ApproachTarget.IsExplicitlyNull())
    {
        const ALMPlayerController* Controller = CharacterOwner ? Cast<ALMPlayerController>(CharacterOwner->GetController()) : nullptr;

        if (!ApproachTarget.IsValid() || !Controller || Controller->GetAttackFocusTarget() != ApproachTarget.Get() || !IsMovingOnGround())
        {
            StopMeleeTargetAssist();
        }
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 사라진 적의 회전·거리 보정만 해제하고 애니메이션의 전진·후퇴는 계속 허용한다.
void ULMPlayerMeleeMovementComponent::StopMeleeTargetAssist()
{
    ApproachTarget.Reset();

    if (UMotionWarpingComponent* Warping = CharacterOwner ? CharacterOwner->FindComponentByClass<UMotionWarpingComponent>() : nullptr)
        Warping->RemoveWarpTarget(LMMelee::WarpTarget);
}

// 실제 캡슐과 같은 응답으로 검사하여 앞·뒤의 구조물에 맞춰 이동 거리를 줄인다.
float ULMPlayerMeleeMovementComponent::FindSafeTravel(const FVector& Direction, float Distance) const
{
    const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    UWorld* World = GetWorld();
    if (!Capsule || !World || Distance <= 0.0f) return 0.0f;

    const FVector Start = Capsule->GetComponentLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LMHeavyPath), false, CharacterOwner);
    TArray<AActor*> AttachedActors;
    CharacterOwner->GetAttachedActors(AttachedActors, true, true);
    Query.AddIgnoredActors(AttachedActors);
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    FHitResult Hit;

    if (!World->SweepSingleByChannel(Hit, Start, Start + Direction * Distance, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses))
        return Distance;

    // 걸을 수 있는 바닥은 허용하며, 그 뒤에 있는 실제 벽은 이동 중 캡슐 Sweep으로 막는다.
    if (IsWalkable(Hit)) return Distance;

    // Hit.Distance에는 캡슐 반지름이 이미 반영되어 있으므로 여유 간격만 뺀다.
    return Hit.bStartPenetrating ? 0.0f : FMath::Clamp(Hit.Distance - SweepMargin, 0.0f, Distance);
}

// 준비 후퇴와 전진의 목적지를 각각 제한하고 시작 위치를 복귀 목표로 고정한다.
bool ULMPlayerMeleeMovementComponent::BeginRoundTripMove(UGameplayAbility* _MoveOwner, UAnimMontage* _MoveMontage, const FVector& Direction, float Distance, float WindupDistance)
{
    UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    UMotionWarpingComponent* Warping = CharacterOwner ? CharacterOwner->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
    if (!IsValid(_MoveOwner) || !_MoveOwner->IsActive() || _MoveOwner->GetAvatarActorFromActorInfo() != CharacterOwner || !IsValid(_MoveMontage) || !Capsule || !Capsule->IsQueryCollisionEnabled() || !Warping || !GetWorld() || !IsMovingOnGround() || Direction.ContainsNaN() || !FMath::IsFinite(Distance) || !FMath::IsFinite(WindupDistance))
        return false;

    if ((bRoundTripMove || bPassMove) && OwnsAttackMove() && MoveOwner.Get() != _MoveOwner)
        return false;

    // 워프 구간이 빠진 에셋을 원본 이동으로 재생하지 않도록 시작 전에 계약을 확인한다.
    bool bHasWindup = false;
    bool bHasOut = false;
    bool bHasBack = false;
    for (const FAnimNotifyEvent& Event : _MoveMontage->Notifies)
    {
        const UAnimNotifyState_MotionWarping* State = Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass);
        const URootMotionModifier_Warp* Modifier = State ? Cast<URootMotionModifier_Warp>(State->RootMotionModifier) : nullptr;
        if (!Modifier || !Modifier->bWarpTranslation) continue;
        bHasWindup |= Modifier->WarpTargetName == LMMelee::HeavyWindupTarget;
        bHasOut |= Modifier->WarpTargetName == LMMelee::HeavyOutTarget;
        bHasBack |= Modifier->WarpTargetName == LMMelee::HeavyBackTarget;
    }
    if (!ensureMsgf(bHasWindup && bHasOut && bHasBack, TEXT("%s: Heavy montage requires LM_HeavyWindup, LM_HeavyOut and LM_HeavyBack translation windows."), *GetNameSafe(_MoveMontage)))
        return false;

    ResetAttackMove();
    FVector MoveDirection = Direction.GetSafeNormal2D();
    if (MoveDirection.IsNearlyZero())
        MoveDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

    // 콜백보다 먼저 소유 상태를 세우고 사전 검사와 실제 이동에 동일한 통과 정책을 적용한다.
    MoveOwner = _MoveOwner;
    MoveMontage = _MoveMontage;
    SavedEnemyResponse = Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel4); // LMEnemyBody
    bRoundTripMove = true;
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);

    const float OutTravel = FindSafeTravel(MoveDirection, FMath::Max(Distance, 0.0f));
    const float WindupTravel = FindSafeTravel(-MoveDirection, FMath::Max(WindupDistance, 0.0f));
    const FVector StartFeet = Capsule->GetComponentLocation() - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());
    const FRotator Facing = MoveDirection.Rotation();
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::HeavyWindupTarget, StartFeet - MoveDirection * WindupTravel, Facing);
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::HeavyOutTarget, StartFeet + MoveDirection * OutTravel, Facing);
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::HeavyBackTarget, StartFeet, Facing);
    return true;
}

// 시작할 때 적 방향만 정하고 원본 이동 거리·속도·검 판정은 그대로 사용한다.
bool ULMPlayerMeleeMovementComponent::BeginPassMove(UGameplayAbility* _MoveOwner, UAnimMontage* _MoveMontage, ALMBaseCharacter* Target)
{
    UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    if (!IsValid(_MoveOwner) || !_MoveOwner->IsActive() || _MoveOwner->GetAvatarActorFromActorInfo() != CharacterOwner || !IsValid(_MoveMontage) || !Capsule || !Capsule->IsQueryCollisionEnabled())
        return false;

    if (OwnsAttackMove() && MoveOwner.Get() != _MoveOwner)
        return false;

    ResetAttackMove();
    MoveOwner = _MoveOwner;
    MoveMontage = _MoveMontage;
    SavedEnemyResponse = Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel4);
    bPassMove = true;

    // 타 사이에만 방향을 정한다. 대상이 도중에 죽어도 돌진 중에는 방향을 바꾸지 않는다.
    if (IsValid(Target) && Target->CanBeAttackTargetFor(Cast<ALMBaseCharacter>(CharacterOwner)))
    {
        const FVector Direction = (Target->GetActorLocation() - CharacterOwner->GetActorLocation()).GetSafeNormal2D();
        if (!Direction.IsNearlyZero() && !Direction.ContainsNaN())
            CharacterOwner->SetActorRotation(Direction.Rotation());
    }

    // 회전 콜백에서 취소됐다면 이미 복구한 충돌을 다시 Ignore로 바꾸지 않는다.
    if (MoveOwner.Get() != _MoveOwner || !bPassMove || !_MoveOwner->IsActive())
        return false;
    // 적 몸체만 통과한다. 벽·바닥 충돌과 LMCombatHit 검 타격 응답은 변경하지 않는다.
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);
    return MoveOwner.Get() == _MoveOwner && bPassMove && _MoveOwner->IsActive();
}

// 워프와 소유권을 먼저 비운 뒤 충돌을 복구하고 필요한 경우 엔진의 침투 해소를 요청한다.
void ULMPlayerMeleeMovementComponent::ResetAttackMove(bool bResolveOverlap)
{
    // 소유 객체가 파괴되어도 명시적으로 정리하기 전까지는 복구를 진행한다.
    if (MoveOwner.IsExplicitlyNull()) return;

    const bool bWasRoundTripMove = bRoundTripMove;
    const bool bRestoreCollision = bRoundTripMove || bPassMove;
    // 돌진 중 취소되면 큰 루트 속도가 일반 걷기 관성으로 남지 않게 자신의 속도만 정리한다.
    if (bRestoreCollision && bResolveOverlap && OwnsAttackMove())
        StopMovementImmediately();

    const ECollisionResponse RestoreResponse = SavedEnemyResponse;
    bRoundTripMove = false;
    bPassMove = false;
    MoveOwner.Reset();
    MoveMontage.Reset();
    ApproachTarget.Reset();
    if (!CharacterOwner) return;

    if (UMotionWarpingComponent* Warping = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>())
    {
        // 이 이동이 등록한 목표만 지워 피벗 등 다른 기능의 목표를 보존한다.
        if (bWasRoundTripMove)
        {
            Warping->RemoveWarpTarget(LMMelee::HeavyWindupTarget);
            Warping->RemoveWarpTarget(LMMelee::HeavyOutTarget);
            Warping->RemoveWarpTarget(LMMelee::HeavyBackTarget);
        }
        else
            Warping->RemoveWarpTarget(LMMelee::WarpTarget);
    }

    if (!bRestoreCollision) return;
    UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    if (!Capsule) return;
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel4, RestoreResponse); // LMEnemyBody

    /* 적 내부 취소는 기존 SafeMove로 침투를 해소한다.
     * 충돌 복구 콜백에서 새 공격이 시작됐다면 그 이동을 건드리지 않는다. */
    if (bResolveOverlap && MoveOwner.IsExplicitlyNull() && RestoreResponse == ECR_Block && Capsule->IsQueryCollisionEnabled() && UpdatedComponent && GetWorld())
    {
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(LMHeavyRestore), false, CharacterOwner);
        const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
        const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        if (GetWorld()->OverlapBlockingTestByChannel(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses))
        {
            FHitResult Hit;
            SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, 0.1f), UpdatedComponent->GetComponentQuat(), true, Hit);
        }
    }
}

// 왕복 공격이 장애물을 올라타기로 우회하지 않게 한다.
bool ULMPlayerMeleeMovementComponent::CanStepUp(const FHitResult& Hit) const
{
    if (bRoundTripMove && OwnsAttackMove() && !IsWalkable(Hit))
        return false;
    return Super::CanStepUp(Hit);
}

// 플레이 종료에서는 이동 없이 남은 충돌 응답과 자신이 만든 목표만 복구한다.
void ULMPlayerMeleeMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetAttackMove(false);
    Super::EndPlay(EndPlayReason);
}
