#include "LMCharacter/LMPlayerCharacter.h"

#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "LMComponent/LMInteractorComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"

/**
 * 플레이어의 충돌, 이동 방식, 기본 카메라를 구성한다.
 *
 * 캐릭터는 컨트롤러의 수평 시점 방향을 바라본다.
 * 따라서 S를 누르면 몸을 돌리지 않고 후진할 수 있다.
 */
ALMPlayerCharacter::ALMPlayerCharacter()
{
    // 캡슐이 캐릭터의 이동 충돌을 담당한다.
    // 두 번째 값 90은 전체 높이가 아니라 반높이다.
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 90.0f);

    // 몸은 좌우 회전만 컨트롤러를 따라간다.
    // 카메라를 위아래로 돌려도 몸이 앞으로 기울지 않게 한다.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();

    // 입력 방향으로 몸을 돌리는 기능을 끈다.
    // S 입력 시 뒤를 돌아보지 않고 뒤로 이동하게 하는 핵심 설정이다.
    Movement->bOrientRotationToMovement = false;

    // 몸의 좌우 회전은 위의 bUseControllerRotationYaw로 처리한다.
    Movement->bUseControllerDesiredRotation = false;

    // 이동 조작감을 확인하기 위한 프로토타입 값.
    // MaxWalkSpeed의 단위는 cm/s이다.
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 2048.0f;
    Movement->BrakingDecelerationWalking = 1600.0f;

    // 점프 초기 상승 속도, 공중 방향 조작 정도, 중력 배율.
    Movement->JumpZVelocity = 500.0f;
    Movement->AirControl = 0.25f;
    Movement->GravityScale = 1.0f;

    // 기본 점프 한 번만 허용한다.
    JumpMaxCount = 1;

    // 버튼을 오래 눌러도 점프 높이가 추가로 늘어나지 않게 한다.
    JumpMaxHoldTime = 0.0f;

    // 캐릭터 뒤에 카메라를 배치할 SpringArm을 만든다.
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom"));

    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.0f;
    CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 60.0f);

    // 마우스 입력으로 변경한 컨트롤러 회전을 사용한다.
    CameraBoom->bUsePawnControlRotation = true;

    // 벽이 캐릭터와 카메라 사이에 들어오면 카메라 거리를 줄인다.
    CameraBoom->bDoCollisionTest = true;

    // SpringArm 끝에 실제 카메라를 연결한다.
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("FollowCamera"));

    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName);

    // 카메라 회전은 부모인 SpringArm을 따라간다.
    FollowCamera->bUsePawnControlRotation = false;

    // BP가 추가한 콜리전을 연결할 목록 관리 컴포넌트를 준비한다.
    InteractorComponent = CreateDefaultSubobject<ULMInteractorComponent>(TEXT("InteractorComponent"));
}

/**
 * 입력 액션과 실제 처리 함수를 연결한다.
 *
 * 키 배치 자체는 PlayerController가 등록하고,
 * 여기서는 액션이 발생했을 때 호출할 함수를 지정한다.
 */
void ALMPlayerCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* Input =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!ensureMsgf(
        Input,
        TEXT("Enhanced Input Component is required.")))
    {
        return;
    }

    // BP에서 입력 에셋을 빠뜨렸다면 원인을 로그에 표시한다.
    if (!ensureMsgf(
        MoveAction && LookAction && JumpAction && SprintAction,
        TEXT("Assign MoveAction, LookAction, JumpAction and SprintAction in the player BP.")))
    {
        return;
    }

    // 이동 입력이 들어오는 동안 이동 방향을 전달한다.
    Input->BindAction(
        MoveAction,
        ETriggerEvent::Triggered,
        this,
        &ALMPlayerCharacter::Move);

    // 마우스 이동이 발생하면 시점을 회전한다.
    Input->BindAction(
        LookAction,
        ETriggerEvent::Triggered,
        this,
        &ALMPlayerCharacter::Look);

    // 점프 버튼을 누른 순간 한 번 요청한다.
    Input->BindAction(
        JumpAction,
        ETriggerEvent::Started,
        this,
        &ALMPlayerCharacter::RequestJump);

    // 일반적으로 버튼을 놓았을 때 점프 입력을 해제한다.
    Input->BindAction(
        JumpAction,
        ETriggerEvent::Completed,
        this,
        &ALMPlayerCharacter::ReleaseJumpInput);

    // 트리거 평가가 취소되는 경로에서도 입력을 해제한다.
    Input->BindAction(
        JumpAction,
        ETriggerEvent::Canceled,
        this,
        &ALMPlayerCharacter::ReleaseJumpInput);

    // 입력 취소 시에도 걷기로 복귀시켜 달리기 상태가 남지 않게 한다.
    Input->BindAction(SprintAction, ETriggerEvent::Started,
        this, &ALMPlayerCharacter::StartSprint);
    Input->BindAction(SprintAction, ETriggerEvent::Completed,
        this, &ALMPlayerCharacter::StopSprint);
    Input->BindAction(SprintAction, ETriggerEvent::Canceled,
        this, &ALMPlayerCharacter::StopSprint);
}

/**
 * 카메라의 수평 방향을 기준으로 이동한다.
 *
 * 입력 X: 좌우 이동
 * 입력 Y: 전진과 후진
 */
void ALMPlayerCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller)
    {
        return;
    }

    // 대각선 입력도 입력 크기가 1을 넘지 않게 한다.
    const FVector2D MoveAxis =
        Value.Get<FVector2D>().GetClampedToMaxSize(1.0f);

    // 위아래 시점인 Pitch는 제외하고 좌우 회전인 Yaw만 사용한다.
    const FRotator YawRotation(
        0.0f,
        Controller->GetControlRotation().Yaw,
        0.0f);

    const FVector ForwardDirection =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

    const FVector RightDirection =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    // 실제 위치 변경, 가속과 충돌은 CharacterMovement가 처리한다.
    // S는 MoveAxis.Y가 음수이므로 앞 방향의 반대로 이동한다.
    AddMovementInput(ForwardDirection, MoveAxis.Y);
    AddMovementInput(RightDirection, MoveAxis.X);
}

/** 마우스 이동량을 시점 회전에 반영한다. */
void ALMPlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookAxis = Value.Get<FVector2D>();

    // 마우스 이동량 입력이므로 DeltaTime을 추가로 곱하지 않는다.
    AddControllerYawInput(LookAxis.X);
    AddControllerPitchInput(LookAxis.Y);
}

/** 지상에서만 기본 점프를 요청한다. */
void ALMPlayerCharacter::RequestJump()
{
    if (!GetCharacterMovement()->IsMovingOnGround())
    {
        return;
    }

    // 엔진에 실제 점프를 요청한다.
    // 애니메이션은 AnimBP가 공중 상태를 읽어 별도로 재생한다.
    Jump();
}

/** 점프 버튼의 유지 상태를 해제한다. */
void ALMPlayerCharacter::ReleaseJumpInput()
{
    // 공중 속도를 즉시 0으로 만드는 함수가 아니다.
    StopJumping();
}

void ALMPlayerCharacter::StartSprint()
{
    bIsSprinting = true;
    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ALMPlayerCharacter::StopSprint()
{
    bIsSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ALMPlayerCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    if (LandingMontage)
    {
        // AnimGraph의 DefaultSlot을 통해 현재 이동 포즈 위에 재생한다.
        PlayAnimMontage(LandingMontage);
    }
}

void ALMPlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

    // 재점프·낙하 자세가 착지 몽타주에 가려지지 않도록 중단한다.
    if (LandingMontage && GetCharacterMovement()->IsFalling())
    {
        StopAnimMontage(LandingMontage);
    }
}
