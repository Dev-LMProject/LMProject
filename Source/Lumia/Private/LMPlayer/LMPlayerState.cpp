#include "LMPlayer/LMPlayerState.h"

#include "LMAbilitySystem/LMAbilitySystemComponent.h"

/** PlayerState가 유지되는 동안 사용할 ASC를 생성한다. */
ALMPlayerState::ALMPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<ULMAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 싱글 전용이므로 ASC의 네트워크 복제를 사용하지 않는다.
	AbilitySystemComponent->SetIsReplicated(false);
}

/** 외부 시스템이 GAS 인터페이스로 접근할 수 있도록 ASC를 반환한다. */
UAbilitySystemComponent* ALMPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

/** PlayerState의 플레이 종료 시 남은 입력과 능력 실행을 정리한다. */
void ALMPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->ClearAbilityInput();

		// 종료 시점이므로 취소 가능한 실행 중 능력에 취소를 요청한다.
		AbilitySystemComponent->CancelAbilities();
	}

	Super::EndPlay(EndPlayReason);
}

/** 캐릭터의 ASC 연결이 준비되면 BP에 지정한 기본 AbilitySet들을 최초 한 번 등록한다. */
void ALMPlayerState::EnsureStartupAbilitiesGranted()
{
	if (!AbilitySystemComponent || !HasAuthority() || bStartupAbilitiesGranted)
	{
		return;
	}

	// ASC가 아직 PlayerState 자신을 Avatar로 갖는 초기 등록 단계에서는 부여하지 않는다.
	if (AbilitySystemComponent->GetOwnerActor() != this || !IsValid(AbilitySystemComponent->GetAvatarActor()) || AbilitySystemComponent->GetAvatarActor() == this)
	{
		return;
	}

	// 능력 부여 중 초기화가 다시 호출되더라도 중복 등록하지 않는다.
	bStartupAbilitiesGranted = true;

	TSet<const ULMAbilitySet*> GrantedSets;

	for (const ULMAbilitySet* AbilitySet : StartupAbilitySets)
	{
		if (!IsValid(AbilitySet) || GrantedSets.Contains(AbilitySet))
		{
			continue;
		}

		GrantedSets.Add(AbilitySet);

		AbilitySet->GiveAbilities(AbilitySystemComponent.Get(), this, StartupGrantedAbilities);
	}
}
