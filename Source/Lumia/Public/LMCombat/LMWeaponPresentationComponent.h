#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LMWeaponPresentationComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (LM), meta = (BlueprintSpawnableComponent))
class LUMIA_API ULMWeaponPresentationComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	/** 별도 매 프레임 갱신 없이 사용하는 컴포넌트를 구성한다. */
	ULMWeaponPresentationComponent();

	/** 몸과 검 메시를 연결하고 애니메이션 갱신 순서를 지정한다. */
	void Initialize(
		USkeletalMeshComponent* InBodyMesh,
		USkeletalMeshComponent* InSwordMesh);

	/** 몸·검 몽타주의 길이와 섹션 시간 배치가 맞는지 검사한다. */
	bool CanPlayPair(
		const UAnimMontage* BodyMontage,
		const UAnimMontage* SwordMontage) const;

	/** 이미 재생 중인 몸 몽타주를 검이 따라가도록 시작한다. */
	bool StartFollowingBody(
		UAnimMontage* BodyMontage,
		UAnimMontage* SwordMontage,
		uint32 ExecutionToken);

	/** 해당 실행이 시작한 검 몽타주만 정리한다. */
	void StopFollowingBody(
		uint32 ExecutionToken,
		float BlendOutSeconds);

protected:
	/** 컴포넌트 종료 시 남은 재생과 갱신 순서 연결을 정리한다. */
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 검 애니메이션이 따라갈 기준 몸 메시. 수명을 유지하지 않는 약한 참조. */
	TWeakObjectPtr<USkeletalMeshComponent> BodyMesh;

	/** 몸 몽타주에 맞춰 검 몽타주를 재생할 메시. 수명을 유지하지 않는 약한 참조. */
	TWeakObjectPtr<USkeletalMeshComponent> SwordMesh;

	/** 현재 검 몽타주를 재생하는 AnimInstance. 재생 정리 시 사용한다. */
	TWeakObjectPtr<UAnimInstance> PlayingSwordAnim;

	/** 현재 실행에서 검에 재생한 몽타주 에셋. */
	TWeakObjectPtr<UAnimMontage> PlayingSwordMontage;

	/** 현재 따라가기 실행의 식별자. 이전 실행의 정리 요청과 구분한다. */
	uint32 ActiveExecutionToken = 0;

	/** 검 몽타주의 재생 인스턴스 ID. 같은 몽타주의 다른 재생과 구분한다. */
	int32 SwordMontageInstanceID = INDEX_NONE;
};