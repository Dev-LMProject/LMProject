#include "LMPlayer/LMPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "LMCamera/LMPlayerCameraManager.h"
#include "LMComponent/LMHackingComponent.h"
#include "LMUI/LMHackingWidget.h"
#include "LMUI/LMInteractionHUDWidget.h"
#include "LMUI/LMUserHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMPlayer/LMPlayerState.h"
#include "LMPlayer/LMInputConfig.h"
#include "UObject/StrongObjectPtr.h"

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
	
	// 휠·방향키는 HUD의 선택만 이동하고, E 요청은 GAS 입력 경로에서 처리한다.
	InputComponent->BindKey(EKeys::Up, IE_Pressed,
		this, &ThisClass::SelectPreviousInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed,
		this, &ThisClass::SelectPreviousInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Down, IE_Pressed,
		this, &ThisClass::SelectNextInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed,
		this, &ThisClass::SelectNextInteractionTarget).bConsumeInput = true;
	
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
	bIsEndingPlay = true;
	ReleaseHackingWidget();
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

// 입력 GA가 위젯 구성에 의존하지 않고 현재 선택 결과를 조회하도록 연결한다.
AActor* ALMPlayerController::GetSelectedInteractionActor() const
{
	return IsValid(InteractionHUD) ? InteractionHUD->GetSelectedActor() : nullptr;
}

bool ALMPlayerController::IsHackingSessionActive(const ULMHackingComponent* InSession) const
{
	// 같은 컨트롤러를 사용하는 다른 해킹 대상의 입력을 현재 화면에 적용하지 않는다.
	return IsValid(InSession)
		&& !InSession->IsBeingDestroyed()
		&& ActiveHackingSession.Get() == InSession
		&& IsValid(ActiveHackingWidget)
		&& ActiveHackingWidget->GetOwningPlayer() == this
		&& ActiveHackingWidget->IsInViewport();
}

bool ALMPlayerController::IsHackingWidgetSource(const ULMHackingComponent* InSession,
	const ULMHackingWidget* SourceWidget) const
{
	// 현재 세션에 연결된 화면에서 보낸 요청인지 확인한다.
	return IsValid(InSession)
		&& IsValid(SourceWidget)
		&& ActiveHackingSession.Get() == InSession
		&& ActiveHackingWidget.Get() == SourceWidget;
}

