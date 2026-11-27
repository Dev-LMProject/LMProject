#include "LMPlayer/LMPlayerController.h"
#include "LMQTE/LMQTEComponent.h"
#include "Blueprint/UserWidget.h"

#include "Engine/LocalPlayer.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "LMCamera/LMPlayerCameraManager.h"
#include "LMCamera/LMTargetQueryLibrary.h"
#include "LMComponent/LMHackingComponent.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMUI/LMHackingWidget.h"
#include "LMUI/LMLoreData.h"
#include "LMUI/LMLoreWidget.h"
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
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMPuzzle/LMCubeRoomPuzzle.h"
#include "LMUI/LMCubePanel.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "Camera/CameraActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMCombat/LMCombatEffectData.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Camera/CameraShakeBase.h"

// 공통 카메라와 한 명의 플레이어가 사용할 QTE 판정을 준비한다.
ALMPlayerController::ALMPlayerController()
{
	PlayerCameraManagerClass = ALMPlayerCameraManager::StaticClass();
	QTEComponent = CreateDefaultSubobject<ULMQTEComponent>(TEXT("QTEComponent"));
}

// 공용 전투 효과를 먼저 비동기로 요청하고 기존 HUD를 연결한다.
void ALMPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && CombatEffectId.IsValid())
	{
		LoadedCombatEffectId = CombatEffectId;
		CombatEffectLoad = UAssetManager::Get().LoadPrimaryAsset(LoadedCombatEffectId, {FName(TEXT("Combat"))}, FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingCombatEffects));
	}
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

	// Started에만 연결하여 버튼을 누르는 동안 고정 상태가 반복해서 바뀌지 않게 한다.
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (ensureMsgf(EnhancedInput && LockOnAction, TEXT("Assign LockOnAction in the PlayerController Blueprint.")))
		EnhancedInput->BindAction(LockOnAction, ETriggerEvent::Started, this, &ThisClass::ToggleLockOn);
	
	// 휠은 캐릭터의 공통 축 입력에서 처리하여 대상 전환과 줌이 중복 실행되지 않게 한다.
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ThisClass::SelectPreviousInteractionTarget).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ThisClass::SelectNextInteractionTarget).bConsumeInput = true;
	
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
	// 이 컨트롤러가 요청한 공용 번들만 해제하며 풀 반환은 Niagara가 맡는다.
	if (CombatEffectLoad)
		CombatEffectLoad->CancelHandle();
	CombatEffectLoad.Reset();
	CombatEffects = nullptr;
	if (LoadedCombatEffectId.IsValid())
		UAssetManager::Get().UnloadPrimaryAsset(LoadedCombatEffectId);
	LoadedCombatEffectId = FPrimaryAssetId();
	QTEComponent->ResetInputState();
	
	// 액터의 Tick·타이머·벽 표시부터 정리하고 남은 화면도 제거한다.
	if (ALMCubeRoomPuzzle* ClosingPuzzle = ActiveCubePuzzle.Get())
		ClosingPuzzle->ClosePuzzle();
	ReleaseCubeView();
	
	ClearFocus();
	
	// Controller 종료 시에는 공격 소유 정보까지 모두 비운다.
	AttackFocusOwner.Reset();
	AttackFocusTarget.Reset();
	
	OnFocusFrame.Clear();

	ReleaseLoreWidget();
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

// 준비가 끝난 리소스만 사용하며 실패한 첫 타격을 나중에 재생하지 않는다.
void ALMPlayerController::FinishLoadingCombatEffects()
{
	if (bIsEndingPlay) return;
	CombatEffects = UAssetManager::Get().GetPrimaryAssetObject<ULMCombatEffectData>(LoadedCombatEffectId);
	if (!CombatEffects || !CombatEffects->NormalHit.IsValid() || !CombatEffects->CriticalHit.IsValid() || !CombatEffects->MonsterStrike.IsValid() || !CombatEffects->CriticalShake.IsValid())
		UE_LOG(LogTemp, Warning, TEXT("[LMCombatEffects] Check %s and its Combat bundle references."), *LoadedCombatEffectId.ToString());
}

