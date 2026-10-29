#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LMPlayerAnimInstance.generated.h"

class ALMPlayerCharacter;
class UAnimSequence;

/** 게임 스레드에서 수집한 표시용 데이터. 행동 승인과 비용 처리는 담당하지 않는다. */
USTRUCT(BlueprintType)
struct FLMPlayerLocomotionFrame
{
    GENERATED_BODY()

    // 속도 단위는 cm/s. GroundSpeed는 수평 속도의 크기다.
    UPROPERTY(BlueprintReadOnly)
    float GroundSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float VerticalSpeed = 0.0f;

    // 걷기/달리기 혼합값(0~1)과 실제 속도에 맞춘 모션 재생 배율.
    UPROPERTY(BlueprintReadOnly)
    float WalkRunAlpha = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float LoopPlayRate = 1.0f;

    // 정지까지 남은 양수 거리(cm). Distance Matching 노드에 그대로 전달한다.
    UPROPERTY(BlueprintReadOnly)
    float StopDistance = 0.0f;

    // Sequence Evaluator에 전달할 각 클립의 시간 위치(초).
    UPROPERTY(BlueprintReadOnly)
    float JumpPoseTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float StartPoseTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float LandingPoseTime = 0.0f;

    // 포즈 선택 조건. FlightPoseActive에는 지상 점프 준비도 포함된다.
    UPROPERTY(BlueprintReadOnly)
    bool HasMoveInput = false;

    UPROPERTY(BlueprintReadOnly)
    bool Moving = false;

    UPROPERTY(BlueprintReadOnly)
    bool Stopping = false;

    UPROPERTY(BlueprintReadOnly)
    bool Starting = false;

    UPROPERTY(BlueprintReadOnly)
    bool InAir = false;

    UPROPERTY(BlueprintReadOnly)
    bool PreparingJump = false;

    UPROPERTY(BlueprintReadOnly)
    bool FlightPoseActive = false;

    UPROPERTY(BlueprintReadOnly)
    bool JumpStarting = false;

    UPROPERTY(BlueprintReadOnly)
    bool Landing = false;

    UPROPERTY(BlueprintReadOnly)
    bool HeavyLanding = false;
};

/** 실제 이동 결과를 모션 선택과 AnimBP 전환 조건으로 변환한다. */
UCLASS(Transient, Blueprintable)
class LUMIA_API ULMPlayerAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    /** 현재 Pawn을 기준으로 애니메이션의 이전 사건·포즈 상태를 초기화한다. */
    virtual void NativeInitializeAnimation() override;

    /** 매 업데이트에서 실제 속도·점프·착지를 읽고 표시용 데이터를 갱신한다. */
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // AnimBP는 이 스냅샷과 선택된 클립을 읽는다.
    UPROPERTY(Transient, BlueprintReadOnly, Category = "LM|Locomotion")
    FLMPlayerLocomotionFrame Locomotion;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "LM|Locomotion")
    TObjectPtr<UAnimSequence> SelectedStop;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "LM|Locomotion")
    TObjectPtr<UAnimSequence> SelectedJump;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "LM|Locomotion")
    TObjectPtr<UAnimSequence> SelectedStart;

protected:
    // AnimBP Class Defaults에서 게임용 정지 복사본 네 개를 지정한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Stop")
    TObjectPtr<UAnimSequence> WalkStopLeft;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Stop")
    TObjectPtr<UAnimSequence> WalkStopRight;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Stop")
    TObjectPtr<UAnimSequence> RunStopLeft;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Stop")
    TObjectPtr<UAnimSequence> RunStopRight;

    // 걷기·달리기의 좌우 도약 원본을 지정하고 초반 도약 구간만 사용한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Jump")
    TObjectPtr<UAnimSequence> WalkJumpLeft;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Jump")
    TObjectPtr<UAnimSequence> WalkJumpRight;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Jump")
    TObjectPtr<UAnimSequence> RunJumpLeft;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Jump")
    TObjectPtr<UAnimSequence> RunJumpRight;

    // 정지 상태에서 달리기를 시작할 때 사용한다. None이면 기본 가속·혼합을 사용한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Start")
    TObjectPtr<UAnimSequence> RunStart;

    // 걷는 도중 달리기로 전환할 때 사용할 모션이다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Assets|Start")
    TObjectPtr<UAnimSequence> WalkToRunStart;

    /** 정지 출발과 걷기→달리기에 사용하는 출발 모션의 재생 배율. */
    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning|Start",
        meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float RunStartPlayRate = 1.5f;

    // 모션 1배속의 기준 속도(cm/s). Character의 실제 최고 속도와 구분한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "1.0"))
    float WalkAnimationReferenceSpeed = 330.0f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "1.0"))
    float RunAnimationReferenceSpeed = 550.0f;

    // 포즈 사용 시간(초)과 강한 착지의 하강 속도 기준(cm/s).
    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "0.0"))
    float JumpStartWindow = 0.24f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "0.0"))
    float MovingLandingWindow = 0.12f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "0.0"))
    float IdleLandingWindow = 0.40f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "0.0"))
    float HeavyLandingWindow = 0.55f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Tuning",
        meta = (ClampMin = "0.0"))
    float HeavyLandingSpeed = 900.0f;

private:
    /** Pawn 교체·재초기화 시 오래된 도약과 착지를 새 사건으로 재생하지 않게 한다. */
    void ResetLocomotionState(ALMPlayerCharacter* Character);

    /** 동기화 그룹의 직전 마커로 좌우 보행 주기를 읽는다. */
    void UpdateSupportFoot();

    /** 정지 진입 때 속도 계열과 좌우 발에 맞는 클립을 한 번 선택한다. */
    void SelectStop(float Speed, float RunThreshold);

    /** 점프 진입 때 속도 계열과 좌우 발에 맞는 도약 클립을 선택한다. */
    void SelectJump(float Speed, float RunThreshold);

    // 이전 사건과 진입 상태를 보관해 클립 재선택·착지 재진입을 방지한다.
    TWeakObjectPtr<ALMPlayerCharacter> CachedCharacter;

    uint32 SeenJumpSerial = 0;
    uint32 SeenLandingSerial = 0;

    bool bLeftSupport = true;
    bool bWasPreparingJump = false;
    bool bStartActive = false;

    // 달리기 요청이 이번 프레임에 시작됐는지 판별한다.
    bool bWasRunRequested = false;
    bool bLandingActive = false;
    bool bLandingWasMoving = false;

    // 출발 클립에서 읽을 시간(초). 재생 배율이 적용된 시간이다.
    float StartAge = 0.0f;

    // AnimBP GroundMovement의 Start -> Move 전환에서 읽은 실제 혼합 시간(초).
    float StartBlendOutSeconds = 0.0f;
};
