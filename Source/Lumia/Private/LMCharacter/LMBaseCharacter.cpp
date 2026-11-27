#include "LMCharacter/LMBaseCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "GameplayEffect.h"
#include "LMCombat/LMAttackAbility.h"
#include "LMCombat/LMAttackData.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayAbilitySpec.h"
#include "LMCombat/LMDodgeAbility.h"

/** 공통 GAS 연결을 관리할 캐릭터 기반을 생성한다. */
/** 모든 캡슐형 전투 캐릭터가 같은 몸체 충돌 기본값을 사용하도록 한다. */
ALMBaseCharacter::ALMBaseCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 실제 이동을 막는 몸체를 캡슐로 통일한다.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block); // LMEnemyBody
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);

	// 적 몸체를 계단이나 지면으로 취급하지 않게 한다.
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	GetCapsuleComponent()->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));

	// 메시의 여러 물리 바디가 추가 이동 장애물이 되지 않게 한다.
	GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore); // LMEnemyBody
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
}

// 진영을 기준으로 몸체를 분류하여 기존 BP와 새 보스에도 같은 통과 규칙을 적용한다.
void ALMBaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (CombatTeam == ELMCombatTeam::Neutral) return;

	/* BP의 예전 Pawn 기본값보다 전투 몸체 분류를 우선한다.
	 * ASC 재연결 때 호출하지 않아 강공격 중의 임시 Ignore를 덮어쓰지 않는다. */
	GetCapsuleComponent()->SetCollisionObjectType(CombatTeam == ELMCombatTeam::Enemy ? ECC_GameTraceChannel4 : ECC_Pawn); // LMEnemyBody
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap); // LMCombatHit

	// BP에 저장된 메시 응답이 캡슐과 별개로 관통 이동이나 검 판정을 막지 않게 한다.
	GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore); // LMCombatHit
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

	// 이 몸체의 첫 연결에서만 초기화하고 UI 구독보다 먼저 종류별 HP를 확정한다.
	if (!bCombatStatsInitialized)
	{
		ULMCombatAttributeSet* CombatStats = const_cast<ULMCombatAttributeSet*>(AbilitySystem->GetSet<ULMCombatAttributeSet>());
		if (CombatStats && ensureMsgf(StartHealth >= 1 && StartHealth <= 100000 && CombatStats->SetCriticalSettings(CriticalChance, CriticalStep, CriticalLimit, CriticalDamagePercent), TEXT("%s: Invalid starting combat settings."), *GetName()))
		{
			bCombatStatsInitialized = true;
			CombatStats->StartNewLife(StartHealth);
		}
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

/** 
 * ActorInfo 준비 후 소유자별 능력 초기화를 수행할 파생 클래스의 확장 지점이다. 
 * ASC 연결 후 최종 BP 설정이 공통 캡슐 전투 규칙을 만족하는지 확인한다.
 */
void ALMBaseCharacter::OnAbilitySystemReady()
{
	// 비전투 대상에는 같은 몸체 규칙을 강제하지 않는다.
	if (CombatTeam == ELMCombatTeam::Neutral)
	{
		return;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const USkeletalMeshComponent* CharacterMesh = GetMesh();

	if (!ensure(Capsule && CharacterMesh))
	{
		return;
	}

	// 자식 BP가 부모 설정을 덮어썼을 때 어느 캐릭터가 잘못됐는지 알린다.
	const ECollisionChannel BodyChannel = CombatTeam == ELMCombatTeam::Enemy ? ECC_GameTraceChannel4 : ECC_Pawn; // LMEnemyBody
	const bool bBodyValid = Capsule->IsQueryCollisionEnabled() && Capsule->GetCollisionObjectType() == BodyChannel && Capsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && Capsule->CanCharacterStepUpOn == ECB_No && Capsule->GetWalkableSlopeOverride().WalkableSlopeBehavior == WalkableSlope_Unwalkable;

	ensureMsgf(bBodyValid, TEXT("%s: Capsule needs its team body channel, query collision, Pawn block, No Step Up, Unwalkable."), *GetName());

	ensureMsgf(Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel3) == ECR_Overlap, TEXT("%s: Capsule LMCombatHit must be Overlap."), *GetName());

	// 충돌 검사를 사용하는 메시만 확인하여 몸체 캡슐과의 중복 충돌을 방지한다.
	//ECC_GameTraceChannel3 = LMCombatHit
	//ECC_GameTraceChannel4 = LMEnemyBody
	
	// Query 충돌을 사용하는 메시만 채널별로 검사한다.
	if (CharacterMesh->IsQueryCollisionEnabled())
	{
		const ECollisionResponse PawnResponse = CharacterMesh->GetCollisionResponseToChannel(ECC_Pawn);
		const ECollisionResponse EnemyBodyResponse = CharacterMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel4);
		const ECollisionResponse CombatHitResponse = CharacterMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel3);

		// 이동 충돌은 캡슐이 담당하므로 메시의 몸체 채널은 Ignore여야 한다.
		ensureMsgf(PawnResponse == ECR_Ignore, TEXT("%s: Mesh [Pawn] must be Ignore. Actual=%d"), *GetName(), static_cast<int32>(PawnResponse));
		ensureMsgf(EnemyBodyResponse == ECR_Ignore, TEXT("%s: Mesh [LMEnemyBody] must be Ignore. Actual=%d"), *GetName(), static_cast<int32>(EnemyBodyResponse));

		// 메시 피격을 위한 Overlap은 허용하되 전투 Trace를 막는 Block은 금지한다.
		ensureMsgf(CombatHitResponse == ECR_Ignore || CombatHitResponse == ECR_Overlap, TEXT("%s: Mesh [LMCombatHit] must be Ignore or Overlap. Actual=%d"), *GetName(), static_cast<int32>(CombatHitResponse));
	}
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

