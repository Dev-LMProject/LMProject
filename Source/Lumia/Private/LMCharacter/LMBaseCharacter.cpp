#include "LMCharacter/LMBaseCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "GameplayEffect.h"
#include "LMCombat/LMAttackData.h"
#include "LMCombat/LMCombatAttributeSet.h"

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

/** 캐릭터 종류와 관계없이 공격 출처를 보존해 이 피격자에게 피해를 적용하고 HP 계산은 AttributeSet에 맡긴다. */
void ALMBaseCharacter::ReceiveCombatDamage(AActor* DamageSource, const ULMAttackData* AttackData, const FHitResult& HitResult)
{
    // this는 피격자다. 권한이 없거나 파괴 중인 몸체에서는 HP 변경을 요청하지 않는다.
    if (!HasAuthority() || IsActorBeingDestroyed())
    {
        return;
    }

    // DamageSource는 공격한 캐릭터다. 공격 출처나 피해 입력을 읽을 수 없으면 중단한다.
    if (!IsValid(DamageSource) || DamageSource->IsActorBeingDestroyed() || !IsValid(AttackData))
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Invalid attacker or attack data."), *GetName());
        return;
    }

    // 공유 DataAsset은 읽기만 한다. 음수·비정상 수치는 거부하며 0은 AttributeSet에서 피해 없이 소비한다.
    const float BaseDamage = AttackData->BaseDamage;
    if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f)
    {
        return;
    }

    // 피격자가 사용하는 ASC다. 플레이어·일반 몬스터·보스 중 누구든 최종 피해 적용 대상이 된다.
    UAbilitySystemComponent* ReceiverASC = GetAbilitySystemComponent();

    // 공격자가 사용하는 ASC다. 실제 소유 위치와 관계없이 공격 출처를 가진 Context와 Spec을 만든다.
    UAbilitySystemComponent* AttackerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamageSource);

    // Owner는 PlayerState일 수도 있지만 Avatar는 현재 몸체여야 한다. 교체 전 몸체와 남은 연결을 거부한다.
    if (!IsValid(ReceiverASC) || ReceiverASC->GetAvatarActor() != this || !IsValid(AttackerASC) || AttackerASC->GetAvatarActor() != DamageSource)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Attacker or receiver ASC is not ready."), *GetName());
        return;
    }

    // 공격자의 수치와 혼동하지 않도록 피격자에게 이미 등록된 전투 속성만 조회한다.
    const ULMCombatAttributeSet* ReceiverAttributes = ReceiverASC->GetSet<ULMCombatAttributeSet>();
    if (!ReceiverAttributes)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: CombatAttributeSet is missing."), *GetName());
        return;
    }

    // HP 소진이나 기존 사망 상태에서는 추가 피해를 막되, 이 함수가 사망·무력화를 결정하지는 않는다.
    if (ReceiverAttributes->GetHealth() <= 0.0f || ReceiverASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead))
    {
        return;
    }

    // GE의 기본 객체(CDO)는 설정 확인용이다. 아직 효과를 적용하지 않으며 Instant 정책만 허용한다.
    const UGameplayEffect* DamageEffectDefaults = DamageEffectClass.GetDefaultObject();
    if (!IsValid(DamageEffectDefaults) || DamageEffectDefaults->DurationPolicy != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Assign the Instant GE_LM_Damage."), *GetName());
        return;
    }

    // 피격자가 접수하더라도 피해 출처는 공격자다. 공격자 ASC의 Owner·Avatar를 기준으로 Context를 만든다.
    FGameplayEffectContextHandle DamageContext = AttackerASC->MakeEffectContext();

    // 어떤 공격 데이터를 사용했는지 기록한다. 피해 숫자는 아래 SetByCaller로 별도 전달한다.
    DamageContext.AddSourceObject(AttackData);

    // 충돌 정보가 있으면 Context에 복사한다. 실제 적중 판정과 중복 타격 제한은 호출자가 담당한다.
    if (HitResult.GetActor())
    {
        DamageContext.AddHitResult(HitResult, true);
    }

    // 이번 피해 요청 전용 Spec을 만든다. 1.0은 GE 레벨이며 피해량이나 피해 배율이 아니다.
    FGameplayEffectSpecHandle DamageSpec = AttackerASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, DamageContext);
    if (!DamageSpec.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Failed to create damage Spec."), *GetName());
        return;
    }

    // GE가 같은 DataTag로 읽을 기본 피해를 저장한다. 이 시점에는 아직 HP를 변경하지 않는다.
    DamageSpec.Data->SetSetByCallerMagnitude(LMGameplayTags::Data_Damage, BaseDamage);

    // 공격 출처가 담긴 Spec을 피격자 자신에게 적용한다. HP 차감·소진 결과 알림은 AttributeSet이 담당한다.
    ReceiverASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
}
