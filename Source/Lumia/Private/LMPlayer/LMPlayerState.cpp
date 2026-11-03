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

/** 현재 캐릭터를 ASC의 행동 대상으로 연결하고 기본 능력을 준비한다. */
void ALMPlayerState::InitializeAbilitySystem(AActor* PlayerCharacter)
{
	if (!AbilitySystemComponent || !IsValid(PlayerCharacter))
	{
		return;
	}

	if (AbilitySystemComponent->GetAvatarActor() == PlayerCharacter)
	{
		// 반복 초기화 시 재생 중인 몽타주 관리 정보를 초기화하지 않는다.
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}
	else
	{
		// 능력의 소유자는 PlayerState, 실제 행동 대상은 플레이어 캐릭터다.
		AbilitySystemComponent->InitAbilityActorInfo(this, PlayerCharacter);
	}

	EnsureStartupAbilitiesGranted();
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

/** 블루프린트에 지정한 기본 AbilitySet들을 최초 한 번 등록한다. */
void ALMPlayerState::EnsureStartupAbilitiesGranted()
{
	if (!AbilitySystemComponent || !HasAuthority() || bStartupAbilitiesGranted)
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

		// 같은 AbilitySet을 배열에 중복 지정한 경우 한 번만 등록한다.
		GrantedSets.Add(AbilitySet);

		AbilitySet->GiveAbilities(AbilitySystemComponent.Get(), this, StartupGrantedAbilities);
	}
}