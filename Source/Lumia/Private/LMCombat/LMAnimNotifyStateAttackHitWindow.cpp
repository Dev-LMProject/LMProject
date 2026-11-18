#include "LMCombat/LMAnimNotifyStateAttackHitWindow.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LMGameplayTags/LMGameplayTags.h"

/** 타격 경계 알림을 몽타주 진행 중 동기적으로 받도록 설정한다. */
ULMAnimNotifyStateAttackHitWindow::ULMAnimNotifyStateAttackHitWindow()
{
	bIsNativeBranchingPoint = true;

#if WITH_EDITORONLY_DATA
	// 애니메이션 미리보기에서는 실제 전투 이벤트를 보내지 않는다.
	bShouldFireInEditor = false;
#endif
}

/** 타격 구간의 시작을 알리며 직접 충돌 검사나 피해 계산을 하지는 않는다. */
void ULMAnimNotifyStateAttackHitWindow::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	SendHitWindowEvent(BranchingPointPayload, LMGameplayTags::Event_Combat_HitWindow_Begin);
}

/** 정상 종료일 때만 마지막 타격 샘플을 허용하고 중단 시에는 취소를 알린다. */
void ULMAnimNotifyStateAttackHitWindow::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	const FGameplayTag EventTag = BranchingPointPayload.bReachedEnd ? LMGameplayTags::Event_Combat_HitWindow_End : LMGameplayTags::Event_Combat_HitWindow_Cancel;

	SendHitWindowEvent(BranchingPointPayload, EventTag);
}

/** 같은 몽타주의 이전 재생과 구분할 수 있도록 공격자와 실행 ID를 이벤트에 담는다. */
void ULMAnimNotifyStateAttackHitWindow::SendHitWindowEvent(const FBranchingPointNotifyPayload& BranchingPointPayload, FGameplayTag EventTag) const
{
	USkeletalMeshComponent* AttackerMesh = BranchingPointPayload.SkelMeshComponent;
	AActor* Attacker = IsValid(AttackerMesh) ? AttackerMesh->GetOwner() : nullptr;
	UWorld* World = IsValid(AttackerMesh) ? AttackerMesh->GetWorld() : nullptr;
	
	// 현재 피해 경로는 권한이 있는 게임 월드에서만 실행한다.
	if (!IsValid(Attacker) || !World || !World->IsGameWorld() || !Attacker->HasAuthority() || Attacker->IsActorBeingDestroyed())
	{
		return;
	}
	
	// EventMagnitude의 float에 정수 ID를 정확하게 담을 수 있는 범위만 사용한다.
	const int32 InstanceID = BranchingPointPayload.MontageInstanceID;
	if (InstanceID < 0 || InstanceID > 16777216)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Attacker;
	Payload.OptionalObject = BranchingPointPayload.SequenceAsset;
	Payload.OptionalObject2 = this;

	// 이 이벤트에서 EventMagnitude는 피해량이 아니라 몽타주 재생 ID다.
	Payload.EventMagnitude = static_cast<float>(InstanceID);

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Attacker, EventTag, Payload);
}
