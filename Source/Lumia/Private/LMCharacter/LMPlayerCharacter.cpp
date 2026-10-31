#include "LMCharacter/LMPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "LMComponent/LMInteractorComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "LMCamera/LMPlayerCameraManager.h"
#include "LMComponent/LMScanComponent.h"
#include "Math/RotationMatrix.h"
#include "AlphaBlend.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionWarpingComponent.h"

namespace LMPivot
{
    const FName Turn(TEXT("Turn"));
    const FName Run(TEXT("Run"));
    const FName Stop(TEXT("Stop"));
    const FName Facing(TEXT("LM_PivotFacing"));

    constexpr double RunReleaseGrace = 0.10;

    // 회전 뒤 달려 나가는 방향에서 45도 이상 벗어나면 새 입력에 제어를 넘긴다.
    constexpr float RunContinuationDot = 0.70710678f;
}

/** 몸은 이동 방향, 카메라는 컨트롤러 방향을 사용하도록 기본값을 구성한다. */
ALMPlayerCharacter::ALMPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 90.0f);

    // 컨트롤러의 시점 회전이 캐릭터 몸에 직접 적용되지 않게 한다.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();

    PivotMotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("PivotMotionWarping"));
    Movement->bAllowPhysicsRotationDuringAnimRootMotion = false;

    // 실제 이동 입력의 가속 방향을 향해 몸을 회전시킨다.
    Movement->bOrientRotationToMovement = true;
    Movement->bUseControllerDesiredRotation = false;
    Movement->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

    Movement->MaxWalkSpeed = WalkSpeed;
    // 최대 가속도
    Movement->MaxAcceleration = 900.0f;
    Movement->GroundFriction = 8.0f;

    // 방향 전환용 마찰과 정지할 때의 감속을 구분한다.
    Movement->bUseSeparateBrakingFriction = true;
    Movement->BrakingFriction = 0.0f;
    Movement->BrakingFrictionFactor = 1.0f;
    Movement->BrakingDecelerationWalking = 1200.0f;

    Movement->JumpZVelocity = 500.0f;
    Movement->GravityScale = 1.0f;
    Movement->AirControl = 0.25f;
    Movement->AirControlBoostMultiplier = 1.0f;

    // 공중에서 이동키를 놓아도 수평 관성이 바로 사라지지 않게 한다.
    Movement->FallingLateralFriction = 0.0f;
    Movement->BrakingDecelerationFalling = 0.0f;

    JumpMaxCount = 1;
    JumpMaxHoldTime = 0.0f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom"));

    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.0f;
    CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 60.0f);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bDoCollisionTest = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("FollowCamera"));

    FollowCamera->SetupAttachment(
        CameraBoom, USpringArmComponent::SocketName);

    FollowCamera->bUsePawnControlRotation = false;

    // BP가 추가한 콜리전을 연결할 목록 관리 컴포넌트를 준비한다.
    InteractorComponent = CreateDefaultSubobject<ULMInteractorComponent>(TEXT("InteractorComponent"));
    
    ScanComponent = CreateDefaultSubobject<ULMScanComponent>(TEXT("ScanComponent"));
}

/** BP에 저장된 설정을 포함하여 이동·점프와 시작 카메라 거리를 확정한다. */
void ALMPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    WalkSpeed = FMath::Max(WalkSpeed, 1.0f);
    RunSpeed = FMath::Max(RunSpeed, WalkSpeed + 1.0f);

    JumpPreparationSeconds = FMath::Clamp(
        JumpPreparationSeconds, 0.0f, 0.1f);

    JumpBufferSeconds = FMath::Clamp(
        JumpBufferSeconds, 0.0f, 0.3f);

    PivotInputReleaseGrace = FMath::Clamp(PivotInputReleaseGrace, 0.0f, 0.2f);
    PivotBlendOutSeconds = FMath::Clamp(PivotBlendOutSeconds, 0.0f, 0.3f);
    PivotRunBlendOutSeconds = FMath::Clamp(PivotRunBlendOutSeconds, 0.05f, 0.3f);

    AirBaseSpeed = WalkSpeed;

    RefreshMovementSettings();

    MinZoomDistance = FMath::Max(MinZoomDistance, 1.0f);
    MaxZoomDistance = FMath::Max(MaxZoomDistance, MinZoomDistance);
    ZoomStep = FMath::Max(ZoomStep, 1.0f);
    ZoomInterpSpeed = FMath::Max(ZoomInterpSpeed, 0.1f);

    // 캐릭터 BP에 지정한 카메라 거리를 시작값으로 사용한다.
    TargetZoomDistance = FMath::Clamp(
        CameraBoom->TargetArmLength, MinZoomDistance, MaxZoomDistance);
    CameraBoom->TargetArmLength = TargetZoomDistance;
}

