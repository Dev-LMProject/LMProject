#include "LMCamera/LMPlayerCameraManager.h"

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

bool ALMPlayerCameraManager::PlayCameraEffect(
	ULevelSequence* Sequence, TSubclassOf<UCameraShakeBase> ShakeClass, float ShakeScale)
{
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
		Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;

		ALevelSequenceActor* SequenceActor = nullptr;
		ActiveSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, SequenceActor);
		ActiveSequenceActor = SequenceActor;
		if (!IsValid(ActiveSequencePlayer))
		{
			RestoreGameplayView();
			return false;
		}

		// 시퀀스의 Camera Cut도 이 매니저의 로컬 플레이어에게 적용한다.
		SequenceActor->SetOwner(PCOwner);
		ActiveSequencePlayer->OnFinished.AddDynamic(this, &ThisClass::RestoreGameplayView);
		ActiveSequencePlayer->OnStop.AddDynamic(this, &ThisClass::RestoreGameplayView);
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

void ALMPlayerCameraManager::RestoreGameplayView()
{
	ULevelSequencePlayer* SequencePlayer = ActiveSequencePlayer;
	ALevelSequenceActor* SequenceActor = ActiveSequenceActor;
	ActiveSequencePlayer = nullptr;
	ActiveSequenceActor = nullptr;

	if (IsValid(SequencePlayer))
	{
		// Stop의 종료 알림이 정리 함수를 다시 호출하지 않도록 먼저 해제한다.
		SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::RestoreGameplayView);
		SequencePlayer->OnStop.RemoveDynamic(this, &ThisClass::RestoreGameplayView);
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

void ALMPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCameraEffect();
	Super::EndPlay(EndPlayReason);
}