bool ALMPlayerController::OpenHackingWidget(ULMHackingComponent* InSession)
{
	// 화면을 표시할 로컬 컨트롤러이며, 다른 화면 전환 중이 아니어야 한다.
	if (!IsLocalPlayerController() || IsActorBeingDestroyed() || bIsEndingPlay || bIsChangingHackingWidget)
	{
		return false;
	}

	// 화면에 연결할 컴포넌트와 생성할 위젯 클래스가 필요하다.
	if (!IsValid(InSession) || InSession->IsBeingDestroyed() || !HackingWidgetClass)
	{
		return false;
	}

	// 이전 화면이나 정리되지 않은 세션을 덮어쓰지 않는다.
	if (ActiveHackingWidget != nullptr || !ActiveHackingSession.IsExplicitlyNull())
	{
		return false;
	}

	// 함수 실행 중 true로 유지하고, 함수를 벗어나면 이전 값으로 복원한다.
	TGuardValue<bool> ChangingGuard(bIsChangingHackingWidget, true);

	// 생성 콜백 이후에는 이 약한 참조로 컴포넌트를 다시 확인한다.
	const TWeakObjectPtr<ULMHackingComponent> RequestedSession(InSession);
	ActiveHackingSession = RequestedSession;

	// 생성 중 취소되더라도 실패 정리가 끝날 때까지 후보 위젯을 유지한다.
	TStrongObjectPtr<ULMHackingWidget> NewWidget(CreateWidget<ULMHackingWidget>(this, HackingWidgetClass));

	// 각 콜백 이후에도 이번 요청의 화면과 세션이 유지되는지 확인한다.
	const auto IsOpeningSessionValid = [this, &RequestedSession, &NewWidget]()
	{
		return !bIsEndingPlay
			&& !IsActorBeingDestroyed()
			&& ActiveHackingWidget == NewWidget.Get()
			&& IsHackingSessionActive(RequestedSession.Get());
	};
	
	bool bOpened = false;
	ULMHackingComponent* Session = RequestedSession.Get();

	if (IsValid(NewWidget.Get()) && IsValid(Session) && !Session->IsBeingDestroyed() && !bIsEndingPlay && !IsActorBeingDestroyed() && ActiveHackingSession.HasSameIndexAndSerialNumber(RequestedSession))
	{
		ActiveHackingWidget = NewWidget.Get();

		// 화면에 등록하기 전에 위젯이 사용할 해킹 컴포넌트를 전달한다.
		NewWidget->InitializeForSession(Session);

		const bool bAddedToScreen = NewWidget->AddToPlayerScreen(10);

		bOpened = bAddedToScreen && IsOpeningSessionValid();
	}

	if (bOpened)
	{
		// 이후 처리에서 취소되더라도 복원할 수 있도록 먼저 저장한다.
		bMouseCursorBeforeHacking = bShowMouseCursor;
		bHasHackingInputState = true;
		bPausedByHacking = false;
		
		// 이미 정지된 상태라면 해킹이 새로 일시정지를 요청하지 않는다.
		if (!IsPaused())
		{
			bPausedByHacking = SetPause(true);
			bOpened = bPausedByHacking && IsPaused();
		}

		bOpened = bOpened && IsOpeningSessionValid();
	}

	if (bOpened)
	{
		// 기존 오버라이드가 GAS 입력 취소와 눌린 키 초기화를 함께 처리한다.
		FlushPressedKeys();
		bOpened = IsOpeningSessionValid();
	}

	if (bOpened)
	{
		bShowMouseCursor = true;

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(NewWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		bOpened = IsOpeningSessionValid();
	}

	if (!bOpened)
	{
		ActiveHackingWidget = nullptr;
		ActiveHackingSession.Reset();

		// 생성·등록 호출이 끝난 뒤 실패한 화면을 제거한다.
		if (IsValid(NewWidget.Get()))
		{
			NewWidget->ClearSession();
			NewWidget->RemoveFromParent();
		}

		// 일시정지나 입력 전환 이후 실패했다면 원래 상태로 돌린다.
		RestoreHackingInput();
	}

	return bOpened;
}

void ALMPlayerController::CloseHackingWidget(ULMHackingComponent* InSession)
{
	if (InSession == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<ULMHackingComponent> RequestedSession(InSession);

	// 다른 해킹 대상의 요청으로 현재 화면을 닫지 않는다.
	if (!ActiveHackingSession.HasSameIndexAndSerialNumber(RequestedSession))
	{
		return;
	}

	ReleaseHackingWidget();
}

void ALMPlayerController::ReleaseHackingWidget()
{
	// 생성 중이라면 이 초기화가 현재 열기의 취소 표시가 된다.
	ActiveHackingSession.Reset();

	if (bIsChangingHackingWidget)
	{
		// Open이 생성·등록 호출을 빠져나온 뒤 화면 제거와 복원을 처리한다.
		return;
	}

	TGuardValue<bool> ChangingGuard(bIsChangingHackingWidget, true);
	TStrongObjectPtr<ULMHackingWidget> ClosingWidget(ActiveHackingWidget.Get());

	ActiveHackingWidget = nullptr;

	if (IsValid(ClosingWidget.Get()))
	{
		ClosingWidget->ClearSession();
		ClosingWidget->RemoveFromParent();
	}

	RestoreHackingInput();
}

void ALMPlayerController::RestoreHackingInput()
{
	if (!bHasHackingInputState)
	{
		return;
	}

	const bool bReleasePause = bPausedByHacking;
	const bool bRestoreCursor = bMouseCursorBeforeHacking;

	// 아래 콜백에서 정리가 다시 호출되어도 중복 복원하지 않는다.
	bHasHackingInputState = false;
	bPausedByHacking = false;

	FlushPressedKeys();
	bShowMouseCursor = bRestoreCursor;

	if (GetWorld())
	{
		// 현재 프로젝트의 일반 게임 입력 상태로 돌아간다.
		SetInputMode(FInputModeGameOnly());

		// 해킹 진입에서 새로 정지시켰던 경우에만 해제를 요청한다.
		if (bReleasePause && IsPaused())
		{
			SetPause(false);
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
