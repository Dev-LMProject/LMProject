#pragma once

#include "CoreMinimal.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
struct FInputActionValue;

/** 자유 이동의 최종 조건. 이후 한 연결 담당이 상태·효과의 제한을 합산해 전달한다. */
USTRUCT(BlueprintType)
struct FLMFreeMovementPolicy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool AllowMove = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool AllowLook = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool AllowJump = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool AllowRun = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool FaceMovement = true;

    // 외부 행동 중에는 자유 이동이 입력·속도·몸 회전을 덮어쓰지 않는다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool ExternalMovement = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        meta = (ClampMin = "0.0"))
    float SpeedMultiplier = 1.0f;
};

/** 입력 요청을 이동 처리와 연결하고, 실제 도약·착지 결과를 애니메이션에 제공한다. */
UCLASS()
class LUMIA_API ALMPlayerCharacter : public ALMBaseCharacter
{
    GENERATED_BODY()

public:
    /** 캡슐·이동 기본값·카메라 컴포넌트를 구성한다. */
    ALMPlayerCharacter();

    /** 점프 요청을 검사하고 허용된 카메라 줌을 갱신한다. */
    virtual void Tick(float DeltaSeconds) override;

    /** 최종 이동 조건을 적용하고, 금지된 점프 예약과 이동 입력을 정리한다. */
    UFUNCTION(BlueprintCallable, Category = "LM|Movement")
    void ApplyFreeMovementPolicy(
        const FLMFreeMovementPolicy& NewPolicy);

    /** 예약·미처리 점프 명령을 취소한다. 이미 발생한 공중 속도는 제거하지 않는다. */
    UFUNCTION(BlueprintCallable, Category = "LM|Movement")
    void CancelPendingJump();

    /** 현재 제한을 통과한 유효한 이동 입력이 있는지 반환한다. */
    bool HasMoveInput() const;

    /** 토글로 켠 달리기 상태가 유지되고 현재 이동·달리기가 허용되는지 반환한다. */
    bool IsRunRequestedAndAllowed() const;

    /** 대시·연출 등 외부 행동이 이동을 맡고 있지 않은지 반환한다. */
    bool UsesFreeMovement() const
    {
        return !MovementPolicy.ExternalMovement;
    }

    /** 속도 보정 전의 걷기 기준값을 반환한다. */
    float GetWalkSpeed() const
    {
        return WalkSpeed;
    }

    /** 속도 보정 전의 달리기 기준값을 반환한다. */
    float GetRunSpeed() const
    {
        return RunSpeed;
    }

    /** 실제 도약 발생 번호를 반환해 AnimInstance가 사건을 한 번만 처리하게 한다. */
    uint32 GetJumpSerial() const
    {
        return JumpSerial;
    }

    /** 실제 착지 발생 번호를 반환해 AnimInstance가 사건을 한 번만 처리하게 한다. */
    uint32 GetLandingSerial() const
    {
        return LandingSerial;
    }

    /** 실제 이륙 순간의 수평 속도(cm/s)를 반환한다. */
    float GetTakeoffSpeed() const
    {
        return TakeoffSpeed;
    }

    /** 접지 직전 아래 방향 속도의 크기(cm/s)를 반환한다. */
    float GetLandingImpactSpeed() const
    {
        return LandingImpactSpeed;
    }

    /** 착지 순간 이동을 계속하려는 입력이 있었는지 반환한다. */
    bool WasMovingAtLanding() const
    {
        return bMovingAtLanding;
    }

    /** 접지 당시 자유 이동이었는지 반환해 특수 행동의 착지를 구분한다. */
    bool WasFreeMovementAtLanding() const
    {
        return bFreeMovementAtLanding;
    }

    /** 현재 공중 상태가 기본 점프 성공에서 시작되었는지 반환한다. */
    bool IsCurrentFallFromJump() const
    {
        return bCurrentFallFromJump;
    }

    /** 마지막 접지의 월드 시간(초)을 반환한다. */
    double GetLastLandingTime() const
    {
        return LastLandingTime;
    }

    /** 지상에서 유효한 점프 준비가 진행 중인지 반환한다. */
    bool IsPreparingJump() const;

    /** 준비부터 이륙 이후까지 연속된 점프 포즈 경과 시간(초)을 반환한다. */
    float GetJumpAnimationElapsed() const;

protected:
    /** BP 설정을 검증하고 초기 이동 속도와 카메라 거리를 반영한다. */
    virtual void BeginPlay() override;

    /** Enhanced Input 액션을 이동·시점·점프·달리기·줌 함수에 연결한다. */
    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    /** 현재 행동 제한과 엔진의 기본 점프 가능 조건을 함께 검사한다. */
    virtual bool CanJumpInternal_Implementation() const override;

    /** 엔진의 점프 처리 후 성공하지 못한 1회 명령을 정리한다. */
    virtual void CheckJumpInput(float DeltaTime) override;

    /** 실제 도약 성공 시 속도·시간·발생 번호를 기록한다. */
    virtual void OnJumped_Implementation() override;

    /** 접지 직전 속도와 이동 상태를 기록한다. 몽타주는 강제 재생하지 않는다. */
    virtual void Landed(const FHitResult& Hit) override;

    /** 지상·공중 전환에 맞춰 속도 기준과 점프 예약을 갱신한다. */
    virtual void OnMovementModeChanged(
        EMovementMode PreviousMode,
        uint8 PreviousCustomMode = 0) override;

    /** 조종 해제 시 이동·달리기 의도와 점프 예약을 비운다. */
    virtual void UnPossessed() override;

