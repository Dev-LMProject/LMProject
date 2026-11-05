#include "LMPlayer/LMPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "LMCamera/LMPlayerCameraManager.h"
#include "LMUI/LMInteractionHUDWidget.h"
#include "LMUI/LMUserHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMPlayer/LMPlayerState.h"
#include "LMPlayer/LMInputConfig.h"

ALMPlayerController::ALMPlayerController()
{
	PlayerCameraManagerClass = ALMPlayerCameraManager::StaticClass();
}

void ALMPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() &&
		ensureMsgf(UserHUDClass, TEXT("Assign UserHUDClass in the player controller BP.")))
	{
		UserHUD = CreateWidget<ULMUserHUDWidget>(this, UserHUDClass);
		if (UserHUD)
		{
			InteractionHUD = UserHUD->GetInteractionHUD();
			UserHUD->AddToPlayerScreen();
		}
	}
}

// 로컬 플레이어에게 기본 키 배치를 등록한다.
void ALMPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// 이 컴퓨터에서 직접 조종하는 플레이어만 처리한다.
	if (!IsLocalPlayerController())
	{
		return;
	}
	
	BindAbilityInputs();
	
	// 컨트롤러에서 소비해 캐릭터 BP의 임시 E 입력과 중복 실행되지 않게 한다.
	InputComponent->BindKey(EKeys::Up, IE_Pressed,
		this, &ThisClass::SelectPreviousInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed,
		this, &ThisClass::SelectPreviousInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Down, IE_Pressed,
		this, &ThisClass::SelectNextInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed,
		this, &ThisClass::SelectNextInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::E, IE_Pressed,
		this, &ThisClass::InteractWithSelectedTarget).bConsumeInput = true;
	
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	
	// 입력 시스템과 BP에서 지정한 매핑 에셋을 확인한다.
	if (!ensureMsgf(
		Subsystem && DefaultMappingContext,
		TEXT("Check the input subsystem and DefaultMappingContext.")))
	{
		return;
	}
	
	// 기본 플레이어 입력 매핑을 우선순위 0으로 등록한다.
	Subsystem->AddMappingContext(DefaultMappingContext, 0);
	
	// 마우스를 게임 시점 조작에 사용한다.
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

// 플레이 종료 또는 컨트롤러 제거 시 입력 매핑을 정리한다.
void ALMPlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	CancelAbilityInput();
	
	if (UserHUD)
	{
		UserHUD->RemoveFromParent();
		UserHUD = nullptr;
	}
	
	InteractionHUD = nullptr;

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

		if (Subsystem && DefaultMappingContext)
		{
			// 이 컨트롤러가 사용한 매핑만 제거한다.
			Subsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ALMPlayerController::SelectPreviousInteractionTarget()
{
	if (InteractionHUD)
	{
		InteractionHUD->MoveSelection(-1);
	}
}

void ALMPlayerController::SelectNextInteractionTarget()
{
	if (InteractionHUD)
	{
		InteractionHUD->MoveSelection(1);
	}
}

void ALMPlayerController::InteractWithSelectedTarget()
{
	if (InteractionHUD)
	{
		const FLMInteractionResult Result = InteractionHUD->TryInteractSelected();
		if (Result.Failure != InteractionFailure::Succsess)
		{
			UE_LOG(LogTemp, Verbose, TEXT("Interaction request failed: %d"),
				static_cast<int32>(Result.Failure));
		}
	}
}

/** 현재 PlayerState가 소유한 프로젝트 ASC를 반환한다. */
ULMAbilitySystemComponent* ALMPlayerController::GetLMAbilitySystemComponent() const
{
	const ALMPlayerState* State = GetPlayerState<ALMPlayerState>();

	return State
		? Cast<ULMAbilitySystemComponent>(State->GetAbilitySystemComponent())
		: nullptr;
}

/** 설정 애셋의 각 입력을 시작·해제·취소 처리 함수에 연결한다. */
void ALMPlayerController::BindAbilityInputs()
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	if (!ensureMsgf(EnhancedInput, TEXT("LMPlayerController requires EnhancedInputComponent.")))
	{
		return;
	}

	if (!ensureMsgf(InputConfig, TEXT("Assign InputConfig in the PlayerController Blueprint.")))
	{
		return;
	}

	for (const FLMAbilityInputBinding& Binding : InputConfig->AbilityInputs)
	{
		if (!Binding.InputAction || !Binding.InputTag.IsValid())
		{
			continue;
		}

		// 정상 해제와 입력 취소를 구분하여 홀드 판정이 잘못 확정되지 않게 한다.
		EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Started, this, &ThisClass::OnAbilityInputPressed, Binding.InputTag);
		EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this, &ThisClass::OnAbilityInputReleased, Binding.InputTag);
		EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Canceled, this, &ThisClass::OnAbilityInputCanceled, Binding.InputTag);
	}
}

/** 입력 시작을 해당 입력 태그와 함께 ASC에 전달한다. */
void ALMPlayerController::OnAbilityInputPressed(FGameplayTag InputTag)
{
	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

/** 정상적인 입력 해제를 ASC에 전달한다. */
void ALMPlayerController::OnAbilityInputReleased(FGameplayTag InputTag)
{
	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

/** 중단된 입력을 정상 해제와 구분하여 ASC에 전달한다. */
void ALMPlayerController::OnAbilityInputCanceled(FGameplayTag InputTag)
{
	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->AbilityInputTagCanceled(InputTag);
	}
}

/** 이번 프레임에 수집된 능력 입력 ASC에서 처리한다. */
void ALMPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->ProcessAbilityInput(bGamePaused);
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

/** UI 전환 등으로 중단된 능력 입력을 취소한다. */
void ALMPlayerController::CancelAbilityInput()
{
	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->ClearAbilityInput();
	}
}

/** 키 초기화를 정상적인 버튼 해제로 해석하지 않도록 먼저 능력 입력을 취소한다. */
void ALMPlayerController::FlushPressedKeys()
{
	CancelAbilityInput();

	Super::FlushPressedKeys();
}