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
#include "Abilities/GameplayAbility.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMComponent/LMTargetComponent.h"

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

// 플레이 종료 시 포커스·해킹 화면·HUD·입력 연결을 정리한다.
void ALMPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 종료 중 새 포커스 갱신이 실행되지 않도록 먼저 막는다.
	bIsEndingPlay = true;
	ClearFocus();
	
	// Controller 종료 시에는 공격 소유 정보까지 모두 비운다.
	AttackFocusOwner.Reset();
	AttackFocusTarget.Reset();
	
	OnFocusFrame.Clear();

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
		UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

		// 이 컨트롤러가 등록한 입력 매핑만 제거한다.
		if (Subsystem && DefaultMappingContext)
			Subsystem->RemoveMappingContext(DefaultMappingContext);
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
// 입력이 중단되는 UI 전환에서 능력 입력과 이전 포커스 표시를 정리한다.
void ALMPlayerController::CancelAbilityInput()
{
	ClearFocus();

	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
		ASC->ClearAbilityInput();
}

/** 키 초기화를 정상적인 버튼 해제로 해석하지 않도록 먼저 능력 입력을 취소한다. */
void ALMPlayerController::FlushPressedKeys()
{
	CancelAbilityInput();

	Super::FlushPressedKeys();
}

// 이번 프레임 카메라 → 대상 검색 → 표식 위치 갱신 순서를 만든다.
void ALMPlayerController::UpdateCameraManager(float DeltaSeconds)
{
	// 부모 함수가 PlayerCameraManager의 카메라 위치·회전 등을 갱신한다.
	// 검색보다 먼저 호출해야 이번 프레임 카메라로 후보를 판단할 수 있다.
	Super::UpdateCameraManager(DeltaSeconds);

	if (bIsEndingPlay) return;

	// 구독 중인 Task의 UpdateFocus가 호출되고 Controller의 후보·선택이 교체된다.
	OnFocusFrame.Broadcast();

	// 검색 결과가 반영된 다음 현재 선택의 표식 위치를 갱신한다.
	// 선택이 그대로여도 카메라나 대상이 움직일 수 있으므로 매 프레임 호출한다.
	if (UserHUD)
		UserHUD->UpdateFocusMarker(GetFocusTarget());
}

// 게임이 멈추면 카메라 갱신도 멈출 수 있으므로 여기서 표식까지 비운다.
bool ALMPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
	// 실제 일시정지 처리는 기존 엔진 함수에 맡기고 성공 여부를 받는다.
	const bool bSucceeded = Super::SetPause(bPause, CanUnpauseDelegate);

	// 일시정지 요청이 성공했을 때만 즉시 목록·선택·표식을 비운다.
	if (bPause && bSucceeded)
		ClearFocus();

	return bSucceeded;
}

// 현재 화면이 플레이어 캐릭터의 일반 플레이 화면인지 확인한다.
bool ALMPlayerController::CanUpdateFocus() const
{
	if (bIsEndingPlay) return false;
	if (!IsLocalPlayerController()) return false;
	if (IsPaused()) return false;

	// 검색 원점과 화면 투영을 얻을 캐릭터·카메라가 모두 필요하다.
	if (!IsValid(GetPawn()) || !IsValid(PlayerCameraManager))
		return false;

	// 해킹 위젯이 있거나 생성·제거 중이면 일반 플레이 표식을 표시하지 않는다.
	if (IsValid(ActiveHackingWidget) || bIsChangingHackingWidget)
		return false;

	// 다른 Actor를 보는 카메라로 전환된 동안에는 일반 플레이 포커스를 멈춘다.
	if (GetViewTarget() != GetPawn()) return false;

	const ALMPlayerCameraManager* Camera = Cast<ALMPlayerCameraManager>(PlayerCameraManager);

	// 기존 CameraManager가 재생 중인 시퀀스 연출도 같은 방식으로 제외한다.
	if (Camera && Camera->IsSequenceCameraActive())
		return false;

	return true;
}

// 저장된 약한 참조에서 현재 사용할 수 있는 선택 Actor만 반환한다.
AActor* ALMPlayerController::GetFocusTarget() const
{
	AActor* Target = FocusTarget.Get();

	// 삭제됐거나 파괴 처리가 시작된 Actor를 다른 기능이 사용하지 않게 한다.
	if (!IsValid(Target) || Target->IsActorBeingDestroyed())
		return nullptr;

	return Target;
}

// 내부 후보 배열을 직접 노출하지 않고 유효한 Actor만 담은 복사본을 반환한다.
TArray<AActor*> ALMPlayerController::GetFocusCandidates() const
{
	TArray<AActor*> Result;
	Result.Reserve(FocusCandidates.Num());

	for (const TWeakObjectPtr<AActor>& Candidate : FocusCandidates)
	{
		AActor* Target = Candidate.Get();

		// 검색 이후 삭제된 Actor가 있을 수 있으므로 조회할 때도 확인한다.
		if (IsValid(Target) && !Target->IsActorBeingDestroyed())
			Result.Add(Target);
	}

	return Result;
}