    /** 준비된 점프를 한 번 요청한다. 이후 GAS 승인 경로로 교체할 지점이다. */
    virtual bool ExecutePreparedJump();

private:
    /** 입력 크기를 제한하고 카메라의 수평 축으로 이동을 요청한다. */
    void Move(const FInputActionValue& Value);

    /** 이동키 해제·취소 시 이동축을 비우고 달리기 토글을 해제한다. */
    void ClearMoveInput();

    /** 허용된 마우스 입력을 카메라의 컨트롤러 회전에 반영한다. */
    void Look(const FInputActionValue& Value);

    /** 현재 로컬 플레이어가 게임 카메라의 줌을 조작할 수 있는지 검사한다. */
    bool CanUseCameraZoom() const;

    /** 휠 입력을 목표 거리에 누적하고 최소·최대 거리를 제한한다. */
    void ZoomCamera(const FInputActionValue& Value);

    /** 카메라 암 길이를 목표 거리까지 부드럽게 변경한다. */
    void UpdateCameraZoom(float DeltaSeconds);

    /** 지상 준비 또는 착지 직전 버퍼에 사용할 점프 요청을 기록한다. */
    void RequestJump();

    /** 정상 버튼 해제를 처리하되 짧게 누른 1회 요청은 보존한다. */
    void ReleaseJumpInput();

    /** Shift를 누를 때마다 달리기를 켜거나 끄고 이동 설정을 갱신한다. */
    void StartSprint();

    /** 달리기 토글을 해제하되 이륙 당시 공중 속도 기준은 유지한다. */
    void StopSprint();

    /** 현재 자유 이동 상태에서 기본 점프 요청이 허용되는지 검사한다. */
    bool CanUseBasicJump() const;

    /** 준비 완료·유효 시간·허용 조건을 확인해 점프 명령을 한 번 발행한다. */
    void TryBufferedJump();

    /** 현재 조건과 속도 배율을 합쳐 자유 이동의 회전·최고 속도를 적용한다. */
    void RefreshMovementSettings();

    /** 공중 진입 속도를 기록하여 공중 Shift 변경과 분리한다. */
    void CaptureAirSpeed();

    // 실제 이동의 기준 속도(cm/s). 최종 속도에는 SpeedMultiplier를 적용한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Movement",
        meta = (ClampMin = "1.0"))
    float WalkSpeed = 330.0f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Movement",
        meta = (ClampMin = "2.0"))
    float RunSpeed = 500.0f;

    // 제자리·걷기 점프의 지상 준비 시간(초). 이동 중 달리기 점프는 즉시 요청한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Jump",
        meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float JumpPreparationSeconds = 0.05f;

    // 착지 직전 누른 점프 입력을 보관하는 시간(초).
    UPROPERTY(EditDefaultsOnly, Category = "LM|Jump",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float JumpBufferSeconds = 0.12f;

    // 현재 적용 중인 최종 조건. 지금은 기본값으로 동작한다.
    UPROPERTY(Transient, VisibleInstanceOnly, Category = "LM|Movement")
    FLMFreeMovementPolicy MovementPolicy;

    // 몸 회전과 독립된 카메라 연결. 기존 컴포넌트 이름을 유지한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    // 플레이어가 선택할 수 있는 카메라 암 길이의 범위(cm).
    UPROPERTY(EditDefaultsOnly, Category = "LM|Camera|Zoom",
        meta = (ClampMin = "1.0", Units = "cm"))
    float MinZoomDistance = 220.0f;

    UPROPERTY(EditDefaultsOnly, Category = "LM|Camera|Zoom",
        meta = (ClampMin = "1.0", Units = "cm"))
    float MaxZoomDistance = 500.0f;

    // 휠 입력 1단위에 변경할 거리(cm).
    UPROPERTY(EditDefaultsOnly, Category = "LM|Camera|Zoom",
        meta = (ClampMin = "1.0", Units = "cm"))
    float ZoomStep = 35.0f;

    // 값이 클수록 목표 거리에 빠르게 도달한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Camera|Zoom",
        meta = (ClampMin = "0.1"))
    float ZoomInterpSpeed = 10.0f;

    // 벽 충돌로 카메라가 당겨져도 플레이어가 선택한 거리는 보존한다.
    UPROPERTY(Transient, VisibleInstanceOnly, Category = "LM|Camera|Zoom")
    float TargetZoomDistance = 350.0f;

    // 캐릭터 BP의 Class Defaults에서 입력 에셋을 지정한다.
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> CameraZoomAction;

    // 원시 입력과 실행 중인 점프 요청을 구분한다. 실제 허용 여부는 별도로 검사한다.
    FVector2D MoveAxis = FVector2D::ZeroVector;

    // Shift 해제·점프·착지에는 유지하고, 이동 입력을 놓으면 해제하는 달리기 상태다.
    bool bWantsToRun = false;
    bool bJumpPending = false;
    bool bJumpIssued = false;
    bool bJumpBeganGrounded = false;
    bool bCurrentFallFromJump = false;
    bool bMovingAtLanding = false;
    bool bFreeMovementAtLanding = true;

    // 월드 시간과 사건 기록. AnimInstance가 실제 도약·착지를 기준으로 포즈를 선택한다.
    double JumpRequestedAt = -1.0;
    double JumpReadyAt = -1.0;
    double JumpExpiresAt = -1.0;
    double LastJumpTime = -1.0;
    double LastLandingTime = -1.0;

    float PreparationElapsed = 0.0f;
    float AirBaseSpeed = 140.0f;
    float TakeoffSpeed = 0.0f;
    float LandingImpactSpeed = 0.0f;

    uint32 JumpSerial = 0;
    uint32 LandingSerial = 0;
};