// 체력 감소 결과 하나에서 효과 하나를 선택하고 플레이어의 성공에만 셰이크를 더한다.
void ALMPlayerController::PlayHitEffect(const FLMDamageResult& Result)
{
	if (bIsEndingPlay || !CombatEffects || Result.ActualDamage <= 0 || Result.bSceneDamage || !IsLocalPlayerController())
		return;
	ALMPlayerCameraManager* ViewManager = Cast<ALMPlayerCameraManager>(PlayerCameraManager);
	if (IsPaused() || bCinematicMode || (ViewManager && ViewManager->IsSequenceCameraActive()) || QTEComponent->HasActiveQTE())
		return;
	const bool bStrongHit = Result.bPlayerAttack && Result.bCritical;
	UNiagaraSystem* HitSystem = bStrongHit ? CombatEffects->CriticalHit.Get() : CombatEffects->NormalHit.Get();
	const float HitScale = bStrongHit ? CombatEffects->CriticalHitScale : CombatEffects->NormalHitScale;
	if (HitSystem && FMath::IsFinite(HitScale) && HitScale > 0.0f && !Result.HitLocation.ContainsNaN() && !Result.HitDirection.ContainsNaN())
	{
		// 풀에서 꺼낸 인스턴스는 이전 색·크기를 남기지 않고 활성화 전에 다시 설정한다.
		UNiagaraComponent* HitEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitSystem, Result.HitLocation, Result.HitDirection.Rotation(), FVector(HitScale), true, false, ENCPoolMethod::AutoRelease, true);
		if (HitEffect)
		{
			HitEffect->SetVariableLinearColor(TEXT("User.Color"), CombatEffects->HitColor);
			HitEffect->Activate(true);
		}
	}
	// 다수 적을 동시에 맞혀도 짧은 간격에 셰이크가 겹치지 않게 한다.
	const double Now = GetWorld()->GetTimeSeconds();
	if (bStrongHit && PlayerCameraManager && Now >= NextCriticalShakeTime && CombatEffects->CriticalShake.IsValid())
	{
		NextCriticalShakeTime = Now + 0.1;
		PlayerCameraManager->StartCameraShake(CombatEffects->CriticalShake.Get(), 1.0f);
	}
}

