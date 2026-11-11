#include "LMCharacter/LMPlayerMeleeMovementComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "MotionWarpingComponent.h"

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

/** BeginMeleeApproach가 이전 표적과 검색 후보를 검사할 때 공통 대상 정책과 기하 조건을 적용한다. */
bool ULMPlayerMeleeMovementComponent::EvaluateCandidate(ALMBaseCharacter* Candidate, FVector& OutDirection, float& OutTravel) const
{
    OutDirection = FVector::ZeroVector;
    OutTravel = 0.0f;

    const ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(CharacterOwner);
    const UCapsuleComponent* SelfCapsule = Attacker ? Attacker->GetCapsuleComponent() : nullptr;
    const UCapsuleComponent* OtherCapsule = IsValid(Candidate) ? Candidate->GetCapsuleComponent() : nullptr;

    // 특정 몬스터 클래스 대신 공통 Base의 대상 허용 정책을 사용한다.
    if (!SelfCapsule || !OtherCapsule || !Candidate->CanBeAttackTargetFor(Attacker) || !GetWorld())
    {
        return false;
    }

    const FVector Start = SelfCapsule->GetComponentLocation();
    const FVector Other = OtherCapsule->GetComponentLocation();

    // 키가 큰 보스도 선택하도록 몸 중심이 아니라 발 높이를 비교한다.
    const float FootDifference = (Other.Z - OtherCapsule->GetScaledCapsuleHalfHeight()) - (Start.Z - SelfCapsule->GetScaledCapsuleHalfHeight());

    if (FMath::Abs(FootDifference) > MaxFootHeightDifference)
    {
        return false;
    }

    const FVector FlatDelta(Other.X - Start.X, Other.Y - Start.Y, 0.0f);
    const float Distance = FlatDelta.Size();

    // 중심이 완전히 겹친 상태에서는 방향을 만들 수 없으므로 자동 접근에서 제외한다.
    if (Distance <= UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    OutDirection = FlatDelta / Distance;

    const FVector Forward = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

    // 뒤쪽 적을 향해 평타 중 갑자기 크게 회전하지 않도록 전방 각도를 제한한다.
    if (FVector::DotProduct(Forward, OutDirection) < FMath::Cos(FMath::DegreesToRadians(TargetHalfAngle)))
    {
        return false;
    }

    // 스케일이 반영된 실제 캡슐 반지름을 사용하므로 새 몬스터 크기가 자동 반영된다.
    const float StopDistance = SelfCapsule->GetScaledCapsuleRadius() + OtherCapsule->GetScaledCapsuleRadius() + ContactGap;
    const float SurfaceTravel = Distance - StopDistance;

    if (SurfaceTravel > MaxApproachDistance)
    {
        return false;
    }

    // LMCombatHit는 현재 프로젝트의 3번 Trace Channel이다.
    FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(LMMeleeVisibility), false, CharacterOwner);
    VisibilityQuery.AddIgnoredActor(Candidate);

    // 자신과 대상의 부착 무기가 시야 검사에서 벽으로 취급되지 않게 한다.
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

    // 이미 가까우면 전진은 0이다. 적에게서 강제로 뒤로 물러나지는 않는다.
    OutTravel = FMath::Clamp(SurfaceTravel, 0.0f, MaxApproachDistance);
    return true;
}

