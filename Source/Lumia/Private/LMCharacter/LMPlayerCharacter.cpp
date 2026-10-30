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
    
    if (ScanComponent)
    {
        ScanComponent->InitializeScan(FollowCamera);
    }
}

/** 예약된 점프를 검사하고 게임 카메라의 줌을 갱신한다. */
void ALMPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

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
        this, &ThisClass::ClearMoveInput);

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

/** 카메라의 수평 방향을 기준으로 이동 입력을 전달한다. */
void ALMPlayerCharacter::Move(const FInputActionValue& Value)
{
    // 대각선 입력의 크기가 직선 입력보다 커지지 않도록 제한한다.
    MoveAxis = Value.Get<FVector2D>().GetClampedToMaxSize(1.0f);

    if (!HasMoveInput())
    {
        StopSprint();
        return;
    }

    const FRotator YawRotation(
        0.0f, Controller->GetControlRotation().Yaw, 0.0f);

    const FRotationMatrix CameraAxes(YawRotation);

    AddMovementInput(
        CameraAxes.GetUnitAxis(EAxis::X), MoveAxis.Y);

    AddMovementInput(
        CameraAxes.GetUnitAxis(EAxis::Y), MoveAxis.X);
}

/** 이동 입력과 달리기 토글을 비운다. 실제 속도는 CharacterMovement가 감속시킨다. */
void ALMPlayerCharacter::ClearMoveInput()
{
    MoveAxis = FVector2D::ZeroVector;
    StopSprint();
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

    RefreshMovementSettings();
}

/** 달리기 토글을 해제하되 이륙 당시 공중 속도 기준은 유지한다. */
void ALMPlayerCharacter::StopSprint()
{
    bWantsToRun = false;

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
        MovementPolicy.AllowMove;

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

/** 조종 해제 시 이동·달리기 의도와 점프 예약을 비운다. */
void ALMPlayerCharacter::UnPossessed()
{
    if (ScanComponent)
    {
        ScanComponent->StopScan();
    }

    ClearMoveInput();

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
