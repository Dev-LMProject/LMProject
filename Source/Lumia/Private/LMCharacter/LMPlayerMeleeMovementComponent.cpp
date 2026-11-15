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

// 전달받은 적의 공격 가능 여부와 지상 접근 조건을 확인하고 방향·전진량을 계산한다.
bool ULMPlayerMeleeMovementComponent::EvaluateCandidate(ALMBaseCharacter* Candidate, FVector& OutDirection, float& OutTravel) const
{
    OutDirection = FVector::ZeroVector;
    OutTravel = 0.0f;

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

    const float StopDistance = SelfCapsule->GetScaledCapsuleRadius() + OtherCapsule->GetScaledCapsuleRadius() + ContactGap;
    const float SurfaceTravel = Distance - StopDistance;

    // 멀리 있는 표식 대상도 바라볼 수는 있지만, 그 적에게 장거리 돌진하지는 않는다.
    // 기존 접근 허용 거리 안일 때만 캡슐 사이의 간격을 줄인다.
    if (SurfaceTravel <= MaxApproachDistance)
        OutTravel = FMath::Max(SurfaceTravel, 0.0f);

    return true;
}

// 이번 타에서 사용할 방향과 짧은 접근 목적지를 한 번 계산해 MotionWarping에 전달한다.
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

    // 직전 이동의 목표·접촉 정지·임시 충돌을 정리한 뒤 이번 타의 소유자를 등록한다.
    ResetAttackMove();
    MoveOwner = _MoveOwner;
    MoveMontage = _MoveMontage;

    FVector Direction;
    float Travel = 0.0f;
    const bool bHasTarget = EvaluateCandidate(_ApproachTarget, Direction, Travel);

    if (bHasTarget)
    {
        ApproachTarget = _ApproachTarget;
    }
    else
    {
        // 전달된 대상이 없거나 사용할 수 없으면 현재 정면으로 공격한다.
        // 다른 적을 찾는 Overlap 검색은 수행하지 않는다.
        Direction = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
        Travel = UntargetedApproachDistance;
    }

    const FVector Start = Capsule->GetComponentLocation();
    FVector Destination = Start + Direction * Travel;

    FCollisionQueryParams SweepQuery(SCENE_QUERY_STAT(LMMeleeApproach), false, CharacterOwner);

    TArray<AActor*> AttachedActors;
    CharacterOwner->GetAttachedActors(AttachedActors, true, true);
    SweepQuery.AddIgnoredActors(AttachedActors);

    FHitResult Hit;
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());

    // 실제 플레이어 캡슐로 경로를 검사해 벽이나 다른 몸체를 뚫는 목적지를 만들지 않는다.
    if (Travel > UE_KINDA_SMALL_NUMBER && GetWorld()->SweepSingleByChannel(Hit, Start, Destination, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, SweepQuery, Responses))
    {
        if (Hit.bStartPenetrating)
        {
            Destination = Start;
            bStopMeleeMove = true;
        }
        else
        {
            const float SafeTravel = FMath::Clamp(Hit.Distance - SweepMargin, 0.0f, Travel);
            Destination = Start + Direction * SafeTravel;
        }
    }

    // 기존 지상 공격과 같은 발 위치 기준이다. 몽타주의 Ignore Z Axis를 켜서 사용한다.
    const FVector WarpLocation = Destination - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());

    // 표적의 Actor 위치가 아니라 접근 목적지와 바라볼 방향을 함께 전달한다.
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::WarpTarget, WarpLocation, Direction.Rotation());

    return bHasTarget;
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
            // 평타는 적 접촉 뒤 남은 전진도 멈춰 몸 옆으로 미끄러지지 않게 한다.
            bStopMeleeMove = true;
            Velocity.X = 0.0f;
            Velocity.Y = 0.0f;
            return 0.0f;
        }
    }

    // 일반 보행·벽·경사·공중 이동과 초기 침투 복구는 기존 엔진 처리를 사용한다.
    return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

