// Fill out your copyright notice in the Description page of Project Settings.


#include "LMCombat/LMANSDodgeParry.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "LMCombat/LMAttackAbility.h"
#include "LMCombat/LMDodgeAbility.h"

// 몽타주의 현재 재생 ID를 담아 전달하는 Branching Point를 사용한다.
ULMANSDodgeParry::ULMANSDodgeParry()
{
	bIsNativeBranchingPoint = true;

#if WITH_EDITORONLY_DATA
	// 실제 판정 창은 게임에서만 연다.
	bShouldFireInEditor = false;
#endif
}

// 이 몽타주 배치의 판정 구간 시작을 전달한다.
void ULMANSDodgeParry::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	UpdateWindow(BranchingPointPayload, true);
}

// 이 몽타주 배치의 판정 구간 끝을 전달한다.
void ULMANSDodgeParry::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	UpdateWindow(BranchingPointPayload, false);
}

// 알림을 보낸 몸체의 회피 또는 공격 GA로 경계를 보낸다.
void ULMANSDodgeParry::UpdateWindow(const FBranchingPointNotifyPayload& Payload, bool bOpen) const
{
	USkeletalMeshComponent* const WindowMesh = Payload.SkelMeshComponent;
	AActor* const WindowOwner = IsValid(WindowMesh) ? WindowMesh->GetOwner() : nullptr;
	const UWorld* const WindowWorld = IsValid(WindowMesh) ? WindowMesh->GetWorld() : nullptr;

	// 실제 피해를 처리하는 게임 월드의 유효한 몸체만 선택한다.
	if (!IsValid(WindowOwner) || !WindowWorld || !WindowWorld->IsGameWorld() || !WindowOwner->HasAuthority() || WindowOwner->IsActorBeingDestroyed())
	{
		return;
	}

	// 플레이어의 ASC 소유자는 PlayerState일 수도 있으므로 현재 Avatar를 비교한다.
	const UAbilitySystemComponent* const WindowASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(WindowOwner);
	if (!IsValid(WindowASC) || WindowASC->GetAvatarActor() != WindowOwner)
	{
		return;
	}

	// 여러 슬롯이 재생돼도 이 몽타주의 실재생 ID를 기록한 GA만 구간을 바꾼다.
	for (const FGameplayAbilitySpec& DodgeSpec : WindowASC->GetActivatableAbilities())
	{
		if (!DodgeSpec.IsActive())
			continue;

		ULMDodgeAbility* const DodgeAbility = Cast<ULMDodgeAbility>(DodgeSpec.GetPrimaryInstance());
		if (DodgeAbility && DodgeAbility->SetJustDodgeWindow(Payload, bOpen))
		{
			return;
		}
	}

	// 회피 GA가 받지 않은 알림은 현재 공격 GA에 배치 객체와 함께 전달한다.
	if (ULMAttackAbility* const AttackAbility = Cast<ULMAttackAbility>(WindowASC->GetAnimatingAbility()))
		AttackAbility->SetParryWindow(Payload, *this, bOpen);
}
