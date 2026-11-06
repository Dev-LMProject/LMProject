#include "LMCharacter/LMBaseCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"

/** 공통 GAS 연결을 관리할 캐릭터 기반을 생성한다. */
ALMBaseCharacter::ALMBaseCharacter()
{
}

/** 캐릭터가 참조 중인 ASC를 GAS 인터페이스로 제공한다. */
UAbilitySystemComponent* ALMBaseCharacter::GetAbilitySystemComponent() const
{
	return CachedAbilitySystem.Get();
}

/** 소유자와 ASC를 현재 몸체에 연결하고 태그 구독 및 파생 클래스의 초기화를 준비한다. */
void ALMBaseCharacter::InitializeAbilitySystem(AActor* AbilityOwner, ULMAbilitySystemComponent* AbilitySystem)
{
	if (bInitializingAbilitySystem || bDisconnectingAbilitySystem)
	{
		return;
	}

	TGuardValue<bool> InitializingGuard(bInitializingAbilitySystem, true);

	if (!IsValid(AbilityOwner) || !IsValid(AbilitySystem))
	{
		DisconnectAbilitySystem();
		return;
	}

	if (CachedAbilitySystem.Get() != AbilitySystem)
	{
		DisconnectAbilitySystem();
		CachedAbilitySystem = AbilitySystem;
	}

	if (AbilitySystem->GetOwnerActor() == AbilityOwner && AbilitySystem->GetAvatarActor() == this)
	{
		// 같은 연결을 갱신할 때 실행 중인 몽타주 정보를 초기화하지 않는다.
		AbilitySystem->RefreshAbilityActorInfo();
	}
	else
	{
		AbilitySystem->InitAbilityActorInfo(AbilityOwner, this);
	}

	// ActorInfo 갱신 콜백에서 연결이 해제되거나 Avatar가 바뀌었으면 중단한다.
	if (CachedAbilitySystem.Get() != AbilitySystem || AbilitySystem->GetAvatarActor() != this)
	{
		return;
	}

	if (!AbilityTagChangedHandle.IsValid())
	{
		AbilityTagChangedHandle = AbilitySystem->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::HandleAbilityTagChanged);
	}

	OnAbilitySystemReady();

	if (CachedAbilitySystem.Get() == AbilitySystem && AbilitySystem->GetAvatarActor() == this)
	{
		OnAbilitySystemTagsChanged();
	}
}

/** 자신의 구독을 해제하고, 현재 Avatar가 자신일 때만 실행과 몸체 연결을 정리한다. */
void ALMBaseCharacter::DisconnectAbilitySystem()
{
	if (bDisconnectingAbilitySystem)
	{
		return;
	}

	TGuardValue<bool> DisconnectingGuard(bDisconnectingAbilitySystem, true);

	if (ULMAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
	{
		ASC->RegisterGenericGameplayTagEvent().Remove(AbilityTagChangedHandle);
		AbilityTagChangedHandle.Reset();

		if (ASC->GetAvatarActor() == this)
		{
			ASC->ClearAbilityInput(this);

			// 각 취소가 새 Avatar를 연결할 수 있으므로 일괄 취소 대신 매번 확인한다.
			{
				FScopedAbilityListLock AbilityListLock(*ASC);
				for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
				{
					if (ASC->GetAvatarActor() != this)
					{
						break;
					}

					if (Spec.IsActive())
					{
						ASC->CancelAbilityHandle(Spec.Handle);
					}
				}
			}

			// 능력 종료 코드가 기존 Avatar를 사용할 수 있도록 취소 후 연결을 해제한다.
			if (ASC->GetAvatarActor() == this)
			{
				ASC->SetAvatarActor(nullptr);
			}
		}
	}

	AbilityTagChangedHandle.Reset();
	CachedAbilitySystem.Reset();
	OnAbilitySystemTagsChanged();
}

/** ActorInfo 준비 후 소유자별 능력 초기화를 수행할 파생 클래스의 확장 지점이다. */
void ALMBaseCharacter::OnAbilitySystemReady()
{
}

/** 연결·태그 변경을 각 캐릭터의 행동 정책에 반영할 파생 클래스의 확장 지점이다. */
void ALMBaseCharacter::OnAbilitySystemTagsChanged()
{
}

/** 현재 Avatar의 태그 변경만 파생 클래스의 행동 정책에 전달한다. */
void ALMBaseCharacter::HandleAbilityTagChanged(FGameplayTag Tag, int32 NewCount)
{
	const ULMAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	if (ASC && ASC->GetAvatarActor() == this)
	{
		OnAbilitySystemTagsChanged();
	}
}

/** 이동 금지·사망·기절 태그에 의해 일반 이동이 제한되는지 반환한다. */
bool ALMBaseCharacter::IsMovementBlockedByAbilities() const
{
	const ULMAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	return ASC && (ASC->HasMatchingGameplayTag(LMGameplayTags::State_Movement_Blocked) || ASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead) || ASC->HasMatchingGameplayTag(LMGameplayTags::State_Stunned));
}

/** 능력이나 루트 모션이 일반 이동 대신 이동 제어를 맡고 있는지 반환한다. */
bool ALMBaseCharacter::IsMovementControlledByAbilities() const
{
	const ULMAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	return ASC && ASC->HasMatchingGameplayTag(LMGameplayTags::State_Movement_External);
}

/** 착지 직전 속도를 보존하고 실제 착지 사건을 GAS에 한 번 전달한다. */
void ALMBaseCharacter::Landed(const FHitResult& Hit)
{
	// 부모 착지 처리나 BP 이벤트가 속도를 바꾸기 전에 충돌 속도를 보존한다.
	const float ImpactSpeed = FMath::Max(0.0f, -GetVelocity().Z);
	Super::Landed(Hit);

	const ULMAbilitySystemComponent* ASC = CachedAbilitySystem.Get();
	const FGameplayTag LandedTag = LMGameplayTags::Event_Movement_Landed;
	if (ASC && ASC->GetAvatarActor() == this && LandedTag.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = LandedTag;
		Payload.Instigator = this;
		Payload.Target = this;
		Payload.EventMagnitude = ImpactSpeed;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, LandedTag, Payload);
	}
}

/** 캐릭터 종료 시 남은 GAS 연결을 해제하고 부모 종료 처리를 수행한다. */
void ALMBaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 파생 클래스가 종료 순서상 먼저 해제한 경우 같은 정리를 반복하지 않는다.
	if (CachedAbilitySystem.IsValid() || AbilityTagChangedHandle.IsValid())
	{
		DisconnectAbilitySystem();
	}

	Super::EndPlay(EndPlayReason);
}
