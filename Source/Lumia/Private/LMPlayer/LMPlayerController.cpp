#include "LMPlayer/LMPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "LMCamera/LMPlayerCameraManager.h"

ALMPlayerController::ALMPlayerController()
{
	PlayerCameraManagerClass = ALMPlayerCameraManager::StaticClass();
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