/** 피벗·점프 요청을 검사하고 카메라 줌을 갱신한다. */
void ALMPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdatePivot();
    TryBufferedJump();
    UpdateCameraZoom(DeltaSeconds);
}

/** 입력 실행·해제·취소를 각각 연결하여 입력 상태가 남지 않게 한다. */
void ALMPlayerCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* Input =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!ensureMsgf(
            Input,
            TEXT("Enhanced Input Component is required.")) ||
        !ensureMsgf(
            MoveAction && LookAction && JumpAction && SprintAction,
            TEXT("Assign MoveAction, LookAction, JumpAction and SprintAction in the player BP.")))
    {
        return;
    }

    Input->BindAction(
        MoveAction, ETriggerEvent::Triggered,
        this, &ThisClass::Move);

    Input->BindAction(
        MoveAction, ETriggerEvent::Completed,
        this, &ThisClass::ClearMoveInput);

    Input->BindAction(
    MoveAction, ETriggerEvent::Canceled,
    this, &ThisClass::CancelMoveInput);

    Input->BindAction(
        LookAction, ETriggerEvent::Triggered,
        this, &ThisClass::Look);

    Input->BindAction(
        JumpAction, ETriggerEvent::Started,
        this, &ThisClass::RequestJump);

    Input->BindAction(
        JumpAction, ETriggerEvent::Completed,
        this, &ThisClass::ReleaseJumpInput);

    Input->BindAction(
        JumpAction, ETriggerEvent::Canceled,
        this, &ThisClass::CancelPendingJump);

    Input->BindAction(
        SprintAction, ETriggerEvent::Started,
        this, &ThisClass::StartSprint);

    // Shift를 놓는 Completed에는 달리기를 끄지 않는다. 이동키 해제 시 초기화한다.
    Input->BindAction(
        SprintAction, ETriggerEvent::Canceled,
        this, &ThisClass::StopSprint);

    if (ensureMsgf(
            CameraZoomAction,
            TEXT("Assign CameraZoomAction in the player BP.")))
    {
        Input->BindAction(
            CameraZoomAction, ETriggerEvent::Triggered,
            this, &ThisClass::ZoomCamera);
    }
    
    if (ensureMsgf(ScanAction, TEXT("Assign ScanAction in the player BP.")))
    {
        Input->BindAction(
            ScanAction,
            ETriggerEvent::Started,
            this,
            &ThisClass::RequestScan);
    }
}

/** 현재 제한을 통과한 유효한 이동 입력이 있는지 반환한다. */
bool ALMPlayerCharacter::HasMoveInput() const
{
    return UsesFreeMovement() &&
        MovementPolicy.AllowMove &&
        Controller &&
        !IsMoveInputIgnored() &&
        !MoveAxis.IsNearlyZero(0.01f);
}

/** 유지 중인 달리기 토글과 현재 이동·달리기 허용 조건을 함께 검사한다. */
bool ALMPlayerCharacter::IsRunRequestedAndAllowed() const
{
    return UsesFreeMovement() &&
        MovementPolicy.AllowMove &&
        MovementPolicy.AllowRun &&
        bWantsToRun;
}

/** 현재 방향키를 카메라의 수평 방향으로 변환한다. */
FVector ALMPlayerCharacter::GetMoveDirection() const
{
    if (!Controller)
    {
        return FVector::ZeroVector;
    }

    const FRotationMatrix CameraAxes(
        FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f));

    return (CameraAxes.GetUnitAxis(EAxis::X) * MoveAxis.Y +
            CameraAxes.GetUnitAxis(EAxis::Y) * MoveAxis.X).GetSafeNormal2D();
}

/** 카메라의 수평 방향을 기준으로 이동 입력을 전달한다. */
void ALMPlayerCharacter::Move(const FInputActionValue& Value)
{
    // 대각선 입력의 크기가 직선 입력보다 커지지 않도록 제한한다.
    MoveAxis = Value.Get<FVector2D>().GetClampedToMaxSize(1.0f);

    if (MoveAxis.IsNearlyZero(0.01f))
    {
        ClearMoveInput();
        return;
    }

    if (!HasMoveInput())
    {
        CancelMoveInput();
        return;
    }

    if (RunReleaseDeadline >= 0.0 &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() >= RunReleaseDeadline)
    {
        StopSprint();
    }

    RunReleaseDeadline = -1.0;

    const FVector DesiredDirection = GetMoveDirection();

    if (IsPivoting())
    {
        UpdatePivotExit();
        if (IsPivoting())
        {
            return;
        }
    }
    else if (TryStartPivot(DesiredDirection))
    {
        return;
    }

    AddMovementInput(DesiredDirection, MoveAxis.Size());
}

