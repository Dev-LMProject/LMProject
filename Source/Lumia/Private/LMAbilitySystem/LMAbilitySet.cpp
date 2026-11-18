#include "LMAbilitySystem/LMAbilitySet.h"

#include "Abilities/GameplayAbility.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"

/** 이 묶음이 뷰요헌 능력만 제거하며 다른 능력은 유지. */
void FLMAbilitySetGrantedHandles::RemoveFromAbilitySystem()
{
	ULMAbilitySystemComponent* ASC = AbilitySystem.Get();
	
	if (ASC && !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	
	if (ASC)
	{
		for (const FGameplayAbilitySpecHandle Handle : Handles)
		{
			ASC -> ClearAbility(Handle);
		}
	}
	
	Handles.Reset();
	AbilitySystem.Reset();
}

/** 데이터에 지정된 능력을 부여하고 입력 태그와 출처를 기록한다. */
void ULMAbilitySet::GiveAbilities(ULMAbilitySystemComponent* AbilitySystem, UObject* SourceObject, FLMAbilitySetGrantedHandles& OutGrantedHandles) const
{
	if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 서로 다른 ASC의 부여 기록이 섞이지 않도록 막는다.
	if (OutGrantedHandles.AbilitySystem.IsValid() && OutGrantedHandles.AbilitySystem.Get() != AbilitySystem)
	{
		ensureMsgf(false, TEXT("Do not share grant records between ASCs."));
		return;
	}

	if (!OutGrantedHandles.AbilitySystem.IsValid())
	{
		OutGrantedHandles.Handles.Reset();
	}

	OutGrantedHandles.AbilitySystem = AbilitySystem;

	for (const FLMAbilitySetEntry& Entry : Abilities)
	{
		if (!Entry.AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec Spec(Entry.AbilityClass, FMath::Max(1, Entry.AbilityLevel), INDEX_NONE, SourceObject);

		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = AbilitySystem->GiveAbility(Spec);

		if (Handle.IsValid())
		{
			OutGrantedHandles.Handles.Add(Handle);
		}
	}
}