/** 엔진의 루트 속도 적용 과정에서 접촉한 타의 XY만 제거하고 Z축 처리는 유지한다. */
FVector ULMPlayerMeleeMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
    FVector Result = Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);

    if (bStopMeleeMove && OwnsAttackMove() && IsMovingOnGround())
    {
        // 접촉 다음 프레임에 애니메이션이 다시 앞으로 미는 것을 막는다.
        Result.X = 0.0f;
        Result.Y = 0.0f;
    }

    return Result;
}

// 기존 이동 처리 전에 공통 공격 대상이 사라졌는지 확인한다.
void ULMPlayerMeleeMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // 종료 알림이 누락되어도 소유권을 잃은 워프와 임시 충돌 응답을 남기지 않는다.
    if (!OwnsAttackMove())
        ResetAttackMove();
    /* 적을 잃으면 평타 접근만 멈춘다. 왕복은 고정된 시작점으로 복귀한다.
     * 파괴된 weak 참조도 검사하도록 명시적 null과 구분한다. */
    else if (!bRoundTripMove && !ApproachTarget.IsExplicitlyNull())
    {
        const ALMPlayerController* Controller = CharacterOwner ? Cast<ALMPlayerController>(CharacterOwner->GetController()) : nullptr;

        if (!ApproachTarget.IsValid() || !Controller || Controller->GetAttackFocusTarget() != ApproachTarget.Get() || !IsMovingOnGround())
        {
            StopMeleeTargetAssist();
        }
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 사라진 적의 이전 위치로 남은 루트 모션이 계속 진행되지 않도록 정지 상태로 바꾼다.
void ULMPlayerMeleeMovementComponent::StopMeleeTargetAssist()
{
    ApproachTarget.Reset();
    bStopMeleeMove = true;

    UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    UMotionWarpingComponent* Warping = CharacterOwner ? CharacterOwner->FindComponentByClass<UMotionWarpingComponent>() : nullptr;

    if (!Capsule || !Warping) return;

    const FVector StopLocation = Capsule->GetComponentLocation() - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());

    // 타깃을 바로 삭제하면 원본 루트 모션의 전진이 다시 나올 수 있어 현재 위치로 바꾼다.
    // 남은 회전 보정도 사라진 적 대신 현재 몸 방향을 목표로 삼는다.
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::WarpTarget, StopLocation, CharacterOwner->GetActorRotation());
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

    /* 걸을 수 있는 경사면은 벽으로 취급하지 않는다.
     * 그 뒤의 구조물은 매 프레임 CharacterMovement의 실제 Sweep이 계속 막는다. */
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

    if (bRoundTripMove && OwnsAttackMove() && MoveOwner.Get() != _MoveOwner)
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

// 워프와 소유권을 먼저 비운 뒤 충돌을 복구하고 필요한 경우 엔진의 침투 해소를 요청한다.
void ULMPlayerMeleeMovementComponent::ResetAttackMove(bool bResolveOverlap)
{
    // 소유 객체가 파괴되어도 명시적으로 정리하기 전까지는 복구를 진행한다.
    if (MoveOwner.IsExplicitlyNull()) return;

    const bool bRestoreCollision = bRoundTripMove;
    // 돌진 중 취소되면 큰 루트 속도가 일반 걷기 관성으로 남지 않게 자신의 속도만 정리한다.
    if (bRestoreCollision && bResolveOverlap && OwnsAttackMove())
        StopMovementImmediately();

    const ECollisionResponse RestoreResponse = SavedEnemyResponse;
    bRoundTripMove = false;
    MoveOwner.Reset();
    MoveMontage.Reset();
    ApproachTarget.Reset();
    bStopMeleeMove = false;
    if (!CharacterOwner) return;

    if (UMotionWarpingComponent* Warping = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>())
    {
        // 이 이동이 등록한 목표만 지워 피벗 등 다른 기능의 목표를 보존한다.
        if (bRestoreCollision)
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

// 왕복 공격의 장애물 접촉을 올라타기로 우회하지 않게 한다.
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
