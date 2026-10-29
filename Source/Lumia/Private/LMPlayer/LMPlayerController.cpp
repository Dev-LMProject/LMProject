#include "LMPlayer/LMPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "LMCamera/LMPlayerCameraManager.h"
#include "LMUI/LMInteractionHUDWidget.h"
#include "LMUI/LMUserHUDWidget.h"

ALMPlayerController::ALMPlayerController()
{
	PlayerCameraManagerClass = ALMPlayerCameraManager::StaticClass();
}

void ALMPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		UserHUD = CreateWidget<ULMUserHUDWidget>(this, ULMUserHUDWidget::StaticClass());
		if (UserHUD)
		{
			InteractionHUD = UserHUD->GetInteractionHUD();
			if (InteractionHUD)
			{
				InteractionHUD->SetInteractionActionTags(InteractionActionTags);
			}
			UserHUD->AddToPlayerScreen();
		}
	}
}

/** 로컬 플레이어에게 기본 키 배치를 등록한다. */
void ALMPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// 이 컴퓨터에서 직접 조종하는 플레이어만 처리한다.
	if (!IsLocalPlayerController())
	{
		return;
	}

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

/** 플레이 종료 또는 컨트롤러 제거 시 입력 매핑을 정리한다. */
void ALMPlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
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

void ALMPlayerController::SetInteractionActionTags(FGameplayTagContainer InTags)
{
	InteractionActionTags = MoveTemp(InTags);
	if (InteractionHUD)
	{
		InteractionHUD->SetInteractionActionTags(InteractionActionTags);
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
