#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMPlayerMeleeMovementComponent.generated.h"

class ALMBaseCharacter;
class UAnimMontage;
class UGameplayAbility;

// 평타 접근과 강공격 왕복 이동을 기존 캐릭터 충돌에 맞춰 처리한다.
UCLASS()
class LUMIA_API ULMPlayerMeleeMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    // 전달받은 적을 이번 타의 방향 대상으로 사용한다. true는 대상 수락이며 명중 성공을 뜻하지 않는다.
    bool BeginMeleeApproach(UGameplayAbility* _MoveOwner, UAnimMontage* _MoveMontage, ALMBaseCharacter* _ApproachTarget);

    // 적 몸체를 통과하는 왕복 공격의 안전한 준비·전진·복귀 목표를 등록한다.
    bool BeginRoundTripMove(UGameplayAbility* _MoveOwner, UAnimMontage* _MoveMontage, const FVector& Direction, float Distance, float WindupDistance);

    // 자신이 시작한 공격 이동을 종료하고 왕복 이동이었다면 몸체 충돌도 복구한다.
    void EndAttackMove(const UGameplayAbility* Requester);
    
    // 공격 이동의 소유권과 대상 상실을 확인한 뒤 엔진의 이동 처리를 실행한다.
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    // 몸체가 사라져도 임시 충돌 응답과 워프 목표를 남기지 않는다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 왕복 공격 중 새로 나타난 구조물을 계단처럼 올라타지 않는다.
    virtual bool CanStepUp(const FHitResult& Hit) const override;

    // 평타의 적 접촉과 강공격의 벽 접촉에서 불필요한 접선 이동을 제한한다.
    virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

    /** 엔진이 애니메이션 루트 속도를 적용할 때 호출하며 접촉한 타의 XY를 제한한다. */
    virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;
    
    

private:
    // 평타·왕복 중 하나만 이동을 소유하며 능력과 몽타주의 수명은 연장하지 않는다.
    TWeakObjectPtr<UGameplayAbility> MoveOwner;
    TWeakObjectPtr<UAnimMontage> MoveMontage;
    bool bRoundTripMove = false;
    ECollisionResponse SavedEnemyResponse = ECR_Block;

    // 현재 공격 이동의 능력·Avatar·몽타주 연결이 아직 유효한지 확인한다.
    bool OwnsAttackMove() const;

    // 실제 캡슐과 현재 충돌 응답으로 한 방향의 안전 이동 거리를 구한다.
    float FindSafeTravel(const FVector& Direction, float Distance) const;

    // 소유자가 이미 무효여도 이 컴포넌트가 변경한 상태를 정확히 한 번 복구한다.
    void ResetAttackMove(bool bResolveOverlap = true);

    // 이보다 많이 걸어가야 하는 적은 이번 타의 자동 접근 대상에서 제외한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxApproachDistance = 40.0f;

    // 두 캡슐 표면 사이에 남길 거리이며 적의 반지름은 별도로 자동 반영한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float ContactGap = 15.0f;

    // 대상이 없을 때의 전진량이다. 최초 검증에서는 0으로 두어 대상 선택을 구분한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float UntargetedApproachDistance = 0.0f;

    // 키가 다른 적도 선택하도록 몸 중심이 아닌 캡슐 바닥 높이 차이를 검사한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxFootHeightDifference = 80.0f;

    // 장애물에 정확히 닿는 지점보다 조금 앞에 목적지를 둔다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float SweepMargin = 2.0f;

    // 공격 GA가 전달한 적을 기억한다. 이동 컴포넌트에서 다른 적을 다시 찾지 않는다.
    TWeakObjectPtr<ALMBaseCharacter> ApproachTarget;

    // 적에게 닿았거나 대상을 잃으면 true로 바꿔 이번 타의 남은 수평 전진을 막는다.
    bool bStopMeleeMove = false;

    /** 공통 표적 정책과 거리·발 높이·전방·벽 조건을 검사한다. */
    bool EvaluateCandidate(ALMBaseCharacter* Candidate, FVector& OutDirection, float& OutTravel) const;
    
    // 대상을 잃었을 때 이전 목적지로 계속 전진하거나 회전하지 않도록 보정을 멈춘다.
    void StopMeleeTargetAssist();
};
