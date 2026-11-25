#include "LMCamera/LMPlayerCameraManager.h"
#include "LMCamera/LMPostProcessCameraModifier.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "TimerManager.h"
#include "LMCamera/LMTargetQueryLibrary.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMComponent/LMTargetComponent.h"
#include "LMPlayer/LMPlayerController.h"

// 기존 후처리 카메라 보정을 등록한다.
ALMPlayerCameraManager::ALMPlayerCameraManager()
{
	DefaultModifiers.AddUnique(ULMPostProcessCameraModifier::StaticClass());
}

ULMPostProcessCameraModifier* ALMPlayerCameraManager::GetPostProcessModifier() const
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (ULMPostProcessCameraModifier* PostProcess = Cast<ULMPostProcessCameraModifier>(Modifier))
		{
			return PostProcess;
		}
	}
	return nullptr;
}

// 기존 호출부는 바인딩을 지정하지 않은 같은 생성 경로를 사용한다.
bool ALMPlayerCameraManager::PlayCameraEffect(ULevelSequence* Sequence, TSubclassOf<UCameraShakeBase> ShakeClass, float ShakeScale)
{
	return StartCameraEffect(Sequence, ShakeClass, ShakeScale, TMap<FName, AActor*>());
}

// 실제 주인공·적·방 액터를 시퀀스 역할에 배치한다.
bool ALMPlayerCameraManager::PlayBoundSequence(ULevelSequence* Sequence, const TMap<FName, AActor*>& Bindings)
{
	if (!IsValid(Sequence)) return false;
	return StartCameraEffect(Sequence, nullptr, 1.0f, Bindings);
}

// 호출자가 다른 장면의 재생기를 실수로 재개하지 않게 참조를 제공한다.
ULevelSequencePlayer* ALMPlayerCameraManager::GetActiveSequencePlayer() const
{
	return ActiveSequencePlayer;
}

// 이전 장면 교체·바인딩·재생 순서를 하나의 경로로 유지한다.
bool ALMPlayerCameraManager::StartCameraEffect(ULevelSequence* Sequence, TSubclassOf<UCameraShakeBase> ShakeClass, float ShakeScale, const TMap<FName, AActor*>& Bindings)
{
	if (bStartingSequence || bFinishingSequence || bEndingCamera)
		return false;
	TGuardValue<bool> StartGuard(bStartingSequence, true);
	for (const TPair<FName, AActor*>& Binding : Bindings)
	{
		if (Binding.Key.IsNone() || !IsValid(Binding.Value) || Binding.Value->GetWorld() != GetWorld())
			return false;
	}

	if (!IsValid(PCOwner) || !PCOwner->IsLocalController() || (!Sequence && !ShakeClass))
	{
		return false;
	}
	if ((Sequence && (!IsValid(Sequence) || !Sequence->GetMovieScene())) ||
		(ShakeClass && (ShakeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
			!FMath::IsFinite(ShakeScale) || ShakeScale <= 0.0f)))
	{
		return false;
	}

	if (ShakeClass && ShakeClass.GetDefaultObject()->bSingleInstance)
	{
		if (UCameraModifier_CameraShake* Modifier = Cast<UCameraModifier_CameraShake>(
			FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass())))
		{
			TArray<FActiveCameraShakeInfo> Shakes;
			Modifier->GetActiveCameraShakes(Shakes);
			for (const FActiveCameraShakeInfo& Info : Shakes)
			{
				// 엔진이 재사용하는 단일 인스턴스를 다른 효과로부터 가져오지 않는다.
				if (Info.ShakeInstance && Info.ShakeInstance != ActiveCameraShake && Info.ShakeInstance->GetClass() == ShakeClass)
				{
					return false;
				}
			}
		}
	}

	StopCameraEffect();

	if (Sequence)
	{
		FMovieSceneSequencePlaybackSettings Settings;
		// 자연 종료의 내부 Stop이 취소 알림을 먼저 보내지 않도록 완료 후 직접 정리한다.
		Settings.bPauseAtEnd = true;
		Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;

		ALevelSequenceActor* SequenceActor = nullptr;
		ActiveSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, SequenceActor);
		ActiveSequenceActor = SequenceActor;
		if (!IsValid(ActiveSequencePlayer))
		{
			FinishSequence(false, false);
			return false;
		}

		// 누락된 역할은 무대에 잘못된 액터를 남기는 대신 시작 실패로 돌려준다.
		for (const TPair<FName, AActor*>& Binding : Bindings)
		{
			if (SequenceActor->FindNamedBindings(Binding.Key).IsEmpty())
			{
				FinishSequence(false, false);
				return false;
			}
			SequenceActor->SetBindingByTag(Binding.Key, TArray<AActor*>{Binding.Value}, false);
		}

		// 시퀀스의 Camera Cut도 이 매니저의 로컬 플레이어에게 적용한다.
		SequenceActor->SetOwner(PCOwner);
		ActiveSequencePlayer->OnFinished.AddDynamic(this, &ThisClass::HandleSequenceFinished);
		ActiveSequencePlayer->OnStop.AddDynamic(this, &ThisClass::HandleSequenceStopped);
	}

	if (ShakeClass)
	{
		ActiveCameraShake = StartCameraShake(ShakeClass, ShakeScale);
	}

	const bool bStarted = IsValid(ActiveSequencePlayer) || IsValid(ActiveCameraShake);
	if (ActiveSequencePlayer)
	{
		ActiveSequencePlayer->Play();
	}
	return bStarted;
}

