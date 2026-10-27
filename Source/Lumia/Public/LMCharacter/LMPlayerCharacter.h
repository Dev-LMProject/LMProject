#pragma once

#include "CoreMinimal.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UAnimMontage;
struct FInputActionValue;

/**
 * 플레이어가 조종하는 캐릭터.
 *
 * 입력을 받아 CharacterMovement에 이동과 점프를 요청하고,
 * 기본 3인칭 카메라를 구성한다.
 */
UCLASS()
class LUMIA_API ALMPlayerCharacter : public ALMBaseCharacter
{
	GENERATED_BODY()

public:
	/** 캡슐, 이동 기본값, 카메라를 구성한다. */
	ALMPlayerCharacter();

protected:
	/** 캐릭터의 입력 액션과 처리 함수를 연결한다. */
	virtual void SetupPlayerInputComponent(
		UInputComponent* PlayerInputComponent) override;

	/** 실제 지면 접촉 시 착지 몽타주를 한 번 재생한다. */
	virtual void Landed(const FHitResult& Hit) override;

	/** 재점프하거나 다시 떨어지면 착지 몽타주를 중단한다. */
	virtual void OnMovementModeChanged(
		EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

private:
	/** 카메라의 수평 방향을 기준으로 이동을 요청한다. */
	void Move(const FInputActionValue& Value);

	/** 마우스 입력으로 시점을 회전한다. */
	void Look(const FInputActionValue& Value);

	/** 지상에서 기본 점프를 요청한다. */
	void RequestJump();

	/** 점프 입력의 유지 상태를 해제한다. */
	void ReleaseJumpInput();

	/** 달리기 상태와 이동 속도를 함께 적용한다. */
	void StartSprint();

	/** 달리기 입력이 해제되거나 취소되면 걷기로 복귀한다. */
	void StopSprint();

	static constexpr float WalkSpeed = 240.0f;
	static constexpr float SprintSpeed = 480.0f;

	/** AnimBP가 Walk / Run을 선택할 때 읽는 달리기 상태. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement",
		meta = (AllowPrivateAccess = "true"))
	bool bIsSprinting = false;

	/** 카메라 거리와 카메라의 벽 충돌을 처리한다. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Camera",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 기본 플레이 시점이며 카메라 연출 종료 후 복귀할 카메라. */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Camera",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** BP의 Class Defaults에서 지정할 이동 입력 에셋. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** BP의 Class Defaults에서 지정할 시점 입력 에셋. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	/** BP의 Class Defaults에서 지정할 점프 입력 에셋. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/** BP의 Class Defaults에서 IA_LM_Sprint를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	/** BP에서 지정할 1회성 착지 몽타주. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> LandingMontage;
};