/** 이동은 즉시 해제하고, 달리기 의도만 방향키 교체 시간 동안 보존한다. */
void ALMPlayerCharacter::ClearMoveInput()
{
    MoveAxis = FVector2D::ZeroVector;
    ConsumeMovementInputVector();

    if ((bWantsToRun || IsPivoting()) && GetWorld())
    {
        if (RunReleaseDeadline < 0.0)
        {
            RunReleaseDeadline =
                GetWorld()->GetTimeSeconds() +
                (IsPivoting() ? PivotInputReleaseGrace : LMPivot::RunReleaseGrace);
        }
    }
    else
    {
        StopSprint();
    }

    UpdatePivotExit();
}

/** 허용된 마우스 입력으로 카메라의 컨트롤러 회전을 변경한다. */
void ALMPlayerCharacter::Look(const FInputActionValue& Value)
{
    if (!Controller ||
        !MovementPolicy.AllowLook ||
        Controller->IsLookInputIgnored())
    {
        return;
    }

    const FVector2D LookAxis = Value.Get<FVector2D>();

    AddControllerYawInput(LookAxis.X);
    AddControllerPitchInput(LookAxis.Y);
}

bool ALMPlayerCharacter::CanUseCameraZoom() const
{
    const APlayerController* PlayerController =
        Cast<APlayerController>(Controller);

    if (!PlayerController ||
        !PlayerController->IsLocalController() ||
        !MovementPolicy.AllowLook ||
        PlayerController->IsLookInputIgnored() ||
        PlayerController->GetViewTarget() != this)
    {
        return false;
    }

    const ALMPlayerCameraManager* CameraManager =
        Cast<ALMPlayerCameraManager>(PlayerController->PlayerCameraManager);

    return !CameraManager || !CameraManager->IsSequenceCameraActive();
}

void ALMPlayerCharacter::ZoomCamera(const FInputActionValue& Value)
{
    if (!CanUseCameraZoom())
    {
        return;
    }

    const float WheelInput = Value.Get<float>();

    // 보간 중에도 연속 휠 입력이 빠지지 않도록 목표 거리에 누적한다.
    TargetZoomDistance = FMath::Clamp(
        TargetZoomDistance - WheelInput * ZoomStep,
        MinZoomDistance, MaxZoomDistance);
}

void ALMPlayerCharacter::UpdateCameraZoom(float DeltaSeconds)
{
    if (!CanUseCameraZoom())
    {
        return;
    }

    CameraBoom->TargetArmLength = FMath::FInterpTo(
        CameraBoom->TargetArmLength, TargetZoomDistance,
        DeltaSeconds, ZoomInterpSpeed);
}

/** 기본 점프 요청과 실행이 현재 행동 제한을 통과하는지 검사한다. */
bool ALMPlayerCharacter::CanUseBasicJump() const
{
    return UsesFreeMovement() &&
        MovementPolicy.AllowJump &&
        Controller &&
        !IsMoveInputIgnored();
}

/** 행동 제한과 엔진의 기본 점프 가능 조건을 함께 검사한다. */
bool ALMPlayerCharacter::CanJumpInternal_Implementation() const
{
    return CanUseBasicJump() &&
        Super::CanJumpInternal_Implementation();
}