void ALMPlayerCameraManager::StopCameraEffect()
{
	RestoreGameplayView();
}

bool ALMPlayerCameraManager::IsSequenceCameraActive() const
{
	return IsValid(ActiveSequencePlayer);
}

// 명시적인 복귀는 시퀀스 성공으로 처리하지 않는다.
void ALMPlayerCameraManager::RestoreGameplayView()
{
	FinishSequence(false);
}

// 끝 프레임 도달을 취소와 구분한다.
void ALMPlayerCameraManager::HandleSequenceFinished()
{
	FinishSequence(true);
}

// 외부 중단도 같은 정리 경로를 거친다.
void ALMPlayerCameraManager::HandleSequenceStopped()
{
	FinishSequence(false);
}

// 참조와 입력 시점을 복원한 뒤 다음 장면의 시작을 허용한다.
void ALMPlayerCameraManager::FinishSequence(bool bFinished, bool bNotify)
{
	if (bFinishingSequence) return;
	bFinishingSequence = true;
	ULevelSequencePlayer* SequencePlayer = ActiveSequencePlayer;
	ALevelSequenceActor* SequenceActor = ActiveSequenceActor;
	ULevelSequence* ClosedSequence = IsValid(SequenceActor) ? SequenceActor->GetSequence() : nullptr;
	ActiveSequencePlayer = nullptr;
	ActiveSequenceActor = nullptr;

	if (IsValid(SequencePlayer))
	{
		// Stop의 종료 알림이 정리 함수를 다시 호출하지 않도록 먼저 해제한다.
		SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::HandleSequenceFinished);
		SequencePlayer->OnStop.RemoveDynamic(this, &ThisClass::HandleSequenceStopped);
		if (SequencePlayer->IsPlaying() || SequencePlayer->IsPaused())
		{
			SequencePlayer->Stop();
		}
	}
	if (IsValid(SequenceActor))
	{
		// 종료 콜백이 반환되기 전에 재생기를 파괴하면 Sequencer의 후속 정리가 깨진다.
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
			SequenceActor, [SequenceActor]() { SequenceActor->Destroy(); }));
	}
	if (IsValid(ActiveCameraShake))
	{
		StopCameraShake(ActiveCameraShake, true);
	}
	ActiveCameraShake = nullptr;

	if (IsValid(PCOwner))
	{
		// 연출 도중 캐릭터가 교체될 수 있으므로 시작 당시의 Pawn을 저장하지 않는다.
		APawn* CurrentPawn = PCOwner->GetPawn();
		PCOwner->SetViewTarget(IsValid(CurrentPawn) ? static_cast<AActor*>(CurrentPawn) : PCOwner.Get());
	}
	bFinishingSequence = false;
	if (bNotify && IsValid(ClosedSequence))
		OnSequenceEnded.Broadcast(ClosedSequence, bFinished);
}

void ALMPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);
	if (ActiveCameraShake && ActiveCameraShake->IsFinished())
	{
		ActiveCameraShake = nullptr;
		if (!ActiveSequencePlayer)
		{
			RestoreGameplayView();
		}
	}
}

// 종료 알림에서 새 연출이 만들어지지 않게 먼저 막는다.
void ALMPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingCamera = true;
	StopCameraEffect();
	Super::EndPlay(EndPlayReason);
	
}