/** 착지 직전 속도와 지면 충돌 정보를 실제 착지 사건으로 GAS에 전달한다. */
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

		// 착지 공격이 캡슐 중심 대신 실제 지면 접점을 읽도록 충돌 정보를 복사한다.
		Payload.ContextHandle = ASC->MakeEffectContext();
		Payload.ContextHandle.AddHitResult(Hit, true);

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

// 공유 공격 데이터에서 피해를 읽고 일반 타격의 검증과 방어 판정을 요청한다.
void ALMBaseCharacter::ReceiveCombatDamage(AActor* DamageSource, const ULMAttackData* AttackData, const FHitResult& HitResult)
{
    if (!IsValid(AttackData)) return;
    ALMBaseCharacter* SourceBody = Cast<ALMBaseCharacter>(DamageSource);
    if (!IsValid(SourceBody)) return;
    ApplyCombatDamage(SourceBody, SourceBody->GetAttackDamage(AttackData), AttackData, HitResult, FGameplayTag());
}

// 플레이어와 보스는 기존 DA의 일반 피해를 그대로 사용한다.
float ALMBaseCharacter::GetAttackDamage(const ULMAttackData* AttackData) const
{
    return IsValid(AttackData) ? AttackData->BaseDamage : 0.0f;
}

// 일반 피해와 시연 피해가 같은 출처·GE·속성·사망 경로를 사용하도록 적용 부분을 공유한다.
bool ALMBaseCharacter::ApplyCombatDamage(ALMBaseCharacter* DamageSource, float RequestedDamage, const ULMAttackData* AttackData, const FHitResult& HitResult, FGameplayTag SceneTag)
{
    if (!HasAuthority() || IsActorBeingDestroyed()) return false;
    if (!IsValid(DamageSource) || DamageSource->IsActorBeingDestroyed() || DamageSource->GetWorld() != GetWorld())
        return false;
    int32 DamageValue = 0;
    if (!ULMCombatAttributeSet::TryGetDamage(RequestedDamage, 100, DamageValue))
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Damage must be a whole number from 0 to 100000: %f"), *GetNameSafe(AttackData), RequestedDamage);
        return false;
    }

    // 명시한 두 시연 이유만 방어와 일반 피해 제한을 우회할 수 있다.
    const bool bSceneHit = SceneTag == LMGameplayTags::Damage_SceneHit || SceneTag == LMGameplayTags::Damage_SceneFinish;
    if (SceneTag.IsValid() && !bSceneHit) return false;
    if (bSceneHit)
    {
        if (!IsHostileTo(DamageSource) || !IsAliveForCombat() || !DamageSource->IsAliveForCombat())
            return false;
    }
    else if (!IsValid(AttackData) || !CanReceiveAttackFrom(DamageSource))
    {
        return false;
    }

    UAbilitySystemComponent* ReceiverASC = GetAbilitySystemComponent();
    UAbilitySystemComponent* AttackerASC = DamageSource->GetAbilitySystemComponent();
    if (!IsValid(ReceiverASC) || ReceiverASC->GetAvatarActor() != this || !IsValid(AttackerASC) || AttackerASC->GetAvatarActor() != DamageSource)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Attacker or receiver ASC is not ready."), *GetName());
        return false;
    }

    const ULMCombatAttributeSet* ReceiverAttributes = ReceiverASC->GetSet<ULMCombatAttributeSet>();
    if (!ReceiverAttributes || ReceiverAttributes->GetHealth() <= 0.0f || ReceiverASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead))
        return false;

    const UGameplayEffect* DamageEffectDefaults = DamageEffectClass.GetDefaultObject();
    if (!IsValid(DamageEffectDefaults) || DamageEffectDefaults->DurationPolicy != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMCombat] %s: Assign the Instant GE_LM_Damage."), *GetName());
        return false;
    }

    // Owner가 PlayerState여도 EffectCauser에는 공격자 몸체가 남는다.
    FGameplayEffectContextHandle DamageContext = AttackerASC->MakeEffectContext();
    if (AttackData)
        DamageContext.AddSourceObject(AttackData);
    if (HitResult.GetActor())
        DamageContext.AddHitResult(HitResult, true);

    FGameplayEffectSpecHandle DamageSpec = AttackerASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, DamageContext);
    if (!DamageSpec.IsValid()) return false;
    if (bSceneHit)
        DamageSpec.Data->AddDynamicAssetTag(SceneTag);

    // 일반 유효 타격만 저스트 회피와 패링을 거치며 시연 예외는 이 Spec에만 남긴다.
    if (!bSceneHit && RequestedDamage > 0.0f)
    {
        for (const FGameplayAbilitySpec& DodgeSpec : ReceiverASC->GetActivatableAbilities())
        {
            if (!DodgeSpec.IsActive())
                continue;
            ULMDodgeAbility* const DodgeAbility = Cast<ULMDodgeAbility>(DodgeSpec.GetPrimaryInstance());
            if (DodgeAbility && DodgeAbility->TryAvoidDamage()) return false;
        }
        ULMAttackAbility* const ParryAbility = Cast<ULMAttackAbility>(ReceiverASC->GetAnimatingAbility());
        if (ParryAbility && ParryAbility->TryParryHit())
        {
            DamageSource->ReceiveParryReaction(this);
            return false;
        }
    }

    // 방어를 통과한 타격만 판정한다. 공격자의 누적 확정은 실제 HP 감소 지점에서 한다.
    const ULMCombatAttributeSet* AttackerStats = AttackerASC->GetSet<ULMCombatAttributeSet>();
    if (!bSceneHit && DamageValue > 0 && AttackData->bAllowCritical && AttackerStats && AttackerStats->RollCritical())
    {
        if (!ULMCombatAttributeSet::TryGetDamage(RequestedDamage, AttackerStats->GetCriticalDamagePercent(), DamageValue))
            return false;
        DamageSpec.Data->AddDynamicAssetTag(LMGameplayTags::Damage_Critical);
    }
    DamageSpec.Data->SetSetByCallerMagnitude(LMGameplayTags::Data_Damage, static_cast<float>(DamageValue));
    ReceiverASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
    return true;
}

