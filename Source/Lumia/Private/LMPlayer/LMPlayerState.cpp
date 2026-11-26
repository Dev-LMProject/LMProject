#include "LMPlayer/LMPlayerState.h"

#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMCombat/LMCombatAttributeSet.h"
#include "LMCombat/LMAttackAbility.h"
#include "LMGameplayTags/LMGameplayTags.h"

/** PlayerState가 유지되는 동안 사용할 ASC와 전투 속성을 생성한다. */
ALMPlayerState::ALMPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<ULMAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 현재 싱글플레이 구성에 맞춰 ASC 복제를 사용하지 않는다.
	AbilitySystemComponent->SetIsReplicated(false);

	// ASC 소유자의 기본 SubObject로 만들어 기존 자동 등록 경로를 사용한다.
	CombatAttributes = CreateDefaultSubobject<ULMCombatAttributeSet>(TEXT("CombatAttributes"));
}

/** 외부 시스템이 GAS 인터페이스로 접근할 수 있도록 ASC를 반환한다. */
UAbilitySystemComponent* ALMPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

// ASC와 속성 등록이 끝난 시점에만 이벤트 구독을 연결한다.
void ALMPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SpecialChargeHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetSpecialChargeAttribute()).AddUObject(this, &ThisClass::HandleSpecialAttributeChanged);
	MaxSpecialChargeHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetMaxSpecialChargeAttribute()).AddUObject(this, &ThisClass::HandleSpecialAttributeChanged);
	SpecialHealthHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleSpecialAttributeChanged);
	SpecialTagHandle = AbilitySystemComponent->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::HandleSpecialTagChanged);
}

// 생존과 정상적인 속성 연결이 있을 때만 자원을 사용한다.
bool ALMPlayerState::IsAliveForSpecial() const
{
	return AbilitySystemComponent && CombatAttributes && FMath::IsFinite(CombatAttributes->GetHealth()) && CombatAttributes->GetHealth() > 0.0f && !AbilitySystemComponent->HasMatchingGameplayTag(LMGameplayTags::State_Dead);
}

// 해금 권한의 원본은 기존 게임플레이 태그로 유지한다.
bool ALMPlayerState::IsSpecialUnlocked() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(LMGameplayTags::Unlock_Attack_Special);
}

// 고정 적중 보상을 최대 충전량까지 더하며 잘못된 입력은 무시한다.
bool ALMPlayerState::AddSpecialCharge(float ChargeAmount)
{
	if (!FMath::IsFinite(ChargeAmount) || ChargeAmount <= 0.0f || !IsAliveForSpecial() || !IsSpecialUnlocked())
		return false;
	const float CurrentCharge = CombatAttributes->GetSpecialCharge();
	const float ChargeLimit = CombatAttributes->GetMaxSpecialCharge();
	const float AddedCharge = FMath::Min(ChargeAmount, FMath::Max(0.0f, ChargeLimit - CurrentCharge));
	if (AddedCharge <= 0.0f) return false;
	CombatAttributes->SetSpecialCharge(CurrentCharge + AddedCharge);
	return true;
}

// 패링 성공에 지정된 총 보상만 더한다.
bool ALMPlayerState::AddParryCharge()
{
	return AddSpecialCharge(ParryChargeGain);
}

// 거의 찬 상태를 만충으로 오인하여 무료 공격을 허용하지 않는다.
bool ALMPlayerState::HasFullSpecialCharge() const
{
	return IsAliveForSpecial() && IsSpecialUnlocked() && CombatAttributes->GetSpecialCharge() >= CombatAttributes->GetMaxSpecialCharge();
}

// 차감은 알림보다 먼저 확정되어 재진입이나 같은 입력의 중복 소비를 막는다.
bool ALMPlayerState::TrySpendSpecialCharge()
{
	if (!HasFullSpecialCharge()) return false;
	CombatAttributes->SetSpecialCharge(0.0f);
	return true;
}