/** 첫 평타 재생 전 또는 다음 평타 진입 전에 호출되어 이번 타의 목적지를 한 번 고정한다. */
void ULMPlayerMeleeMovementComponent::BeginMeleeApproach(UGameplayAbility* Requester, UAnimMontage* Montage)
{
    // 현재 플레이어가 실제 수행 중인 능력만 접근을 시작할 수 있다.
    if (!IsValid(Requester) || !Requester->IsActive() || !IsValid(Montage) || Requester->GetAvatarActorFromActorInfo() != CharacterOwner)
    {
        return;
    }

    UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
    UMotionWarpingComponent* Warping = CharacterOwner ? CharacterOwner->FindComponentByClass<UMotionWarpingComponent>() : nullptr;

    // 기존 피벗용 MotionWarping을 재사용하며 두 번째 컴포넌트를 만들지 않는다.
    if (!IsMovingOnGround() || !Capsule || !GetWorld() || !ensureMsgf(Warping, TEXT("LM melee approach requires the existing MotionWarping component.")))
    {
        EndMeleeApproach(Requester);
        return;
    }

    // 다른 능력이 시작한 공격에는 이전 공격의 표적을 넘기지 않는다.
    if (ApproachOwner.Get() != Requester)
    {
        FocusedTarget.Reset();
    }

    ApproachOwner = Requester;
    ApproachMontage = Montage;

    // 새 타에서는 직전 타의 접촉 정지를 해제한다.
    bStoppedByMeleeContact = false;

    FVector Direction;
    float Travel = 0.0f;

    // 같은 공격에서 사용하던 표적이 여전히 유효하면 유지한다.
    if (!EvaluateCandidate(FocusedTarget.Get(), Direction, Travel))
    {
        FocusedTarget.Reset();

        Direction = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
        Travel = UntargetedApproachDistance;

        float BestSurfaceDistance = TNumericLimits<float>::Max();

        // 중심점 거리만 검사하지 않고 검색용 캡슐과 겹치는 적 몸체를 수집한다.
        TArray<FOverlapResult> Overlaps;

        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn);

        FCollisionQueryParams SearchQuery(SCENE_QUERY_STAT(LMMeleeSearch), false, CharacterOwner);

        const float SearchRadius = Capsule->GetScaledCapsuleRadius() + ContactGap + MaxApproachDistance;
        const float SearchHalfHeight = SearchRadius + Capsule->GetScaledCapsuleHalfHeight() + MaxFootHeightDifference;

        GetWorld()->OverlapMultiByObjectType(Overlaps, Capsule->GetComponentLocation(), FQuat::Identity, Objects, FCollisionShape::MakeCapsule(SearchRadius, SearchHalfHeight), SearchQuery);

        for (const FOverlapResult& Overlap : Overlaps)
        {
            ALMBaseCharacter* Candidate = Cast<ALMBaseCharacter>(Overlap.GetActor());

            // 같은 적의 메시 등 여러 컴포넌트를 중복 후보로 사용하지 않는다.
            if (!Candidate || Overlap.GetComponent() != Candidate->GetCapsuleComponent())
            {
                continue;
            }

            FVector CandidateDirection;
            float CandidateTravel = 0.0f;

            if (!EvaluateCandidate(Candidate, CandidateDirection, CandidateTravel))
            {
                continue;
            }

            // 이동량이 모두 0인 근접 후보도 실제 표면 거리로 구분한다.
            const float SurfaceDistance = FVector::Dist2D(Capsule->GetComponentLocation(), Candidate->GetCapsuleComponent()->GetComponentLocation()) - Candidate->GetCapsuleComponent()->GetScaledCapsuleRadius();

            if (SurfaceDistance < BestSurfaceDistance)
            {
                BestSurfaceDistance = SurfaceDistance;
                FocusedTarget = Candidate;
                Direction = CandidateDirection;
                Travel = CandidateTravel;
            }
        }
    }

    const FVector Start = Capsule->GetComponentLocation();
    FVector Destination = Start + Direction * Travel;

    FCollisionQueryParams SweepQuery(SCENE_QUERY_STAT(LMMeleeApproach), false, CharacterOwner);

    TArray<AActor*> AttachedActors;
    CharacterOwner->GetAttachedActors(AttachedActors, true, true);
    SweepQuery.AddIgnoredActors(AttachedActors);

    FHitResult Hit;

    // 이번에는 검색용 크기가 아니라 실제 플레이어 캡슐 크기로 이동 경로를 검사한다.
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());

    if (Travel > UE_KINDA_SMALL_NUMBER && GetWorld()->SweepSingleByChannel(Hit, Start, Destination, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, SweepQuery, Responses))
    {
        if (Hit.bStartPenetrating)
        {
            // 이미 겹친 상태에서는 추가 전진을 만들지 않고 엔진의 침투 복구를 유지한다.
            Destination = Start;
            bStoppedByMeleeContact = true;
        }
        else
        {
            // 벽이나 다른 몸체가 먼저 있으면 그 앞까지로 목적지를 줄인다.
            const float SafeTravel = FMath::Clamp(Hit.Distance - SweepMargin, 0.0f, Travel);
            Destination = Start + Direction * SafeTravel;
        }
    }

    // 표준 Z축 중력의 지상 공격 기준이다. 몽타주에서는 Ignore Z Axis를 켠다.
    const FVector WarpLocation = Destination - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());

    // 이 위치를 매 Tick 갱신하지 않고 이번 타의 고정 목표로 사용한다.
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(LMMelee::WarpTarget, WarpLocation, Direction.Rotation());
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
    FocusedTarget.Reset();
    bStoppedByMeleeContact = false;
}

/** 엔진이 충돌 뒤 남은 이동을 처리할 때 적 캡슐에 막힌 지상 평타의 접선 이동만 생략한다. */
float ULMPlayerMeleeMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
    const ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(CharacterOwner);
    const ALMBaseCharacter* Other = Cast<ALMBaseCharacter>(Hit.GetActor());

    if (OwnsActiveApproach() && IsMovingOnGround() && HasAnimRootMotion() && Hit.IsValidBlockingHit() && !IsWalkable(Hit) && Other && Other->IsHostileTo(Attacker) && Hit.GetComponent() == Other->GetCapsuleComponent())
    {
        // 이미 도달한 접촉 위치는 유지하고, 남은 이동을 옆으로 보내지 않는다.
        bStoppedByMeleeContact = true;
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

    if (bStoppedByMeleeContact && OwnsActiveApproach() && IsMovingOnGround())
    {
        // 접촉 다음 프레임에 애니메이션이 다시 앞으로 미는 것을 막는다.
        Result.X = 0.0f;
        Result.Y = 0.0f;
    }

    return Result;
}