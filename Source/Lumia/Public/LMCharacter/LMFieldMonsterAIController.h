#pragma once

#include "DetourCrowdAIController.h"
#include "LMFieldMonsterAIController.generated.h"

UCLASS(Blueprintable)
class LUMIA_API ALMFieldMonsterAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()

public:
	// 필드 몬스터가 이동 중 주변 몬스터와 간격을 벌리도록 설정한다.
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LM|Crowd")
	float SeparationWeight=0.f;

protected:
	// 블루프린트 기본값이 준비된 뒤 군중 이동에 간격 보정을 적용한다.
	virtual void PostInitializeComponents() override;
};
