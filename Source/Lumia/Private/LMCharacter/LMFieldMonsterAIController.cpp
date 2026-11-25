#include "LMCharacter/LMFieldMonsterAIController.h"

#include "Navigation/CrowdFollowingComponent.h"

// 블루프린트에 지정한 간격 보정 값을 군중 이동에 적용한다.
void ALMFieldMonsterAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UCrowdFollowingComponent* CrowdFollowing = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!CrowdFollowing) return;

	// 이미 Crowd Manager에 등록된 경우에도 설정을 갱신한다.
	CrowdFollowing->SetCrowdSeparation(true);
	CrowdFollowing->SetCrowdSeparationWeight(SeparationWeight);
}