// 유효한 수동 고정은 계속 추적하고 해제 상태는 기존 자유 시점 규칙을 사용한다.
void ALMPlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
    const ALMPlayerController* Controller = Cast<ALMPlayerController>(PCOwner);
    const bool bTrackLock = Controller && Controller->IsLockOn() && Controller->CanLockTarget(Controller->GetFocusTarget());
    if (bTrackLock)
    {
        // 입력 무시 플래그는 추적도 멈추므로 이번 자유 시점 회전량만 소비한다.
        OutDeltaRot = FRotator::ZeroRotator;
        LookWait = 0.0f;
        if (DeltaTime > 0.0f)
            ApplyFocusRotation(DeltaTime, OutViewRotation);
    }
    else
    {
        const bool bHasLookInput = !OutDeltaRot.IsNearlyZero();
        LookWait = bHasLookInput ? LookDelay : FMath::Max(0.0f, LookWait - DeltaTime);
        if (DeltaTime > 0.0f && !bHasLookInput && LookWait <= 0.0f)
            ApplyFocusRotation(DeltaTime, OutViewRotation);
    }

    // 기존 수동 입력과 CameraModifier, 엔진의 시점 각도 제한을 적용한다.
    Super::ProcessViewRotation(DeltaTime, OutViewRotation, OutDeltaRot);
}

// 현재 표식 대상을 향해 회전하며, 평타 대상이 확정된 동안에는 더 빠르게 따라간다.
void ALMPlayerCameraManager::ApplyFocusRotation(float DeltaTime, FRotator& ViewRotation)
{
    ALMPlayerController* Controller = Cast<ALMPlayerController>(PCOwner);
    ALMPlayerCharacter* Player = Controller ? Cast<ALMPlayerCharacter>(Controller->GetPawn()) : nullptr;

    // 일반 플레이 화면이 아니거나 시점 조작이 금지된 상태에서는 자동 회전하지 않는다.
    if (!Controller || !Player || !Controller->CanUpdateFocus() || Controller->IsLookInputIgnored() || !Player->IsLookAllowed() || !Player->IsAliveForCombat())
    {
        return;
    }

    // 이번 타의 대상이 사라져도 새 자동 포커스와 표식을 그대로 따라간다.
    AActor* Target = Controller->GetFocusTarget();
    const bool bLockOn = Controller->IsLockOn();
    const bool bAttackFocus = Target && Controller->GetAttackFocusTarget() == Target;

    if (!IsValid(Target)) return;

    const ULMTargetComponent* Point = Target->FindComponentByClass<ULMTargetComponent>();

    if (!Point || !Point->CanFocus()) return;

    const ALMBaseCharacter* TargetCharacter = Cast<ALMBaseCharacter>(Target);

    // 다음 후보 검색 전에 몬스터가 죽었더라도 카메라 추적은 중단한다.
    if (TargetCharacter && !TargetCharacter->IsAliveForCombat())
    {
        return;
    }

    // 고정 중에는 플레이어 위치도 일부 반영하여 적만 화면 중앙에 몰리지 않게 한다.
    FVector AimPoint = ULMTargetQueryLibrary::GetFocusPoint(Target);
    if (bLockOn)
        AimPoint = FMath::Lerp(Player->GetPawnViewLocation(), AimPoint, FMath::Clamp(LockTargetWeight, 0.0f, 1.0f));
    const FVector AimDirection = AimPoint - GetCameraLocation();

    if (AimDirection.ContainsNaN() || AimDirection.IsNearlyZero())
    {
        return;
    }

    FRotator WantedRotation = AimDirection.Rotation();

    // 자동 회전 때문에 카메라가 지나치게 위나 아래로 기울지 않도록 한다.
    WantedRotation.Pitch = FMath::Clamp<double>(WantedRotation.Pitch, FocusMinPitch, FocusMaxPitch);

    // 각도가 180도 경계를 넘어도 가까운 방향으로 회전하도록 차이를 정규화한다.
    FRotator Turn = (WantedRotation - ViewRotation).GetNormalized();

    // 거의 정면에 있는 대상은 미세하게 따라가지 않아 화면 떨림을 줄인다.
    if (FMath::Abs(Turn.Yaw) <= FocusDeadAngle)
        Turn.Yaw = 0.0;

    if (FMath::Abs(Turn.Pitch) <= FocusDeadAngle)
        Turn.Pitch = 0.0;

    const float Speed = bLockOn ? LockFocusSpeed : (bAttackFocus ? AttackFocusSpeed : NormalFocusSpeed);

    // 프레임 시간에 맞춰 남은 각도를 줄이고, 한 프레임에 회전할 최대 각도를 계산한다.
    const double Alpha = 1.0 - FMath::Exp(-static_cast<double>(Speed) * DeltaTime);
    const double MaxStep = static_cast<double>(MaxFocusTurnRate) * DeltaTime;

    ViewRotation.Yaw += FMath::Clamp(Turn.Yaw * Alpha, -MaxStep, MaxStep);
    ViewRotation.Pitch += FMath::Clamp(Turn.Pitch * Alpha, -MaxStep, MaxStep);
}
