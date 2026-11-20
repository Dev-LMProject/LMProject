#include "LMInterface/LMBreakable.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "LMCharacter/LMBaseCharacter.h"

// 검이나 착지 조회에서 발견한 부품이 해당 환경 물체의 피격 상자인지 판별한다.
bool ILMBreakable::IsHitBody(const AActor* HitActor, const UPrimitiveComponent* HitBody, ECollisionChannel HitChannel)
{
	// 몬스터는 기존 캐릭터 피해 처리로 보내고, 사라지는 물체나 일반 배경은 제외한다.
	if (!IsValid(HitActor) || HitActor->IsActorBeingDestroyed() || HitActor->IsA<ALMBaseCharacter>() || !HitActor->Implements<ULMBreakable>())
		return false;

	// 실제로 이 액터가 소유하고 있고 현재 충돌 조회가 가능한 상자만 받는다.
	const UBoxComponent* HitBox = Cast<UBoxComponent>(HitBody);
	if (!IsValid(HitBox) || HitBox->GetOwner() != HitActor || !HitBox->IsRegistered() || !HitBox->IsQueryCollisionEnabled())
		return false;

	// 기존 착지 범위 판정에 맞춰 세워 둔 상자를 사용한다. 수평 방향 회전은 가능하다.
	if (!FMath::IsNearlyEqual(FMath::Abs(HitBox->GetUpVector().Z), 1.0, 0.0001))
		return false;

	// 착지 조회에 잡히는 WorldDynamic이며 현재 공격의 검사 채널도 막아야 한다.
	if (HitBox->GetCollisionObjectType() != ECC_WorldDynamic || HitBox->GetCollisionResponseToChannel(HitChannel) != ECR_Block)
		return false;

	// 같은 액터의 문틀이나 센서가 아니라 물체가 직접 지정한 부품인지 확인한다.
	return ILMBreakable::Execute_GetBreakHitBody(HitActor) == HitBox;
}