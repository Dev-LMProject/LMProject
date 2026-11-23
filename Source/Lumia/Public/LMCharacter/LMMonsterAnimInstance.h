#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LMMonsterAnimInstance.generated.h"

class ALMFieldMonsterCharacter;

// 몬스터의 실제 이동과 반응을 AnimBP에 전달하는 표시용 값이다.
USTRUCT(BlueprintType)
struct LUMIA_API FLMMonsterLocomotionFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float MoveSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bMoving = false;

	UPROPERTY(BlueprintReadOnly)
	bool bHitReacting = false;

	UPROPERTY(BlueprintReadOnly)
	bool bParryRetreating = false;

	UPROPERTY(BlueprintReadOnly)
	bool bGroggy = false;

	UPROPERTY(BlueprintReadOnly)
	bool bDead = false;
};

// 몬스터의 상태를 읽어 애니메이션 표시 값만 갱신한다.
UCLASS(Transient, Blueprintable)
class LUMIA_API ULMMonsterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 애니메이션 재초기화 시 이전 소유자와 표시 값을 비운다.
	virtual void NativeInitializeAnimation() override;

	// 실제 속도와 몬스터의 피격·사망 상태를 매 프레임 읽는다.
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "LM|Locomotion")
	FLMMonsterLocomotionFrame Locomotion;

private:
	// 이전 이동 표시가 어느 몬스터의 값인지 구분하며 수명을 유지하지 않는다.
	TWeakObjectPtr<ALMFieldMonsterCharacter> CachedMonster;
};