// 기존 해금 또는 같은 시연의 재진입은 충전 보상을 반복하지 않는다.
bool ALMPlayerState::UnlockSpecialAttack()
{
	if (!IsAliveForSpecial()) return false;
	const bool bWasUnlocked = IsSpecialUnlocked();
	const bool bFillOnUnlock = !bWasUnlocked && !bUnlockChargeGiven;
	bUnlockChargeGiven = true;
	if (!bWasUnlocked)
		AbilitySystemComponent->AddLooseGameplayTag(LMGameplayTags::Unlock_Attack_Special);
	if (bFillOnUnlock)
		AddSpecialCharge(CombatAttributes->GetMaxSpecialCharge());
	return IsSpecialUnlocked();
}

// 각성방 완료 보상과 HUD 공개를 같은 1회 경계로 묶는다.
bool ALMPlayerState::ShowSpecialGauge()
{
	if (!IsAliveForSpecial() || !IsSpecialUnlocked()) return false;
	if (bSpecialGaugeShown) return true;
	bSpecialGaugeShown = true;
	AddSpecialCharge(CombatAttributes->GetMaxSpecialCharge());
	OnSpecialGaugeChanged.Broadcast();
	return true;
}

// UI가 만들어지는 시점을 특수공격 시연용 해금과 분리한다.
bool ALMPlayerState::ShouldShowSpecialGauge() const
{
	return bSpecialGaugeShown;
}

// 현재 실행 중인 콤보 창과 입력 제한을 함께 반영한다.
bool ALMPlayerState::CanPrepareSpecialAttack() const
{
	const ULMAttackAbility* PrepareAbility = SpecialPrepareAbility.Get();
	return HasFullSpecialCharge() && PrepareAbility && PrepareAbility->IsActive() && AbilitySystemComponent->GetAnimatingAbility() == PrepareAbility && !AbilitySystemComponent->HasMatchingGameplayTag(LMGameplayTags::State_Attack_Special) && !AbilitySystemComponent->HasMatchingGameplayTag(LMGameplayTags::State_Input_Blocked) && !AbilitySystemComponent->HasMatchingGameplayTag(LMGameplayTags::State_Stunned);
}

// 종료된 다른 GA가 새 콤보의 준비 창을 닫지 않도록 소유자를 비교한다.
void ALMPlayerState::SetSpecialPrepareWindow(ULMAttackAbility* SourceAbility, bool bOpen)
{
	if (!SourceAbility) return;
	if (bOpen && SourceAbility->IsActive() && SourceAbility->GetAbilitySystemComponentFromActorInfo() == AbilitySystemComponent)
		SpecialPrepareAbility = SourceAbility;
	else if (SpecialPrepareAbility.Get() == SourceAbility)
		SpecialPrepareAbility.Reset();
	OnSpecialGaugeChanged.Broadcast();
}

// 수치 변경 이벤트만 처리하며 매 프레임 게이지를 조회하지 않는다.
void ALMPlayerState::HandleSpecialAttributeChanged(const FOnAttributeChangeData& Change)
{
	if (Change.Attribute == ULMCombatAttributeSet::GetHealthAttribute() && Change.NewValue <= 0.0f)
	{
		SpecialPrepareAbility.Reset();
		CombatAttributes->SetSpecialCharge(0.0f);
	}
	OnSpecialGaugeChanged.Broadcast();
}

// 관련 태그만 HUD에 알려 불필요한 표시 갱신을 줄인다.
void ALMPlayerState::HandleSpecialTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (Tag == LMGameplayTags::State_Dead && NewCount > 0)
	{
		SpecialPrepareAbility.Reset();
		CombatAttributes->SetSpecialCharge(0.0f);
	}
	if (Tag == LMGameplayTags::Unlock_Attack_Special || Tag == LMGameplayTags::State_Dead || Tag == LMGameplayTags::State_Stunned || Tag == LMGameplayTags::State_Input_Blocked || Tag == LMGameplayTags::State_Attack_Special)
		OnSpecialGaugeChanged.Broadcast();
}

/** PlayerState의 플레이 종료 시 남은 입력과 능력 실행을 정리한다. */
void ALMPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetSpecialChargeAttribute()).Remove(SpecialChargeHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetMaxSpecialChargeAttribute()).Remove(MaxSpecialChargeHandle);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ULMCombatAttributeSet::GetHealthAttribute()).Remove(SpecialHealthHandle);
		AbilitySystemComponent->RegisterGenericGameplayTagEvent().Remove(SpecialTagHandle);
		SpecialPrepareAbility.Reset();
		OnSpecialGaugeChanged.Clear();
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