// 새 후보와 새 선택을 모두 저장한 뒤 선택 변경 알림을 보낸다.
void ALMPlayerController::SetFocusResult(const TArray<TWeakObjectPtr<AActor>>& _FocusCandidates, AActor* _FocusTarget)
{
	// 갱신 전후에 선택이 달라졌는지 비교하기 위해 이전 약한 참조를 보관한다.
	const TWeakObjectPtr<AActor> PreviousTarget = FocusTarget;

	// 이번 검색 결과로 교체하므로 이전 배열에 계속 추가하지 않는다.
	FocusCandidates.Reset();

	for (const TWeakObjectPtr<AActor>& Candidate : _FocusCandidates)
	{
		AActor* Target = Candidate.Get();

		// 이미 파괴된 Actor는 저장하지 않고 같은 Actor가 중복 전달돼도 한 번만 넣는다.
		if (IsValid(Target) && !Target->IsActorBeingDestroyed())
			FocusCandidates.AddUnique(Target);
	}

	// 이전 선택을 먼저 지우고 새 후보 안에 있는 대상만 다시 선택한다.
	FocusTarget.Reset();
	
	// 새로 선정된 대상이 후보 목록에 있을 때 현재 포커스로 저장한다.
	const TWeakObjectPtr<AActor> NewTarget(_FocusTarget);
	if (IsValid(_FocusTarget) && FocusCandidates.Contains(NewTarget))
		FocusTarget = NewTarget;

	// 이번 타의 대상이 후보에서 빠졌다면 표식과 공격 대상을 함께 비운다.
	// GA 소유 정보는 남겨, 공격 도중 다른 몬스터가 대신 선택되는 것을 막는다.
	if (IsAttackFocusActive() && FocusTarget != AttackFocusTarget)
	{
		AttackFocusTarget.Reset();
		FocusTarget.Reset();
	}

	// 공격 중에는 표식 대상과 이번 타의 대상이 반드시 같아야 한다.
	checkSlow(!IsAttackFocusActive() || FocusTarget == AttackFocusTarget);

	// 이 검사가 활성화된 빌드에서 '선택은 후보 안에 있어야 한다'는 규칙을 확인한다.
	checkSlow(!FocusTarget.IsValid() || FocusCandidates.Contains(FocusTarget));

	// Get() 결과만 비교하면 파괴된 이전 대상과 새 nullptr이 같아 보일 수 있다.
	// 약한 참조 자체를 비교해 그 경우에도 선택 해제 알림을 보낸다.
	if (PreviousTarget != FocusTarget)
		OnFocusChanged.Broadcast(FocusTarget.Get());
}

// 대상이 없는 검색 결과를 저장하고 화면에 남아 있는 표식을 숨긴다.
void ALMPlayerController::ClearFocus()
{
	const TArray<TWeakObjectPtr<AActor>> EmptyCandidates;

	// 빈 목록과 nullptr도 같은 갱신 함수를 사용해 변경 알림 규칙을 유지한다.
	SetFocusResult(EmptyCandidates, nullptr);

	// 일시정지나 종료 중에는 다음 카메라 프레임이 없을 수 있어 즉시 숨긴다.
	if (UserHUD)
		UserHUD->UpdateFocusMarker(nullptr);
}

// 현재 플레이어가 실행 중인 공격만 대상을 등록하고, 표식도 같은 대상으로 맞춘다.
void ALMPlayerController::BeginAttackFocus(UGameplayAbility* _AttackFocusOwner, AActor* _AttackFocusTarget)
{
    if (!IsValid(_AttackFocusOwner) || !_AttackFocusOwner->IsActive() || _AttackFocusOwner->GetAvatarActorFromActorInfo() != GetPawn())
    {
        return;
    }

    // nullptr도 저장한다. 대상 없이 시작한 공격이 도중에 옆 몬스터를 잡지 않게 한다.
    AttackFocusOwner = _AttackFocusOwner;
    AttackFocusTarget = _AttackFocusTarget;

    // 현재 표식과 다르거나 공격할 수 없는 대상은 받아들이지 않는다.
    if (!GetAttackFocusTarget())
        AttackFocusTarget.Reset();

    // SetFocusResult가 내부 후보 배열을 비우므로, 같은 배열을 인자로 넘기지 않고 복사한다.
    const TArray<TWeakObjectPtr<AActor>> SavedCandidates = FocusCandidates;
    SetFocusResult(SavedCandidates, AttackFocusTarget.Get());
}

// 자신이 시작한 공격의 보관 상태만 해제하고, 일반 포커스 선정은 다음 Task 갱신에 맡긴다.
void ALMPlayerController::EndAttackFocus(const UGameplayAbility* _AttackFocusOwner)
{
    if (!_AttackFocusOwner) return;
    if (AttackFocusOwner.Get() != _AttackFocusOwner) return;

    AttackFocusOwner.Reset();
    AttackFocusTarget.Reset();
}

// 이미 끝난 GA나 다른 캐릭터의 GA를 공격 중인 상태로 취급하지 않는다.
bool ALMPlayerController::IsAttackFocusActive() const
{
    const UGameplayAbility* OwnerAbility = AttackFocusOwner.Get();

    return OwnerAbility && OwnerAbility->IsActive() && OwnerAbility->GetAvatarActorFromActorInfo() == GetPawn();
}

// 카메라와 이동 보정이 죽었거나 비활성화된 대상을 계속 사용하지 않도록 확인한다.
AActor* ALMPlayerController::GetAttackFocusTarget() const
{
    if (!IsAttackFocusActive() || !CanUpdateFocus() || AttackFocusTarget != FocusTarget)
    {
        return nullptr;
    }

    ALMBaseCharacter* Attacker = Cast<ALMBaseCharacter>(GetPawn());
    ALMBaseCharacter* Target = Cast<ALMBaseCharacter>(AttackFocusTarget.Get());
    const ULMTargetComponent* Point = Target ? Target->FindComponentByClass<ULMTargetComponent>() : nullptr;

    // 일반 중요 오브젝트는 포커스할 수 있어도, 현재 지상 평타의 적 대상은 아니다.
    if (!Attacker || !Target || !Point || !Point->CanFocus() || !Target->CanBeAttackTargetFor(Attacker))
    {
        return nullptr;
    }

    return Target;
}