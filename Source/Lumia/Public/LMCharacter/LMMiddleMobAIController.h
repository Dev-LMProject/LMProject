#pragma once

#include "DetourCrowdAIController.h"
#include "LMMiddleMobAIController.generated.h"

UCLASS(Blueprintable)
class LUMIA_API ALMMiddleMobAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()

public:
	// 미드몬스터가 이동 중 주변 몬스터와 간격을 벌리도록 설정한다.
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LM|Crowd")
	float SeparationWeight=0.f;
	ALMMiddleMobAIController(const FObjectInitializer& ObjectInitializer);
};
