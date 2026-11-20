// Fill out your copyright notice in the Description page of Project Settings.


#include "LMCombat/LMANSJustDodge.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "LMCombat/LMDodgeAbility.h"

// 몽타주의 현재 재생 ID를 담아 전달하는 Branching Point를 사용한다.
ULMANSJustDodge::ULMANSJustDodge()
{
	bIsNativeBranchingPoint = true;

#if WITH_EDITORONLY_DATA
	// 애니메이션 에디터 미리보기에서 실제 회피 상태를 열지 않는다.
	bShouldFireInEditor = false;
#endif
}

// 지금 재생 중인 회피에서 저스트 회피 판정 구간을 연다.
void ULMANSJustDodge::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	UpdateDodgeWindow(BranchingPointPayload, true);
}

// 현재 회피의 판정 구간을 닫도록 알린다.
void ULMANSJustDodge::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	UpdateDodgeWindow(BranchingPointPayload, false);
}

// 이 알림이 발생한 몸체의 ASC에 등록된 활성 회피에만 알림을 보낸다.
void ULMANSJustDodge::UpdateDodgeWindow(const FBranchingPointNotifyPayload& Payload, bool bOpen) const
{
	USkeletalMeshComponent* const DodgeMesh = Payload.SkelMeshComponent;
	AActor* const DodgeOwner = IsValid(DodgeMesh) ? DodgeMesh->GetOwner() : nullptr;
	const UWorld* const DodgeWorld = IsValid(DodgeMesh) ? DodgeMesh->GetWorld() : nullptr;

	// 실제 피해를 처리하는 게임 월드의 유효한 몸체만 선택한다.
	if (!IsValid(DodgeOwner) || !DodgeWorld || !DodgeWorld->IsGameWorld() || !DodgeOwner->HasAuthority() || DodgeOwner->IsActorBeingDestroyed())
	{
		return;
	}

	// 플레이어의 ASC 소유자는 PlayerState일 수도 있으므로 현재 Avatar를 비교한다.
	const UAbilitySystemComponent* const DodgeASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DodgeOwner);
	if (!IsValid(DodgeASC) || DodgeASC->GetAvatarActor() != DodgeOwner)
	{
		return;
	}

	// 여러 슬롯이 재생돼도 이 몽타주의 실재생 ID를 기록한 GA만 구간을 바꾼다.
	for (const FGameplayAbilitySpec& DodgeSpec : DodgeASC->GetActivatableAbilities())
	{
		if (!DodgeSpec.IsActive())
			continue;

		ULMDodgeAbility* const DodgeAbility = Cast<ULMDodgeAbility>(DodgeSpec.GetPrimaryInstance());
		if (DodgeAbility && DodgeAbility->SetJustDodgeWindow(Payload, bOpen))
		{
			return;
		}
	}
}
