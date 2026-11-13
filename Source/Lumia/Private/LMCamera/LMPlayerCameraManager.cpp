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

bool ALMPlayerCameraManager::IsSequenceCameraActive() const
{
	return IsValid(ActiveSequencePlayer);
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

// 직접 카메라를 조작하는 동안 자동 회전을 멈추고, 입력이 끝난 뒤 다시 시작한다.
void ALMPlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
    // 부모 함수가 입력 회전량을 소비하기 전에 사용자의 시점 입력 여부를 확인한다.
    const bool bHasLookInput = !OutDeltaRot.IsNearlyZero();

    if (bHasLookInput)
        LookWait = LookDelay;
    else
        LookWait = FMath::Max(0.0f, LookWait - DeltaTime);

    // 수동 입력이 없고 대기 시간이 끝났을 때만 포커스 대상을 향해 회전한다.
    if (DeltaTime > 0.0f && !bHasLookInput && LookWait <= 0.0f)
        ApplyFocusRotation(DeltaTime, OutViewRotation);

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

    // 공격 중에는 이번 타의 대상만 사용하고, 평소에는 일반 포커스 대상을 사용한다.
    const bool bAttackFocus = Controller->IsAttackFocusActive();
    AActor* Target = bAttackFocus ? Controller->GetAttackFocusTarget() : Controller->GetFocusTarget();

    if (!IsValid(Target)) return;

    const ULMTargetComponent* Point = Target->FindComponentByClass<ULMTargetComponent>();

    if (!Point || !Point->CanFocus()) return;

    const ALMBaseCharacter* TargetCharacter = Cast<ALMBaseCharacter>(Target);

    // 다음 후보 검색 전에 몬스터가 죽었더라도 카메라 추적은 중단한다.
    if (TargetCharacter && !TargetCharacter->IsAliveForCombat())
    {
        return;
    }

    // HUD 표식과 같은 월드 표적점을 카메라가 바라보도록 방향을 구한다.
    const FVector AimDirection = ULMTargetQueryLibrary::GetFocusPoint(Target) - GetCameraLocation();

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

    const float Speed = bAttackFocus ? AttackFocusSpeed : NormalFocusSpeed;

    // 프레임 시간에 맞춰 남은 각도를 줄이고, 한 프레임에 회전할 최대 각도를 계산한다.
    const double Alpha = 1.0 - FMath::Exp(-static_cast<double>(Speed) * DeltaTime);
    const double MaxStep = static_cast<double>(MaxFocusTurnRate) * DeltaTime;

    ViewRotation.Yaw += FMath::Clamp(Turn.Yaw * Alpha, -MaxStep, MaxStep);
    ViewRotation.Pitch += FMath::Clamp(Turn.Pitch * Alpha, -MaxStep, MaxStep);
}