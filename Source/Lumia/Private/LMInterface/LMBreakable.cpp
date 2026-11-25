#include "LMInterface/LMBreakable.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
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

// 실제 타격을 적용하지 않고 전방의 유효한 파괴 물체만 조회하여 사전 연출 판단에 사용한다.
AActor* ILMBreakable::FindHitActorAhead(const AActor* SourceActor, float CheckDistance, float CheckRadius, ECollisionChannel HitChannel)
{
	if (!IsValid(SourceActor)) return nullptr;
	UWorld* HitWorld = SourceActor->GetWorld();
	if (!IsValid(HitWorld)) return nullptr;

	// 잘못된 수치나 채널을 충돌 조회에 전달하지 않는다. 거리 0은 사전 조회를 끄는 값이다.
	if (!FMath::IsFinite(CheckDistance) || CheckDistance <= 0.0f || !FMath::IsFinite(CheckRadius) || CheckRadius <= 0.0f || HitChannel >= ECC_MAX || HitChannel == ECC_OverlapAll_Deprecated)
		return nullptr;

	const FVector CheckStart = SourceActor->GetActorLocation();
	const FVector CheckEnd = CheckStart + SourceActor->GetActorForwardVector() * CheckDistance;
	FCollisionQueryParams HitQuery(SCENE_QUERY_STAT(LMBreakableFrontHit), false, SourceActor);
	TArray<FHitResult> FrontHits;

	/*
	 * LMCombatHit은 캐릭터를 Overlap, 벽을 Block으로 처리하므로 Multi 조회로 둘 다 받는다.
	 * 첫 차단 지점까지만 조회하여 다른 벽 뒤의 파괴 물체를 고르지 않는다.
	 */
	HitWorld->SweepMultiByChannel(FrontHits, CheckStart, CheckEnd, FQuat::Identity, HitChannel, FCollisionShape::MakeSphere(CheckRadius), HitQuery);

	AActor* FrontHitActor = nullptr;
	for (const FHitResult& FrontHit : FrontHits)
	{
		AActor* FoundActor = FrontHit.GetActor();

		// 파괴 물체 앞의 캐릭터를 때리는 상황에서는 환경 물체용 연출을 선택하지 않는다.
		if (IsValid(FoundActor) && FoundActor->IsA<ALMBaseCharacter>())
			return nullptr;

		// 파괴 여부와 실제 피격 상자 조건은 검·착지가 사용하는 기존 계약을 재사용한다.
		if (IsHitBody(FoundActor, FrontHit.GetComponent(), HitChannel))
			FrontHitActor = FoundActor;
	}

	// shortcut: 사전 조회이므로 이후 실제 명중은 보장하지 않는다. 정확한 명중별 연출이 필요해지면 피격 결과를 사용한다.
	return FrontHitActor;
}