// 별도 규칙이 없는 캐릭터의 피해량은 바꾸지 않는다.
float ALMBaseCharacter::ChangeIncomingDamage(const FGameplayEffectSpec&, float RequestedDamage, float, float, bool&)
{
    return RequestedDamage;
}

// 기본 캐릭터에는 피해 계산 뒤 실행할 예약 반응이 없다.
void ALMBaseCharacter::FinishCombatDamage()
{
}

// 캐릭터 종류에 따라 필요한 경우에만 패링 반응을 구현한다.
void ALMBaseCharacter::ReceiveParryReaction(ALMBaseCharacter*)
{
}

/** 진영 관계는 생존 여부와 분리하여 몸체 충돌 처리에서도 사용한다. */
bool ALMBaseCharacter::IsHostileTo(const ALMBaseCharacter* Other) const
{
	return IsValid(Other) && Other != this && CombatTeam != ELMCombatTeam::Neutral && Other->CombatTeam != ELMCombatTeam::Neutral && CombatTeam != Other->CombatTeam;
}

/** 오래된 Avatar 연결이나 체력이 소진된 몸체를 전투 대상으로 사용하지 않는다. */
bool ALMBaseCharacter::IsAliveForCombat() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const ULMCombatAttributeSet* Attributes = IsValid(ASC) ? ASC->GetSet<ULMCombatAttributeSet>() : nullptr;

	return !IsActorBeingDestroyed() && ASC && ASC->GetAvatarActor() == this && Attributes && FMath::IsFinite(Attributes->GetHealth()) && Attributes->GetHealth() > 0.0f && !ASC->HasMatchingGameplayTag(LMGameplayTags::State_Dead);
}

/** 접근 허용과 적대 관계·생존 조건을 함께 확인한다. */
bool ALMBaseCharacter::CanBeAttackTargetFor(const ALMBaseCharacter* Attacker) const
{
	return bCanBeAttackTarget && IsHostileTo(Attacker) && IsAliveForCombat() && Attacker->IsAliveForCombat();
}

/** 접근 허용과 독립적으로 피해 허용 여부를 판단한다. */
bool ALMBaseCharacter::CanReceiveAttackFrom(const ALMBaseCharacter* Attacker) const
{
	return bCanReceiveCombatDamage && IsHostileTo(Attacker) && IsAliveForCombat() && Attacker->IsAliveForCombat();
}
