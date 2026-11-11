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
    /** 공격 능력이 첫 평타 재생 전과 다음 평타 섹션 진입 전에 호출한다. */
    void BeginMeleeApproach(UGameplayAbility* Requester, UAnimMontage* Montage);

    /** 공격 능력이 회복·특수 진입·종료·취소 시 자신이 시작한 접근을 정리한다. */
    void EndMeleeApproach(const UGameplayAbility* Requester);

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

    // 카메라가 아닌 캐릭터 정면을 기준으로 좌우 각각 허용하는 각도다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
    float TargetHalfAngle = 60.0f;

    // 대상이 없을 때의 전진량이다. 최초 검증에서는 0으로 두어 대상 선택을 구분한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float UntargetedApproachDistance = 0.0f;

    // 키가 다른 적도 선택하도록 몸 중심이 아닌 캡슐 바닥 높이 차이를 검사한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxFootHeightDifference = 80.0f;

    // 장애물에 정확히 닿는 지점보다 조금 앞에 목적지를 둔다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Melee Approach", meta = (ClampMin = "0.0", Units = "cm"))
    float SweepMargin = 2.0f;

    // 약한 참조이므로 취소된 능력이나 삭제된 몬스터의 수명을 연장하지 않는다.
    TWeakObjectPtr<UGameplayAbility> ApproachOwner;
    TWeakObjectPtr<UAnimMontage> ApproachMontage;
    TWeakObjectPtr<ALMBaseCharacter> FocusedTarget;

    // 이번 타에서 실제 접촉한 이후에만 후속 전진을 막는다. 다음 타에서 초기화한다.
    bool bStoppedByMeleeContact = false;

    /** 현재 Avatar·능력·몽타주가 이 접근을 소유하는지 확인한다. */
    bool OwnsActiveApproach() const;

    /** 공통 표적 정책과 거리·발 높이·전방·벽 조건을 검사한다. */
    bool EvaluateCandidate(ALMBaseCharacter* Candidate, FVector& OutDirection, float& OutTravel) const;
};