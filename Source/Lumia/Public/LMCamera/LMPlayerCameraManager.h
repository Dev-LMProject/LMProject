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
	
	// Controller가 시점 회전을 확정하기 전에 수동 입력과 자동 포커스 회전을 함께 처리한다.
	virtual void ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot) override;

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
	
	// 현재 공통 대상을 향해 시점 회전을 조금씩 보정한다.
	void ApplyFocusRotation(float DeltaTime, FRotator& ViewRotation);

	// 평소 포커스 대상 쪽으로 회전하는 보간 속도다. 클수록 빠르게 따라간다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "0.0"))
	float NormalFocusSpeed = 1.5f;

	// 지상 평타의 대상이 확정된 동안 사용하는 보간 속도다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "0.0"))
	float AttackFocusSpeed = 6.0f;

	// 큰 각도 차이에서도 한 축이 초당 이 각도보다 빠르게 돌아가지 않게 제한한다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "0.0"))
	float MaxFocusTurnRate = 180.0f;

	// 직접 카메라를 움직인 뒤 자동 보정을 다시 시작하기까지 기다리는 시간이다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "0.0", Units = "s"))
	float LookDelay = 0.35f;

	// 거의 정면에 있는 대상을 계속 미세하게 따라가며 화면이 흔들리는 것을 줄인다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "0.0", Units = "deg"))
	float FocusDeadAngle = 2.0f;

	// 자동 보정이 바닥을 지나치게 내려다보지 않도록 목표 Pitch를 제한한다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float FocusMinPitch = -25.0f;

	// 큰 몬스터를 볼 때 자동 보정이 위쪽으로 지나치게 기울지 않도록 제한한다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus Camera", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float FocusMaxPitch = 20.0f;

	// 수동 시점 입력 이후 남은 대기 시간이다. 저장할 설정값은 아니다.
	float LookWait = 0.0f;
};
