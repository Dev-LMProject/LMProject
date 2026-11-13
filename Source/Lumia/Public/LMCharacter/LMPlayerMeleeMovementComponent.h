#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LMPlayerMeleeMovementComponent.generated.h"

class ALMBaseCharacter;
class UAnimMontage;
class UGameplayAbility;

/** 지상 캡슐형 적에게 공통으로 적용할 평타 접근과 접촉 정지를 처리한다. */
UCLASS()
class LUMIA_API ULMPlayerMeleeMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    // 전달받은 적을 이번 타의 방향 대상으로 사용한다. true는 대상 수락이며 명중 성공을 뜻하지 않는다.
    bool BeginMeleeApproach(UGameplayAbility* _ApproachOwner, UAnimMontage* _ApproachMontage, ALMBaseCharacter* _ApproachTarget);

    /** 공격 능력이 회복·특수 진입·종료·취소 시 자신이 시작한 접근을 정리한다. */
    void EndMeleeApproach(const UGameplayAbility* Requester);
    
    // 기존 이동 Tick에서 대상 상실을 확인한 뒤 엔진의 이동 처리를 실행한다.
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    /** 엔진이 충돌 후 잔여 이동을 처리할 때 호출하며 지상 평타의 적 접촉만 제한한다. */
    virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

    /** 엔진이 애니메이션 루트 속도를 적용할 때 호출하며 접촉한 타의 XY를 제한한다. */
    virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;
    
    

private:
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

    // 이번 접근 보정을 시작한 공격 GA를 기억한다.
    TWeakObjectPtr<UGameplayAbility> ApproachOwner;

    // 이번 접근 보정에 사용하는 공격 몽타주를 기억한다.
    TWeakObjectPtr<UAnimMontage> ApproachMontage;

    // 공격 GA가 전달한 적을 기억한다. 이동 컴포넌트에서 다른 적을 다시 찾지 않는다.
    TWeakObjectPtr<ALMBaseCharacter> ApproachTarget;

    // 적에게 닿았거나 대상을 잃으면 true로 바꿔 이번 타의 남은 수평 전진을 막는다.
    bool bStopMeleeMove = false;

    /** 현재 Avatar·능력·몽타주가 이 접근을 소유하는지 확인한다. */
    bool OwnsActiveApproach() const;

    /** 공통 표적 정책과 거리·발 높이·전방·벽 조건을 검사한다. */
    bool EvaluateCandidate(ALMBaseCharacter* Candidate, FVector& OutDirection, float& OutTravel) const;
    
    // 대상을 잃었을 때 이전 목적지로 계속 전진하거나 회전하지 않도록 보정을 멈춘다.
    void StopMeleeTargetAssist();
};