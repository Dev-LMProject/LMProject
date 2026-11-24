#include "LMCharacter/LMMiddleMobAIController.h"

#include "Navigation/CrowdFollowingComponent.h"

// 부모가 만든 CrowdFollowingComponent에 간격 보정을 적용한다.
ALMMiddleMobAIController::ALMMiddleMobAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UCrowdFollowingComponent* CrowdFollowing = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!CrowdFollowing) return;

	// Crowd Manager에 등록되기 전에 간격 보정 값을 설정한다.
	CrowdFollowing->SetCrowdSeparation(true, false);
	CrowdFollowing->SetCrowdSeparationWeight(SeparationWeight, false);
}