// 미들 공격 효과는 기존 손 소켓을 따라가며 종료 시 자동으로 풀에 돌아간다.
void ALMPlayerController::PlayMonsterStrike(USkeletalMeshComponent* StrikeMesh, FName StrikeSocket)
{
	if (bIsEndingPlay || !CombatEffects || !IsValid(StrikeMesh) || !StrikeMesh->DoesSocketExist(StrikeSocket) || !IsLocalPlayerController())
		return;
	ALMPlayerCameraManager* ViewManager = Cast<ALMPlayerCameraManager>(PlayerCameraManager);
	if (IsPaused() || bCinematicMode || (ViewManager && ViewManager->IsSequenceCameraActive()) || QTEComponent->HasActiveQTE())
		return;
	UNiagaraSystem* StrikeSystem = CombatEffects->MonsterStrike.Get();
	const float StrikeScale = CombatEffects->MonsterStrikeScale;
	if (!StrikeSystem || !FMath::IsFinite(StrikeScale) || StrikeScale <= 0.0f)
		return;
	UNiagaraComponent* StrikeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(StrikeSystem, StrikeMesh, StrikeSocket, FVector::ZeroVector, FRotator::ZeroRotator, FVector(StrikeScale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::AutoRelease, false, true);
	if (StrikeEffect)
	{
		StrikeEffect->SetVariableLinearColor(TEXT("User.Color"), CombatEffects->StrikeColor);
		StrikeEffect->Activate(true);
	}
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

// 다른 전용 화면과 겹치지 않게 해킹 창을 열고 입력을 전환한다.
bool ALMPlayerController::OpenHackingWidget(ULMHackingComponent* InSession)
{
	// 시작·종료 중인 QTE도 입력 소유권을 넘기지 않는다.
	if (QTEComponent->HasActiveQTE() || bHasQTEInputState)
		return false;

	// 큐브나 로어 화면이 입력을 소유하거나 전환 중이면 해킹 화면을 겹쳐 열지 않는다.
	if (bChangingCubeView || !ActiveCubePuzzle.IsExplicitlyNull() || ActiveCubePanel || bIsChangingLoreWidget || ActiveLoreWidget || !ActiveLoreInteraction.IsExplicitlyNull())
	{
		return false;	
	}
	
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

// 로어 창을 표시하고 소유한 일시정지와 UI 입력으로 전환한다.
bool ALMPlayerController::OpenLoreWidget(ULMInteractionComponent* LoreInteraction, ULMLoreData* LoreData)
{
	// 시작·종료 중인 QTE도 입력 소유권을 넘기지 않는다.
	if (QTEComponent->HasActiveQTE() || bHasQTEInputState)
		return false;

	// 다른 화면이 입력이나 카메라를 소유한 상태에서는 새 창을 열지 않는다.
	if (!IsLocalPlayerController() || IsActorBeingDestroyed() || bIsEndingPlay || IsPaused() || bIsChangingLoreWidget || ActiveLoreWidget || !ActiveLoreInteraction.IsExplicitlyNull() || bIsChangingHackingWidget || ActiveHackingWidget || !ActiveHackingSession.IsExplicitlyNull() || bChangingCubeView || ActiveCubePanel || !ActiveCubePuzzle.IsExplicitlyNull())
	{
		return false;
	}
	if (!IsValid(LoreInteraction) || LoreInteraction->IsBeingDestroyed() || !IsValid(LoreData) || !LoreData->WidgetClass)
	{
		return false;
	}

	// 위젯 생성이나 일시정지 콜백에서 재진입한 요청을 구분한다.
	TGuardValue<bool> ChangingGuard(bIsChangingLoreWidget, true);
	if (TryOpenLoreWidget(LoreInteraction, LoreData)) return true;

	// 열기 도중 취소되거나 실패하면 변경한 상태를 한 번만 복원한다.
	ClearLoreWidget();
	return false;
}

// 위젯을 등록하고 같은 요청이 유효한 동안에만 일시정지와 입력 전환을 진행한다.
bool ALMPlayerController::TryOpenLoreWidget(ULMInteractionComponent* LoreInteraction, ULMLoreData* LoreData)
{
	const TWeakObjectPtr<ULMInteractionComponent> RequestedInteraction(LoreInteraction);
	const TWeakObjectPtr<ULMLoreData> RequestedData(LoreData);

	// 생성 콜백에서도 카드가 제거되면 현재 요청을 취소할 수 있게 대상을 먼저 기록한다.
	ActiveLoreInteraction = RequestedInteraction;
	TStrongObjectPtr<ULMLoreWidget> NewWidget(CreateWidget<ULMLoreWidget>(this, LoreData->WidgetClass));

	// 생성 콜백에서 취소된 위젯도 같은 정리 경로에서 제거할 수 있게 보관한다.
	ActiveLoreWidget = NewWidget.Get();

	if (!IsValid(NewWidget.Get()) || !RequestedInteraction.IsValid() || !RequestedData.IsValid() || bIsEndingPlay || ActiveLoreInteraction != RequestedInteraction)
	{
		return false;
	}

	// 화면을 등록한 뒤에는 같은 요청과 위젯이 여전히 유효한지만 확인한다.
	const auto IsSameLoreRequest = [this, &RequestedInteraction, &RequestedData, &NewWidget]()
	{
		return !bIsEndingPlay && !IsActorBeingDestroyed()
			&& RequestedInteraction.IsValid() && RequestedData.IsValid() && ActiveLoreInteraction == RequestedInteraction
			&& IsValid(NewWidget.Get()) && ActiveLoreWidget == NewWidget.Get()
			&& NewWidget->GetOwningPlayer() == this && NewWidget->IsInViewport();
	};

	// 준비를 마친 위젯만 화면에 등록하고, 등록 중 취소되면 일시정지하지 않는다.
	if (!NewWidget->InitializeLore(RequestedData.Get()) || !NewWidget->AddToPlayerScreen(10) || !IsSameLoreRequest())
	{
		return false;
	}

	// 실패하거나 도중에 닫혀도 이 창이 바꾼 상태만 복원한다.
	bMouseCursorBeforeLore = bShowMouseCursor;
	bHasLoreInputState = true;
	bPausedByLore = SetPause(true);
	if (!bPausedByLore || !IsPaused() || !IsSameLoreRequest())
	{
		return false;
	}

	// 능력 취소 콜백에서 요청이 취소됐다면 UI 입력으로 전환하지 않는다.
	FlushPressedKeys();
	if (!IsSameLoreRequest()) return false;

	// 입력 전환을 마친 뒤에도 같은 창이 남아 있어야 열기에 성공한 것이다.
	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(NewWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	return IsSameLoreRequest();
}

// 현재 창에서 발생한 닫기 요청만 받아 세션을 정리한다.
void ALMPlayerController::CloseLoreWidget(ULMLoreWidget* SourceWidget)
{
	if (!IsValid(SourceWidget) || ActiveLoreWidget != SourceWidget)
	{
		return;
	}
	ReleaseLoreWidget();
}

// 대상 종료 시 동일한 대상이 연 창만 닫는다.
void ALMPlayerController::CloseLoreFor(ULMInteractionComponent* LoreInteraction)
{
	if (!LoreInteraction) return;
	const TWeakObjectPtr<ULMInteractionComponent> RequestedInteraction(LoreInteraction);
	if (!ActiveLoreInteraction.HasSameIndexAndSerialNumber(RequestedInteraction))
	{
		return;
	}
	ReleaseLoreWidget();
}

// 현재 로어 창을 닫되 열기 도중에는 실제 제거를 미룬다.
void ALMPlayerController::ReleaseLoreWidget()
{
	if (bIsChangingLoreWidget)
	{
		// 생성 중에는 같은 요청이 아니라는 표시만 남기고 열기 실패 경로에 정리를 맡긴다.
		ActiveLoreInteraction.Reset();
		return;
	}

	TGuardValue<bool> ChangingGuard(bIsChangingLoreWidget, true);
	ClearLoreWidget();
}

// 화면과 대상 참조를 해제하고 이 창이 바꾼 입력과 일시정지를 복원한다.
void ALMPlayerController::ClearLoreWidget()
{
	TStrongObjectPtr<ULMLoreWidget> ClosingWidget(ActiveLoreWidget.Get());

	// 위젯 제거 콜백이 다시 닫기를 요청해도 같은 창을 두 번 정리하지 않는다.
	ActiveLoreWidget = nullptr;
	ActiveLoreInteraction.Reset();
	if (IsValid(ClosingWidget.Get()))
	{
		ClosingWidget->ClearLore();
		ClosingWidget->RemoveFromParent();
	}
	RestoreLoreInput();
}

// 로어 창이 확보한 입력 상태와 일시정지를 한 번만 복원한다.
void ALMPlayerController::RestoreLoreInput()
{
	if (!bHasLoreInputState) return;

	const bool bReleasePause = bPausedByLore;
	const bool bRestoreCursor = bMouseCursorBeforeLore;
	// 복원 과정의 콜백이 다시 정리해도 같은 상태를 두 번 복원하지 않는다.
	bHasLoreInputState = false;
	bPausedByLore = false;
	FlushPressedKeys();
	bShowMouseCursor = bRestoreCursor;

	if (GetWorld())
	{
		SetInputMode(FInputModeGameOnly());
		if (bReleasePause && IsPaused())
			SetPause(false);
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
	// 완료 콜백에서 다음 장면으로 넘어가도 같은 입력을 일반 공격에 재사용하지 않는다.
	const bool bUsedByQTE = QTEComponent->HasActiveQTE();
	QTEComponent->SubmitButtonPress(InputTag);
	if (bUsedByQTE) return;

	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

/** 정상적인 입력 해제를 ASC에 전달한다. */
void ALMPlayerController::OnAbilityInputReleased(FGameplayTag InputTag)
{
	// 완료 콜백에서 다음 장면으로 넘어가도 같은 입력을 일반 공격에 재사용하지 않는다.
	const bool bUsedByQTE = QTEComponent->HasActiveQTE();
	QTEComponent->SubmitButtonRelease(InputTag);
	if (bUsedByQTE) return;

	if (ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

/** 중단된 입력을 정상 해제와 구분하여 ASC에 전달한다. */
void ALMPlayerController::OnAbilityInputCanceled(FGameplayTag InputTag)
{
	const bool bUsedByQTE = QTEComponent->HasActiveQTE();
	QTEComponent->SubmitButtonRelease(InputTag);
	if (bUsedByQTE) return;

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
	QTEComponent->ResetInputState();
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
	
	// 원래 캐릭터나 퍼즐을 잃으면 Ready 대기 중에도 즉시 정리한다.
	if (!bChangingCubeView && !ActiveCubePuzzle.IsExplicitlyNull())
	{
		ALMCubeRoomPuzzle* CurrentPuzzle = ActiveCubePuzzle.Get();
		ALMPlayerCharacter* Body = CubeBody.Get();

		if (!IsValid(CurrentPuzzle) || !CurrentPuzzle->CanStayOpen() || !IsValid(Body) || Body != GetPawn() || !Body->IsAliveForCombat() || !IsCubePuzzleActive(CurrentPuzzle))
		{
			if (IsValid(CurrentPuzzle))
				CurrentPuzzle->ClosePuzzle();

			ReleaseCubeView();
		}
	}

	// 구독 중인 Task의 UpdateFocus가 호출되고 Controller의 후보·선택이 교체된다.
	OnFocusFrame.Broadcast();

	// 검색 결과가 반영된 다음 현재 선택의 표식 위치를 갱신한다.
	// 선택이 그대로여도 카메라나 대상이 움직일 수 있으므로 매 프레임 호출한다.
	if (UserHUD)
		UserHUD->UpdateFocusMarker(GetFocusTarget(), IsLockOn());
}

// 게임이 멈추면 카메라 갱신도 멈출 수 있으므로 여기서 표식까지 비운다.
bool ALMPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
	// 실제 일시정지 처리는 기존 엔진 함수에 맡기고 성공 여부를 받는다.
	const bool bSucceeded = Super::SetPause(bPause, CanUnpauseDelegate);

	// 일시정지 요청이 성공했을 때만 즉시 목록·선택·표식을 비운다.
	if (bPause && bSucceeded)
	{
		QTEComponent->ResetInputState();
		ClearFocus();
	}

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
	
	// 카메라 블렌드가 끝나기 전에도 일반 플레이 포커스가 켜지지 않게 한다.
	if (bChangingCubeView || !ActiveCubePuzzle.IsExplicitlyNull() || ActiveCubePanel)
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

	// 수동 고정 대상을 잃으면 새 자동 선정 결과가 있어도 고정 상태는 다시 켜지지 않는다.
	if (bLockOn && !CanLockTarget(FocusTarget.Get()))
		StopLockOn();

	// 공격 보조 대상의 상실이 카메라의 자동 선정을 지우지 않게 별도로 검증한다.
	if (IsAttackFocusActive())
	{
		if (CanChangeAttackTarget())
		{
			// 특수 스킬의 복귀 대상·이번 타의 이동 방향은 유지하고 포커스 보조 대상만 갱신한다.
			AttackFocusTarget = CanLockTarget(FocusTarget.Get()) ? FocusTarget : TWeakObjectPtr<AActor>();
		}
		else if (!CanLockTarget(AttackFocusTarget.Get()))
			AttackFocusTarget.Reset();
	}
	else
	{
		AttackFocusOwner.Reset();
		AttackFocusTarget.Reset();
	}

	// 수동 선택과 이번 타의 적은 달라도 되지만 둘 다 현재 감지 후보 안에 있어야 한다.
	checkSlow(!FocusTarget.IsValid() || FocusCandidates.Contains(FocusTarget));
	checkSlow(!AttackFocusTarget.IsValid() || FocusCandidates.Contains(AttackFocusTarget));

	// Get() 결과만 비교하면 파괴된 이전 대상과 새 nullptr이 같아 보일 수 있다.
	// 약한 참조 자체를 비교해 그 경우에도 선택 해제 알림을 보낸다.
	if (PreviousTarget != FocusTarget)
		OnFocusChanged.Broadcast(FocusTarget.Get());
}

// 대상이 없는 검색 결과를 저장하고 화면에 남아 있는 표식을 숨긴다.
void ALMPlayerController::ClearFocus()
{
	StopLockOn();
	const TArray<TWeakObjectPtr<AActor>> EmptyCandidates;

	// 빈 목록과 nullptr도 같은 갱신 함수를 사용해 변경 알림 규칙을 유지한다.
	SetFocusResult(EmptyCandidates, nullptr);

	// 일시정지나 종료 중에는 다음 카메라 프레임이 없을 수 있어 즉시 숨긴다.
	if (UserHUD)
		UserHUD->UpdateFocusMarker(nullptr);
}

// 이번 공격의 보조 대상을 등록하되 사용자의 유효한 수동 선택은 덮어쓰지 않는다.
void ALMPlayerController::BeginAttackFocus(UGameplayAbility* _AttackFocusOwner, AActor* _AttackFocusTarget)
{
    if (!IsValid(_AttackFocusOwner) || !_AttackFocusOwner->IsActive() || _AttackFocusOwner->GetAvatarActorFromActorInfo() != GetPawn())
    {
        return;
    }

    // nullptr도 저장하여 대상 없이 시작한 평타가 도중에 다른 적에게 접근하지 않게 한다.
    AttackFocusOwner = _AttackFocusOwner;
    AttackFocusTarget = CanLockTarget(_AttackFocusTarget) ? _AttackFocusTarget : nullptr;

    // 접근할 수 없었던 공격이 유효한 락온이나 일반 포커스까지 지우지 않게 한다.
    if (!bLockOn && AttackFocusTarget.IsValid())
    {
        const TArray<TWeakObjectPtr<AActor>> SavedCandidates = FocusCandidates;
        SetFocusResult(SavedCandidates, AttackFocusTarget.Get());
    }
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

// 기존 특수 상태 태그로 재선정 정책을 구분하고 별도 태그나 컴포넌트는 만들지 않는다.
bool ALMPlayerController::CanChangeAttackTarget() const
{
    const ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent();
    return IsAttackFocusActive() && ASC && ASC->HasMatchingGameplayTag(LMGameplayTags::State_Attack_Special);
}

// 현재 수동 선택과 달라도 유효한 이번 타의 적은 기존 접근 보조에 제공한다.
AActor* ALMPlayerController::GetAttackFocusTarget() const
{
    if (!IsAttackFocusActive()) return nullptr;
    AActor* Target = AttackFocusTarget.Get();
    return CanLockTarget(Target) ? Target : nullptr;
}

// Actor 참조와 별도로 보관한 수동 고정 여부를 반환한다.
bool ALMPlayerController::IsLockOn() const
{
    return bLockOn;
}

// 대상 선정 중에도 재귀 검색 없이 수동 고정만 해제한다.
void ALMPlayerController::StopLockOn()
{
    bLockOn = false;
}

// 휠 클릭과 회전에 같은 생존·입력 차단 규칙을 적용한다.
bool ALMPlayerController::CanUseFocusInput() const
{
    const ALMBaseCharacter* PlayerCharacter = Cast<ALMBaseCharacter>(GetPawn());
    const ULMAbilitySystemComponent* ASC = GetLMAbilitySystemComponent();
    return CanUpdateFocus() && PlayerCharacter && PlayerCharacter->IsAliveForCombat() && ASC && !ASC->HasMatchingGameplayTag(LMGameplayTags::State_Input_Blocked);
}

// 중요 오브젝트와 전투 가능한 적을 구분하고 현재 감지 목록에 포함됐는지 확인한다.
bool ALMPlayerController::CanLockTarget(AActor* Target) const
{
    if (!CanUpdateFocus() || !IsValid(Target)) return false;
    const ALMBaseCharacter* PlayerCharacter = Cast<ALMBaseCharacter>(GetPawn());
    const ALMBaseCharacter* Enemy = Cast<ALMBaseCharacter>(Target);
    const ULMTargetComponent* Point = Target->FindComponentByClass<ULMTargetComponent>();
    if (!PlayerCharacter || !Enemy || !FocusCandidates.Contains(TWeakObjectPtr<AActor>(Target)))
    {
        return false;
    }

    return Point && Point->CanFocus() && !Target->IsHidden() && Enemy->CanBeAttackTargetFor(PlayerCharacter);
}

// 감지된 적을 화면 좌표로 비교하여 배열 순서와 관계없이 원하는 방향의 적을 고른다.
AActor* ALMPlayerController::FindLockTarget(int32 Direction) const
{
    int32 Width = 0;
    int32 Height = 0;
    GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return nullptr;

    FVector2D Origin(Width * 0.5, Height * 0.5);
    AActor* CurrentTarget = GetFocusTarget();
    if (Direction != 0 && (!CurrentTarget || !ProjectWorldLocationToScreen(ULMTargetQueryLibrary::GetFocusPoint(CurrentTarget), Origin, false)))
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    double BestScore = TNumericLimits<double>::Max();
    for (const TWeakObjectPtr<AActor>& Candidate : FocusCandidates)
    {
        AActor* Target = Candidate.Get();
        if (!CanLockTarget(Target)) continue;
        if (Direction != 0 && Target == CurrentTarget) continue;

        FVector2D ScreenPoint;
        if (!ProjectWorldLocationToScreen(ULMTargetQueryLibrary::GetFocusPoint(Target), ScreenPoint, false))
            continue;

        const FVector2D Offset = ScreenPoint - Origin;
        if (Direction != 0 && Offset.X * Direction <= 1.0)
            continue;

        const double Score = Offset.SizeSquared();
        if (Score < BestScore)
        {
            BestScore = Score;
            BestTarget = Target;
        }
    }

    return BestTarget;
}

// 입력 직전 후보를 갱신하고 사용자가 클릭했을 때만 수동 고정을 시작한다.
void ALMPlayerController::ToggleLockOn()
{
	// 사망·일시정지·UI 입력 차단 등으로 사용할 수 없으면 처리하지 않는다.
	if (!CanUseFocusInput()) return;

	// 이미 고정 중이면 고정을 풀고 기존 오토포커싱에 즉시 갱신을 요청한다.
	if (bLockOn)
	{
		StopLockOn();
		OnFocusFrame.Broadcast();
		return;
	}

	// 기존 오토포커싱 Task를 호출하여 클릭 순간의 최신 후보를 확인한다.
	OnFocusFrame.Broadcast();

	// 갱신 중 UI가 열리거나 플레이어가 사망했을 수 있으므로 다시 검사한다.
	if (!CanUseFocusInput()) return;

	// 현재 포커스가 적이면 사용하고, 그렇지 않으면 화면 중앙에 가까운 적을 찾는다.
	AActor* Target = GetFocusTarget();
	if (!CanLockTarget(Target))
		Target = FindLockTarget(0);
	if (!Target) return;

	// SetFocusResult가 내부 목록을 교체하므로 전달할 후보를 먼저 복사한다.
	const TArray<TWeakObjectPtr<AActor>> SavedCandidates = FocusCandidates;

	// 사용자의 클릭으로만 수동 고정 상태를 시작한다.
	bLockOn = true;
	SetFocusResult(SavedCandidates, Target);
}

// 고정 대상이 아직 유효할 때만 최신 후보에서 화면상 이전·다음 적으로 전환한다.
void ALMPlayerController::SwitchLockTarget(int32 Direction)
{
    if (!bLockOn || !CanUseFocusInput()) return;
    OnFocusFrame.Broadcast();
    if (!bLockOn || !CanUseFocusInput()) return;

    AActor* Target = FindLockTarget(Direction);
    if (!Target) return;

    const TArray<TWeakObjectPtr<AActor>> SavedCandidates = FocusCandidates;
    SetFocusResult(SavedCandidates, Target);
}

// 락온 전환·상호작용 선택·줌 중 하나만 한 번의 휠 입력을 사용하게 한다.
bool ALMPlayerController::HandleMouseWheel(float WheelInput)
{
    if (!FMath::IsFinite(WheelInput)) return true;
    if (FMath::IsNearlyZero(WheelInput)) return true;
    if (!CanUseFocusInput()) return true;

    const int32 Direction = WheelInput > 0.0f ? -1 : 1;
    if (bLockOn)
    {
        SwitchLockTarget(Direction);
        return true;
    }

    // 상호작용 목록이 비었으면 캐릭터의 기존 줌으로 입력을 넘긴다.
    return InteractionHUD && InteractionHUD->MoveSelection(Direction);
}

// 현재 요청이 소유한 화면이 뷰포트에 등록됐는지 확인한다.
bool ALMPlayerController::IsCubePuzzleActive(const ALMCubeRoomPuzzle* Puzzle) const
{
	return IsValid(Puzzle) && ActiveCubePuzzle.Get() == Puzzle && IsValid(ActiveCubePanel) && ActiveCubePanel->IsInViewport();
}

// 해킹과 중복되지 않게 화면을 열고 프로젝트 입력을 정리한다.
bool ALMPlayerController::OpenCubePuzzle(ALMCubeRoomPuzzle* Puzzle, ACameraActor* Camera)
{
	// 시작·종료 중인 QTE도 입력 소유권을 넘기지 않는다.
	if (QTEComponent->HasActiveQTE() || bHasQTEInputState)
		return false;

    if (!IsLocalPlayerController() || IsActorBeingDestroyed() || bIsEndingPlay || IsPaused() || bChangingCubeView || bIsChangingHackingWidget || bIsChangingLoreWidget || ActiveLoreWidget || !ActiveLoreInteraction.IsExplicitlyNull() || !ActiveCubePuzzle.IsExplicitlyNull() || ActiveCubePanel || !ActiveHackingSession.IsExplicitlyNull() || ActiveHackingWidget)
    {
        return false;
    }

    ALMPlayerCharacter* Body = Cast<ALMPlayerCharacter>(GetPawn());
    ALMPlayerCameraManager* ViewManager = Cast<ALMPlayerCameraManager>(PlayerCameraManager);

    if (!IsValid(Puzzle) || !Puzzle->CanStayOpen() || !IsValid(Camera) || !IsValid(ViewManager) || !CubePanelClass || !IsValid(Body) || !Body->IsAliveForCombat() || Body->GetCharacterMovement()->MovementMode != MOVE_Walking)
    {
        return false;
    }

    const TWeakObjectPtr<ALMCubeRoomPuzzle> OpeningPuzzle(Puzzle);
    const TWeakObjectPtr<ALMPlayerCharacter> OpeningBody(Body);

    TStrongObjectPtr<ULMCubePanel> NewPanel(nullptr);
    bool bOpened = false;

    {
        TGuardValue<bool> ChangeGuard(bChangingCubeView, true);

        ActiveCubePuzzle = OpeningPuzzle;
        CubeBody = OpeningBody;

        // BP 생성·표시와 GAS 취소 콜백 이후 같은 요청인지 다시 검사한다.
        const auto IsRequestCurrent = [this, &OpeningPuzzle, &OpeningBody]()
        {
            return !bIsEndingPlay && !IsActorBeingDestroyed() && OpeningPuzzle.IsValid() && OpeningPuzzle->CanStayOpen() && ActiveCubePuzzle == OpeningPuzzle && OpeningBody.IsValid() && GetPawn() == OpeningBody.Get() && OpeningBody->IsAliveForCombat();
        };

        do
        {
            NewPanel.Reset(CreateWidget<ULMCubePanel>(this, CubePanelClass));

            if (!IsValid(NewPanel.Get()) || !IsRequestCurrent()) break;

            ActiveCubePanel = NewPanel.Get();
            NewPanel->OpenFor(Puzzle, InteractionHUD);

            if (!NewPanel->AddToPlayerScreen(100) || !IsRequestCurrent() || !NewPanel->HasCubeView()) break;

            // 기존 카메라 연출의 종료 복귀를 먼저 끝내고 복귀 시점을 저장한다.
            ViewManager->StopCameraEffect();

            if (!IsRequestCurrent()) break;

            CubeOldView = GetViewTarget();
            bCubeOldCursor = bShowMouseCursor;
            bHasCubeInput = true;

            SetIgnoreMoveInput(true);
            SetIgnoreLookInput(true);

            // 프로젝트의 능력 입력 취소도 함께 수행된다.
            FlushPressedKeys();

            if (!IsRequestCurrent()) break;

            Body->CancelMoveInput();
            Body->CancelPendingJump();

            if (!IsRequestCurrent() || !Body->UsesFreeMovement() || Body->HasAnyRootMotion()) break;
            if (Body->GetCharacterMovement()->MovementMode != MOVE_Walking) break;

            Body->GetCharacterMovement()->StopMovementImmediately();

            bCubeMovementHeld = true;
            Body->GetCharacterMovement()->DisableMovement();

            if (!IsRequestCurrent()) break;

            FInputModeUIOnly Mode;
            Mode.SetWidgetToFocus(NewPanel->TakeWidget());
            SetInputMode(Mode);

            if (!IsRequestCurrent()) break;

            bShowMouseCursor = false;
            NewPanel->SetUserFocus(this);

            if (!IsRequestCurrent()) break;

            SetViewTargetWithBlend(Camera, CubeBlendTime, VTBlend_Cubic);

            bOpened = IsRequestCurrent() && IsCubePuzzleActive(Puzzle);
        } while (false);
    }

    if (!bOpened)
    {
        // 생성 중 취소는 콜백을 빠져나온 여기에서 한 번만 정리한다.
        ReleaseCubeView();

        if (IsValid(NewPanel.Get()))
        {
            NewPanel->ClearPuzzle();
            NewPanel->RemoveFromParent();
        }
    }

    return bOpened;
}

// 다른 퍼즐의 늦은 종료 요청이 현재 화면을 닫지 않게 한다.
void ALMPlayerController::CloseCubePuzzle(ALMCubeRoomPuzzle* Puzzle)
{
    if (!Puzzle) return;

    const TWeakObjectPtr<ALMCubeRoomPuzzle> ClosingPuzzle(Puzzle);

    if (!ActiveCubePuzzle.HasSameIndexAndSerialNumber(ClosingPuzzle)) return;

    ReleaseCubeView();
}

// 활성 화면에만 상태를 보내고 UMG 배치나 문구에는 관여하지 않는다.
void ALMPlayerController::RefreshCubePanel(ALMCubeRoomPuzzle* Puzzle)
{
    if (IsCubePuzzleActive(Puzzle))
        ActiveCubePanel->RefreshView();
}

// 중복 정리와 사망 후 이동 재활성화를 막으면서 원래 플레이로 돌아간다.
void ALMPlayerController::ReleaseCubeView()
{
    ActiveCubePuzzle.Reset();

    if (bChangingCubeView) return;

    TGuardValue<bool> ChangeGuard(bChangingCubeView, true);
    TStrongObjectPtr<ULMCubePanel> ClosingPanel(ActiveCubePanel.Get());

    const TWeakObjectPtr<ALMPlayerCharacter> ClosingBody = CubeBody;
    const TWeakObjectPtr<AActor> ReturnView = CubeOldView;

    const bool bReleaseInput = bHasCubeInput;
    const bool bReleaseMovement = bCubeMovementHeld;
    const bool bReturnCursor = bCubeOldCursor;

    // BP Destruct나 입력 취소가 재진입하기 전에 소유 정보를 비운다.
    ActiveCubePanel = nullptr;
    CubeBody.Reset();
    CubeOldView.Reset();

    bHasCubeInput = false;
    bCubeMovementHeld = false;

    if (IsValid(ClosingPanel.Get()))
    {
        ClosingPanel->ClearPuzzle();
        ClosingPanel->RemoveFromParent();
    }

    ALMPlayerCharacter* Body = ClosingBody.Get();

    if (bReleaseMovement && IsValid(Body) && Body->IsAliveForCombat() && Body->UsesFreeMovement() && Body->GetCharacterMovement()->MovementMode == MOVE_None)
    {
        Body->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    if (!bReleaseInput) return;

    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);

    FlushPressedKeys();
    bShowMouseCursor = bReturnCursor;
    SetInputMode(FInputModeGameOnly());

    if (bIsEndingPlay || IsActorBeingDestroyed()) return;

    AActor* ViewTarget = ReturnView.Get();

    if (!IsValid(ViewTarget) || ViewTarget->IsActorBeingDestroyed() || (ViewTarget == Body && Body != GetPawn()))
        ViewTarget = GetPawn();

    if (IsValid(ViewTarget))
        SetViewTargetWithBlend(ViewTarget, CubeBlendTime, VTBlend_Cubic);
}

// 다른 장면에서도 컨트롤러에 이미 있는 판정을 사용한다.
ULMQTEComponent* ALMPlayerController::GetQTEComponent() const
{
	return QTEComponent;
}

// 하드코딩한 Q 대신 현재 매핑의 실제 키 표시명을 얻는다.
FText ALMPlayerController::GetQTEInputText(FGameplayTag InputTag) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* InputSystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputSystem || !InputConfig) return FText::GetEmpty();
	for (const FLMAbilityInputBinding& Binding : InputConfig->AbilityInputs)
	{
		if (Binding.InputTag != InputTag || !Binding.InputAction) continue;
		const TArray<FKey> Keys = InputSystem->QueryKeysMappedToAction(Binding.InputAction);
		if (!Keys.IsEmpty()) return Keys[0].GetDisplayName();
	}
	return FText::GetEmpty();
}

// QTE는 시퀀스 안에서도 동작하지만 다른 조작 화면과는 함께 열지 않는다.
bool ALMPlayerController::CanStartQTE() const
{
	const ALMPlayerCharacter* PlayerBody = Cast<ALMPlayerCharacter>(GetPawn());
	return IsLocalPlayerController() && !IsActorBeingDestroyed() && !bIsEndingPlay && !IsPaused() && IsValid(PlayerBody) && PlayerBody->IsAliveForCombat()
		&& !bIsChangingHackingWidget && !bHasHackingInputState && !ActiveHackingWidget && ActiveHackingSession.IsExplicitlyNull()
		&& !bIsChangingLoreWidget && !bHasLoreInputState && !ActiveLoreWidget && ActiveLoreInteraction.IsExplicitlyNull()
		&& !bChangingCubeView && !bHasCubeInput && !ActiveCubePanel && ActiveCubePuzzle.IsExplicitlyNull();
}

// 월드를 멈추지 않고 QTE가 필요한 입력만 확보한다.
bool ALMPlayerController::BeginQTEInput(bool bDrag, UUserWidget* Widget)
{
	if (!CanStartQTE() || bHasQTEInputState || !IsValid(Widget) || Widget->GetOwningPlayer() != this)
		return false;

	bHasQTEInputState = true;
	bMouseCursorBeforeQTE = bShowMouseCursor;
	bQTEChangedInputMode = bDrag;
	// FlushPressedKeys는 새 QTE도 취소하므로 기존 능력 큐만 비운다.
	CancelAbilityInput();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	if (bDrag)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		// 포인터만 받아 기존 뷰포트의 키 초기화가 새 QTE를 취소하지 않게 한다.
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	return true;
}

// 연출 BP가 따로 확보한 잠금은 남기고 QTE 몫만 해제한다.
void ALMPlayerController::EndQTEInput()
{
	if (!bHasQTEInputState) return;
	const bool bRestoreGameInput = bQTEChangedInputMode;
	bHasQTEInputState = false;
	bQTEChangedInputMode = false;
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = bMouseCursorBeforeQTE;
	if (bRestoreGameInput)
		SetInputMode(FInputModeGameOnly());
	CancelAbilityInput();
}