/** 지상 준비 또는 착지 직전 입력 버퍼에 사용할 점프 요청을 기록한다. */
void ALMPlayerCharacter::RequestJump()
{
    if (!CanUseBasicJump() || bJumpIssued || !GetWorld())
    {
        return;
    }

    const UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    const bool bGrounded = Movement->IsMovingOnGround();

    if ((!bGrounded && !Movement->IsFalling()) ||
        (bGrounded && !CanJump()) ||
        (!bGrounded && JumpBufferSeconds <= 0.0f))
    {
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();

    CancelPivot();

    bJumpPending = true;
    bJumpBeganGrounded = bGrounded;

    // 이동 중 달리기를 요청했다면 준비 대기를 생략한다.
    // 대기하는 동안 발판을 벗어나 점프 예약이 취소되는 경우를 줄인다.
    const bool bRunningJump =
        bGrounded &&
        IsRunRequestedAndAllowed() &&
        HasMoveInput() &&
        GetVelocity().Size2D() > 5.0f;

    JumpRequestedAt = Now;
    JumpReadyAt = Now +
        (bGrounded && !bRunningJump ? JumpPreparationSeconds : 0.0f);

    // 공중에서 누른 점프 입력의 유효 시간이다.
    JumpExpiresAt = Now + JumpBufferSeconds;

    TryBufferedJump();
}

/** 정상적인 버튼 해제에서는 짧게 누른 1회 점프 요청을 보존한다. */
void ALMPlayerCharacter::ReleaseJumpInput()
{
    // 엔진이 아직 처리하지 않은 명령은 검사 시점까지 유지한다.
    if (!bJumpIssued)
    {
        StopJumping();
    }
}

/** 준비 완료·유효 시간·허용 조건을 확인하여 점프 명령을 한 번 발행한다. */
void ALMPlayerCharacter::TryBufferedJump()
{
    if ((!bJumpPending && !bJumpIssued) || !GetWorld())
    {
        return;
    }

    if (!CanUseBasicJump())
    {
        CancelPendingJump();
        return;
    }

    if (bJumpIssued)
    {
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    const bool bGrounded =
        GetCharacterMovement()->IsMovingOnGround();

    if ((!bJumpBeganGrounded && Now > JumpExpiresAt) ||
        (bJumpBeganGrounded && !bGrounded))
    {
        CancelPendingJump();
        return;
    }

    if (!bGrounded || Now < JumpReadyAt)
    {
        return;
    }

    bJumpPending = false;
    bJumpIssued = true;

    if (!ExecutePreparedJump())
    {
        CancelPendingJump();
    }
}

/** 준비된 점프를 엔진에 요청한다. 이후 GAS 승인 경로를 연결할 지점이다. */
bool ALMPlayerCharacter::ExecutePreparedJump()
{
    if (!CanJump())
    {
        return false;
    }

    Jump();
    return true;
}

/** 엔진의 점프 검사 후 성공하지 못한 1회 명령을 정리한다. */
void ALMPlayerCharacter::CheckJumpInput(float DeltaTime)
{
    if (bJumpIssued && !CanUseBasicJump())
    {
        CancelPendingJump();
    }

    Super::CheckJumpInput(DeltaTime);

    // 성공했다면 OnJumped에서 이미 false로 바뀐다.
    if (bJumpIssued)
    {
        CancelPendingJump();
    }
}

/** 실제 도약 성공 시 속도·시간·발생 번호를 기록한다. */
void ALMPlayerCharacter::OnJumped_Implementation()
{
    Super::OnJumped_Implementation();

    const double Now = GetWorld()->GetTimeSeconds();

    PreparationElapsed =
        (bJumpIssued && bJumpBeganGrounded)
        ? static_cast<float>(Now - JumpRequestedAt)
        : 0.0f;

    LastJumpTime = Now;
    TakeoffSpeed = GetVelocity().Size2D();

    bCurrentFallFromJump = true;
    bJumpPending = false;
    bJumpIssued = false;

    ++JumpSerial;

    // 버튼을 계속 누르고 있어도 이번 1회 명령은 소비한다.
    StopJumping();
}

/** 준비·예약·미처리 점프 명령을 취소한다. 실제 공중 속도는 제거하지 않는다. */
void ALMPlayerCharacter::CancelPendingJump()
{
    bJumpPending = false;
    bJumpIssued = false;
    bJumpBeganGrounded = false;

    JumpRequestedAt = -1.0;
    JumpReadyAt = -1.0;
    JumpExpiresAt = -1.0;

    StopJumping();
}

/** 공중 진입 당시 속도를 기록하여 공중 Shift 변경과 분리한다. */
void ALMPlayerCharacter::CaptureAirSpeed()
{
    const float Scale = MovementPolicy.SpeedMultiplier;

    AirBaseSpeed = Scale > KINDA_SMALL_NUMBER
        ? FMath::Max(WalkSpeed, GetVelocity().Size2D() / Scale)
        : WalkSpeed;
}

/** 지상·공중 전환에 맞춰 속도 기준과 점프 예약을 갱신한다. */
void ALMPlayerCharacter::OnMovementModeChanged(
    EMovementMode PreviousMode,
    uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(
        PreviousMode, PreviousCustomMode);

    const UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    if (!Movement->IsMovingOnGround())
    {
        CancelPivot();
    }

    if (Movement->IsFalling() && PreviousMode != MOVE_Falling)
    {
        bCurrentFallFromJump = false;

        if (UsesFreeMovement())
        {
            CaptureAirSpeed();
        }

        // 지상에서 준비하는 동안 발판 밖으로 나갔다면 요청을 취소한다.
        if (bJumpPending && bJumpBeganGrounded && !bJumpIssued)
        {
            CancelPendingJump();
        }
    }
    else if (Movement->IsMovingOnGround())
    {
        bCurrentFallFromJump = false;
    }
    else if (!Movement->IsFalling())
    {
        bCurrentFallFromJump = false;
        CancelPendingJump();
    }

    RefreshMovementSettings();
}

/** 접지 직전 속도와 이동 상태를 기록하여 AnimInstance에 전달한다. */
void ALMPlayerCharacter::Landed(const FHitResult& Hit)
{
    bFreeMovementAtLanding = UsesFreeMovement();

    LandingImpactSpeed =
        FMath::Max(0.0f, -GetVelocity().Z);

    // 공중 관성이 남아 있어도 키를 놓았다면 정지 착지 자세를 충분히 보여준다.
    bMovingAtLanding = HasMoveInput();

    LastLandingTime = GetWorld()->GetTimeSeconds();

    ++LandingSerial;

    // 착지 몽타주를 강제 재생하지 않고 AnimBP가 착지 표현을 선택한다.
    Super::Landed(Hit);
}

/** 지상에서 유효한 점프 준비가 진행 중인지 반환한다. */
bool ALMPlayerCharacter::IsPreparingJump() const
{
    return (bJumpPending || bJumpIssued) &&
        bJumpBeganGrounded &&
        CanUseBasicJump() &&
        GetCharacterMovement()->IsMovingOnGround();
}

/** 준비부터 이륙 이후까지 연속된 애니메이션 경과 시간을 반환한다. */
float ALMPlayerCharacter::GetJumpAnimationElapsed() const
{
    if (!GetWorld())
    {
        return 0.0f;
    }

    const double Now = GetWorld()->GetTimeSeconds();

    if (IsPreparingJump())
    {
        return static_cast<float>(Now - JumpRequestedAt);
    }

    return bCurrentFallFromJump
        ? PreparationElapsed +
            static_cast<float>(Now - LastJumpTime)
        : 0.0f;
}

/** Shift를 누를 때마다 달리기를 전환한다. 키 해제로는 달리기를 중단하지 않는다. */
void ALMPlayerCharacter::StartSprint()
{
    if (!UsesFreeMovement() ||
        !MovementPolicy.AllowMove ||
        !MovementPolicy.AllowRun ||
        !Controller ||
        IsMoveInputIgnored())
    {
        return;
    }

    bWantsToRun = !bWantsToRun;
    RunReleaseDeadline = -1.0;

    UpdatePivotExit();
    RefreshMovementSettings();
}

/** 달리기 의도를 해제하고 피벗의 다음 구간도 갱신한다. */
void ALMPlayerCharacter::StopSprint()
{
    bWantsToRun = false;
    RunReleaseDeadline = -1.0;

    UpdatePivotExit();
    RefreshMovementSettings();
}

/** 현재 조건과 속도 배율을 합쳐 자유 이동의 회전·최고 속도를 적용한다. */
void ALMPlayerCharacter::RefreshMovementSettings()
{
    UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    Movement->bOrientRotationToMovement =
        UsesFreeMovement() &&
        MovementPolicy.FaceMovement &&
        MovementPolicy.AllowMove &&
        !IsPivoting();

    if (Movement->bOrientRotationToMovement)
    {
        bUseControllerRotationYaw = false;
        Movement->bUseControllerDesiredRotation = false;
    }

    // 외부 행동이 이동을 맡는 동안에는 자유 이동의 속도를 덮어쓰지 않는다.
    if (!UsesFreeMovement())
    {
        return;
    }

    if (Movement->IsMovingOnGround())
    {
        Movement->MaxWalkSpeed =
            (IsRunRequestedAndAllowed() ? RunSpeed : WalkSpeed) *
            MovementPolicy.SpeedMultiplier;
    }
    else if (Movement->IsFalling())
    {
        Movement->MaxWalkSpeed =
            AirBaseSpeed * MovementPolicy.SpeedMultiplier;
    }
}

/** 최종 이동 조건을 적용하고 금지된 입력·점프 예약을 정리한다. */
void ALMPlayerCharacter::ApplyFreeMovementPolicy(
    const FLMFreeMovementPolicy& NewPolicy)
{
    const bool bWasExternal = MovementPolicy.ExternalMovement;

    MovementPolicy = NewPolicy;
    MovementPolicy.SpeedMultiplier =
        FMath::Max(0.0f, MovementPolicy.SpeedMultiplier);

    if (IsPivoting() && !CanUsePivot())
    {
        CancelPivot();
    }

    // 피격·연출 등으로 제한된 달리기가 제한 해제 후 저절로 재개되지 않게 한다.
    if (!UsesFreeMovement() ||
        !MovementPolicy.AllowMove ||
        !MovementPolicy.AllowRun)
    {
        bWantsToRun = false;
    }

    if (!CanUseBasicJump())
    {
        CancelPendingJump();
    }

    if (!MovementPolicy.AllowMove || !UsesFreeMovement())
    {
        ConsumeMovementInputVector();
    }

    if (!UsesFreeMovement())
    {
        bCurrentFallFromJump = false;
    }

    if (bWasExternal &&
        UsesFreeMovement() &&
        GetCharacterMovement()->IsFalling())
    {
        CaptureAirSpeed();
    }

    RefreshMovementSettings();
}

void ALMPlayerCharacter::Restart()
{
    Super::Restart();

    if (ScanComponent)
    {
        const APlayerController* PlayerController = Cast<APlayerController>(GetController());
        const ALMPlayerCameraManager* CameraManager = PlayerController && PlayerController->IsLocalController()
            ? Cast<ALMPlayerCameraManager>(PlayerController->PlayerCameraManager) : nullptr;
        ScanComponent->InitializeScan(CameraManager ? CameraManager->GetPostProcessModifier() : nullptr);
    }
}

/** 조종 해제 시 이동·달리기 의도와 점프 예약을 비운다. */
void ALMPlayerCharacter::UnPossessed()
{
    if (ScanComponent)
    {
        ScanComponent->StopScan();
    }

    ClearMoveInput();
    CancelMoveInput();

    CancelPendingJump();
    ConsumeMovementInputVector();

    Super::UnPossessed();

    RefreshMovementSettings();
}
void ALMPlayerCharacter::RequestScan()
{
    if (ScanComponent)
    {
        ScanComponent->StartScan();
    }
}

/** 현재 피벗의 재생 인스턴스를 찾는다. */
FAnimMontageInstance* ALMPlayerCharacter::GetPivotInstance() const
{
    UAnimInstance* Anim = PivotAnimInstance.Get();

    return Anim && IsPivoting()
        ? Anim->GetMontageInstanceForID(PivotInstanceID)
        : nullptr;
}

/** 정지 / 기립 구간인지 확인한다. */
bool ALMPlayerCharacter::IsPivotRecovering() const
{
    const FAnimMontageInstance* Instance = GetPivotInstance();

    return Instance && Instance->GetCurrentSection() == LMPivot::Stop;
}

/** 현재 이동 정책과 접지 상태가 피벗을 허용하는지 검사한다. */
bool ALMPlayerCharacter::CanUsePivot() const
{
    return UsesFreeMovement() &&
        MovementPolicy.AllowMove &&
        MovementPolicy.AllowRun &&
        MovementPolicy.FaceMovement &&
        MovementPolicy.SpeedMultiplier > KINDA_SMALL_NUMBER &&
        Controller &&
        !IsMoveInputIgnored() &&
        GetCharacterMovement()->IsMovingOnGround();
}

/** 충분한 달리기 속도에서 반대 입력이 들어오면 피벗을 시작한다. */
bool ALMPlayerCharacter::TryStartPivot(const FVector& DesireDirection)
{
    if (IsPivoting() ||
        !PivotMontage ||
        !CanUsePivot() ||
        !IsRunRequestedAndAllowed() ||
        IsPreparingJump() ||
        DesireDirection.IsNearlyZero())
    {
        return false;
    }

    const float Speed = GetVelocity().Size2D();
    const float MinimumSpeed = RunSpeed * MovementPolicy.SpeedMultiplier * 0.75f;

    if (Speed < MinimumSpeed)
    {
        return false;
    }

    const float DirectionDot = FVector::DotProduct(
        GetVelocity().GetSafeNormal2D(),
        DesireDirection);

    const float RequiredDot = FMath::Cos(
        FMath::DegreesToRadians(PivotMinimumAngle));

    if (DirectionDot > RequiredDot)
    {
        return false;
    }

    UAnimInstance* Anim = GetMesh()->GetAnimInstance();

    if (!Anim)
    {
        return false;
    }

    if (!ensureMsgf(
       PivotMontage->HasRootMotion() &&
       PivotMontage->GetSectionIndex(LMPivot::Turn) == 0 &&
       PivotMontage->GetSectionIndex(LMPivot::Run) != INDEX_NONE &&
       PivotMontage->GetSectionIndex(LMPivot::Stop) != INDEX_NONE,
       TEXT("Pivot montage requires root motion and Turn/Run/Stop sections.")))
    {
        return false;
    }

    // 다른 전신 동작이나 루트 모션을 피벗으로 끊지 않는다.
    for (const FAnimMontageInstance* Other : Anim->MontageInstances)
    {
        if (Other && Other->Montage &&
            (Other->Montage->GetGroupName() ==
                PivotMontage->GetGroupName() ||
                Other->Montage->HasRootMotion()))
        {
            return false;
        }
    }

    // 지지 자세가 흔들리지 않도록 목표 방향은 시작할 때 고정한다.
    PivotMotionWarping->AddOrUpdateWarpTargetFromTransform(
        LMPivot::Facing,
        FTransform(DesireDirection.Rotation(), GetActorLocation()));

    const float PlayRate = FMath::Clamp(
        Speed / FMath::Max(PivotAnimationReferenceSpeed, 1.0f),
        0.25f,
        2.0f);

    const float Result = Anim->Montage_Play(
        PivotMontage,
        PlayRate,
        EMontagePlayReturnType::MontageLength,
        0.0f,
        false);

    FAnimMontageInstance* Instance =
        Anim->GetActiveInstanceForMontage(PivotMontage);

    if (Result <= 0.0f || !Instance)
    {
        if (Result > 0.0f)
        {
            Anim->Montage_Stop(0.0f, PivotMontage);
        }

        PivotMotionWarping->RemoveWarpTarget(LMPivot::Facing);
        return false;
    }

    PivotAnimInstance = Anim;
    PivotInstanceID = Instance->GetInstanceID();
    RunReleaseDeadline = -1.0;

    CancelPendingJump();
    ConsumeMovementInputVector();

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(
        this,
        &ThisClass::OnPivotEnded,
        PivotInstanceID);

    Anim->Montage_SetEndDelegate(EndDelegate, PivotMontage);

    FOnMontageBlendingOutStarted BlendOutDelegate;
    BlendOutDelegate.BindUObject(
        this,
        &ThisClass::OnPivotBlendingOut,
        PivotInstanceID);
    Anim->Montage_SetBlendingOutDelegate(BlendOutDelegate, PivotMontage);

    Instance->SetNextSectionName(LMPivot::Run, NAME_None);
    Instance->SetNextSectionName(LMPivot::Stop, NAME_None);

    UpdatePivotExit();
    RefreshMovementSettings();
    return true;
}

/** 회전과 재가속을 이어 재생하고, Run 끝 또는 새 입력에서 일반 이동에 연결한다. */
void ALMPlayerCharacter::UpdatePivotExit()
{
    FAnimMontageInstance* Instance = GetPivotInstance();

    if (!Instance ||
        !Instance->IsActive())
    {
        return;
    }

    const bool bHasInput = HasMoveInput();
    const bool bChangingMoveKey =
        RunReleaseDeadline >= 0.0 && GetWorld() &&
        GetWorld()->GetTimeSeconds() < RunReleaseDeadline;

    const FName Section = Instance->GetCurrentSection();
    if (Section == LMPivot::Turn)
    {
        // Shift를 다시 눌러 걷기로 바꾼 경우에도 이동 입력은 이어간다.
        Instance->SetNextSectionName(
            LMPivot::Turn,
            (bHasInput || bChangingMoveKey) ? LMPivot::Run : LMPivot::Stop);
        return;
    }

    if (Section == LMPivot::Run)
    {
        // 같은 방향을 유지하면 Run의 밀어내기·재가속 동작을 보존한다.
        // 입력을 놓거나 새 방향/걷기를 요청하면 남은 Run을 기다리지 않는다.
        const bool bChangeDirection = bHasInput &&
            FVector::DotProduct(GetMoveDirection(), GetActorForwardVector()) <
                LMPivot::RunContinuationDot;

        if ((!bHasInput && !bChangingMoveKey) ||
            (bHasInput && (!IsRunRequestedAndAllowed() || bChangeDirection)))
        {
            BlendOutPivot(PivotBlendOutSeconds);
            return;
        }

        float RunStart = 0.0f;
        float RunEnd = 0.0f;
        Instance->Montage->GetSectionStartAndEndTime(
            Instance->Montage->GetSectionIndex(LMPivot::Run), RunStart, RunEnd);

        const float PlayRate = FMath::Max(
            Instance->GetPlayRate() * Instance->Montage->RateScale,
            KINDA_SMALL_NUMBER);
        const float SecondsRemaining =
            FMath::Max(RunEnd - Instance->GetPosition(), 0.0f) / PlayRate;

        if (SecondsRemaining <= PivotRunBlendOutSeconds)
        {
            BlendOutPivot(PivotRunBlendOutSeconds);
        }
    }
    else if (Section == LMPivot::Stop && bHasInput)
    {
        // 정지 모션 중 재입력에도 즉시 이동을 재개한다.
        BlendOutPivot(PivotBlendOutSeconds);
    }
}

/** 피벗 유지 조건과 방향키 교체 중의 짧은 입력 공백을 처리한다. */
void ALMPlayerCharacter::UpdatePivot()
{
    if (RunReleaseDeadline >= 0.0 &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() >= RunReleaseDeadline)
    {
        StopSprint();
    }

    if (!IsPivoting())
    {
        return;
    }

    if (!CanUsePivot() ||
        !GetPivotInstance() ||
        PivotAnimInstance.Get() != GetMesh()->GetAnimInstance())
    {
        CancelPivot();
        return;
    }

    UpdatePivotExit();
}

/** 포즈는 블렌드 아웃하되, 멈춘 몽타주의 루트 모션과 이동 잠금은 함께 해제한다. */
void ALMPlayerCharacter::BlendOutPivot(float BlendSeconds)
{
    if (!IsPivoting())
    {
        return;
    }

    FAnimMontageInstance* Instance = GetPivotInstance();
    const int32 StoppingInstanceID = PivotInstanceID;
    if (Instance)
    {
        Instance->Stop(
            FAlphaBlend(Instance->Montage->BlendOut, FMath::Max(BlendSeconds, 0.0f)),
            true);
    }

    // 종료 콜백이 지연되는 프레임에도 같은 입력 처리에서 이동을 이어간다.
    if (PivotInstanceID == StoppingInstanceID)
    {
        FinishPivot();
    }
}

/** 피벗 상태를 해제하고 현재 이동 정책을 다시 적용한다. */
void ALMPlayerCharacter::FinishPivot()
{
    PivotInstanceID = INDEX_NONE;
    PivotAnimInstance.Reset();

    PivotMotionWarping->RemoveWarpTarget(LMPivot::Facing);

    if (!HasMoveInput() &&
        (RunReleaseDeadline < 0.0 || !GetWorld() ||
         GetWorld()->GetTimeSeconds() >= RunReleaseDeadline))
    {
        bWantsToRun = false;
        RunReleaseDeadline = -1.0;
    }

    RefreshMovementSettings();
}

/** 엔진이 루트 모션 몽타주를 해제하면 종료 포즈가 남아 있어도 이동은 재개한다. */
void ALMPlayerCharacter::OnPivotBlendingOut(
    UAnimMontage* Montage, bool bInterrupted, int32 FinishedInstanceID)
{
    if (FinishedInstanceID == PivotInstanceID)
    {
        FinishPivot();
    }
}

/**  현재 피벗의 종료 알림만 처리한다. */
void ALMPlayerCharacter::OnPivotEnded(UAnimMontage* Montage, bool bInterrupted, int32 FinishedInstanceID)
{
    if (FinishedInstanceID == PivotInstanceID)
    {
        FinishPivot();
    }
}

/** 현재 피벗만 중단하고 다른 몽타주는 건드리지 않는다. */
void ALMPlayerCharacter::CancelPivot()
{
    BlendOutPivot(PivotBlendOutSeconds);
}

/** 강제 입력 취소에서는 달리기 유예와 피벗을 즉시 정리한다. */
void ALMPlayerCharacter::CancelMoveInput()
{
    MoveAxis = FVector2D::ZeroVector;
    ConsumeMovementInputVector();
    StopSprint();
    CancelPivot();
}

/** 캐릭터 종료 시 입력과 예약 동작을 정리한다. */
void ALMPlayerCharacter::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    BlendOutPivot(0.0f);
    CancelMoveInput();
    CancelPendingJump();

    Super::EndPlay(EndPlayReason);
}
