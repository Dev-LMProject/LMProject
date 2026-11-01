#include "LMPlayer/LMPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

/** 플레이어가 소유할 ASC를 한 번 생성한다. */
ALMPlayerState::ALMPlayerState()
{
	AbilitySystemComponent =
		CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
}

/** 이 플레이어가 사용하는 ASC를 반환한다. */
UAbilitySystemComponent* ALMPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

/** 공격 능력이 아직 없을 때만 ASC에 부여한다. */
void ALMPlayerState::EnsureAttackAbilityGranted()
{
	if (!HasAuthority() ||
		!AbilitySystemComponent ||
		!AttackAbilityClass)
	{
		return;
	}

	// Restart나 재초기화가 반복되어도 능력을 중복 부여하지 않는다.
	if (AbilitySystemComponent->FindAbilitySpecFromHandle(
			AttackAbilityHandle))
	{
		return;
	}

	AttackAbilityHandle =
		AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(
				AttackAbilityClass,
				1,
				INDEX_NONE,
				this));
}

/** 부여한 공격 능력의 식별자를 반환한다. */
FGameplayAbilitySpecHandle
ALMPlayerState::GetAttackAbilityHandle() const
{
	return AttackAbilityHandle;
}