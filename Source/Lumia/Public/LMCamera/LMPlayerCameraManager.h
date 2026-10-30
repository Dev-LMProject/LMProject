#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "LMPlayerCameraManager.generated.h"

class ALevelSequenceActor;
class UCameraShakeBase;
class ULevelSequence;
class ULevelSequencePlayer;
class ULMPostProcessCameraModifier;

/** 로컬 플레이어의 카메라 연출과 기본 시점 복귀를 관리한다. */
UCLASS()
class LUMIA_API ALMPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ALMPlayerCameraManager();

	ULMPostProcessCameraModifier* GetPostProcessModifier() const;

	/** 기존 연출을 교체한다. Sequence와 ShakeClass 중 사용할 애셋만 전달한다. */
	UFUNCTION(BlueprintCallable, Category = "LM|Camera")
	bool PlayCameraEffect(ULevelSequence* Sequence, TSubclassOf<UCameraShakeBase> ShakeClass, float ShakeScale = 1.0f);

	/** 이 매니저가 재생한 연출을 중단하고 기본 시점으로 복귀한다. */
	UFUNCTION(BlueprintCallable, Category = "LM|Camera")
	void StopCameraEffect();

	/** 연출을 정리하고 현재 조종 중인 캐릭터의 카메라로 복귀한다. */
	UFUNCTION(BlueprintCallable, Category = "LM|Camera")
	void RestoreGameplayView();

	/** 이 매니저의 시퀀스 연출이 아직 정리되지 않았는지 반환한다. */
	bool IsSequenceCameraActive() const;

	/** 단독 재생한 카메라 흔들림의 자연 종료를 처리한다. */
	virtual void UpdateCamera(float DeltaTime) override;

protected:
	/** 월드 종료 시 런타임 재생기와 종료 알림을 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 현재 시퀀스 연출을 위해 생성한 임시 액터. 종료 후 제거한다. */
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> ActiveSequenceActor;

	/** 현재 시퀀스의 재생·중단을 제어하고 종료 알림을 받는 재생기. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> ActiveSequencePlayer;

	/** 이 매니저가 시작한 카메라 흔들림. 연출 중단 시 해당 인스턴스만 정리한다. */
	UPROPERTY(Transient)
	TObjectPtr<UCameraShakeBase> ActiveCameraShake;
};
