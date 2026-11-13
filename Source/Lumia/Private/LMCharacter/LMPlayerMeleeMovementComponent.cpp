#include "LMCharacter/LMPlayerMeleeMovementComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "MotionWarpingComponent.h"
#include "LMComponent/LMTargetComponent.h"
#include "LMPlayer/LMPlayerController.h"

namespace LMMelee
{
    // shortcut: 지상 단일 캡슐형 적 기준이다. 다중 몸체·비행 보스를 지원할 때 표적 형상 조회를 확장한다.
    const FName WarpTarget(TEXT("LM_AttackApproach"));
}

/** 이동 처리 도중 현재 몸체·능력·몽타주가 여전히 이 접근의 소유자인지 확인한다. */
bool ULMPlayerMeleeMovementComponent::OwnsActiveApproach() const
{
    const UGameplayAbility* OwnerAbility = ApproachOwner.Get();
    const UAbilitySystemComponent* ASC = OwnerAbility ? OwnerAbility->GetAbilitySystemComponentFromActorInfo() : nullptr;

    return OwnerAbility && OwnerAbility->IsActive() && ApproachMontage.IsValid() && OwnerAbility->GetAvatarActorFromActorInfo() == CharacterOwner && ASC && ASC->GetAvatarActor() == CharacterOwner && ASC->GetAnimatingAbility() == OwnerAbility && OwnerAbility->GetCurrentMontage() == ApproachMontage.Get();
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
bool ULMPlayerMeleeMovementComponent::BeginMeleeApproach(UGameplayAbility* _ApproachOwner, UAnimMontage* _ApproachMontage, ALMBaseCharacter* _ApproachTarget)
{
    if (!IsValid(_ApproachOwner) || !_ApproachOwner->IsActive() || !IsValid(_ApproachMontage) || _ApproachOwner->GetAvatarActorFromActorInfo() != CharacterOwner)
    {
        return false;
    }

    UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    UMotionWarpingComponent* Warping = CharacterOwner ? CharacterOwner->FindComponentByClass<UMotionWarpingComponent>() : nullptr;

    // 기존 피벗용 MotionWarping 컴포넌트를 재사용한다.
    if (!IsMovingOnGround() || !Capsule || !GetWorld() || !ensureMsgf(Warping, TEXT("LM melee approach requires the existing MotionWarping component.")))
    {
        EndMeleeApproach(_ApproachOwner);
        return false;
    }

    ApproachOwner = _ApproachOwner;
    ApproachMontage = _ApproachMontage;
    ApproachTarget.Reset();

    // 새 타가 시작되면 직전 타의 접촉·대상 상실 정지를 해제한다.
    bStopMeleeMove = false;

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

/** 회복·특수 진입·종료에서 호출되어 요청 능력의 타깃만 제거하고 피벗 타깃은 보존한다. */
void ULMPlayerMeleeMovementComponent::EndMeleeApproach(const UGameplayAbility* Requester)
{
    // 이전 능력의 늦은 종료가 새 능력의 상태를 지우지 못하게 한다.
    if (!Requester || ApproachOwner.Get() != Requester)
    {
        return;
    }

    if (CharacterOwner)
    {
        if (UMotionWarpingComponent* Warping = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>())
        {
            // LM_PivotFacing 등 다른 기능의 타깃은 제거하지 않는다.
            Warping->RemoveWarpTarget(LMMelee::WarpTarget);
        }
    }

    ApproachOwner.Reset();
    ApproachMontage.Reset();
    ApproachTarget.Reset();
    bStopMeleeMove = false;
}

/** 엔진이 충돌 뒤 남은 이동을 처리할 때 적 캡슐에 막힌 지상 평타의 접선 이동만 생략한다. */
float ULMPlayerMeleeMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
    const ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(CharacterOwner);
    const ALMBaseCharacter* Other = Cast<ALMBaseCharacter>(Hit.GetActor());

    if (OwnsActiveApproach() && IsMovingOnGround() && HasAnimRootMotion() && Hit.IsValidBlockingHit() && !IsWalkable(Hit) && Other && Other->IsHostileTo(Attacker) && Hit.GetComponent() == Other->GetCapsuleComponent())
    {
        // 이미 도달한 접촉 위치는 유지하고, 남은 이동을 옆으로 보내지 않는다.
        bStopMeleeMove = true;
        Velocity.X = 0.0f;
        Velocity.Y = 0.0f;
        return 0.0f;
    }

    // 일반 보행·벽·경사·공중 이동과 초기 침투 복구는 기존 엔진 처리를 사용한다.
    return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

/** 엔진의 루트 속도 적용 과정에서 접촉한 타의 XY만 제거하고 Z축 처리는 유지한다. */
FVector ULMPlayerMeleeMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
    FVector Result = Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);

    if (bStopMeleeMove && OwnsActiveApproach() && IsMovingOnGround())
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
    // 명시적으로 비운 참조와, 몬스터 파괴로 무효가 된 참조를 구분한다.
    // 파괴된 참조도 이 검사에 들어와 기존 목적지로의 이동을 멈춰야 한다.
    if (OwnsActiveApproach() && !ApproachTarget.IsExplicitlyNull())
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