#include "LMCombat/LMCombatAttributeSet.h"

#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMCombat/LMAttackData.h"
#include "LMPlayer/LMPlayerState.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMPlayer/LMPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

/** 새로 생성되는 전투 속성의 기본 체력과 임시 피해량을 설정한다. */
ULMCombatAttributeSet::ULMCombatAttributeSet()
{
	// 종류별 시작 수치는 ASC가 연결될 때 캐릭터 설정에서 적용한다.
	InitMaxHealth(100.0f);

	// ASC 재연결과 분리하여 새 객체의 초기 체력만 설정한다.
	InitHealth(GetMaxHealth());

	// 아직 전달받은 피해가 없으므로 임시 피해량은 0에서 시작한다.
	InitIncomingDamage(0.0f);
	InitMaxSpecialCharge(100.0f);
	InitSpecialCharge(0.0f);
}

// 입력 검사를 곱셈보다 먼저 수행하여 소수·과대 입력과 정수 넘침을 막는다.
bool ULMCombatAttributeSet::TryGetDamage(float BaseDamage, int32 DamagePercent, int32& OutDamage)
{
	OutDamage = 0;
	if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.0f || BaseDamage > 100000.0f || FMath::FloorToFloat(BaseDamage) != BaseDamage || DamagePercent < 100 || DamagePercent > 300)
		return false;
	const int64 ScaledDamage = (static_cast<int64>(BaseDamage) * DamagePercent + 50) / 100;
	OutDamage = static_cast<int32>(FMath::Min<int64>(ScaledDamage, 100000));
	return true;
}

// 새 몸체의 HP 알림이 발생하기 전에 누적 상태를 먼저 비운다.
void ULMCombatAttributeSet::StartNewLife(int32 NewMaxHealth)
{
	if (!ensureMsgf(NewMaxHealth >= 1 && NewMaxHealth <= 100000, TEXT("Start health must be between 1 and 100000.")))
		return;
	CriticalFailCount = 0;
	SetMaxHealth(static_cast<float>(NewMaxHealth));
	SetHealth(static_cast<float>(NewMaxHealth));
	SetIncomingDamage(0.0f);
}

// 잘못된 설정은 적용하지 않으며 같은 설정의 재연결은 누적을 보존한다.
bool ULMCombatAttributeSet::SetCriticalSettings(int32 _CriticalChance, int32 _CriticalStep, int32 _CriticalLimit, int32 _CriticalDamagePercent)
{
	if (_CriticalChance < 0 || _CriticalChance > 100 || _CriticalStep < 0 || _CriticalStep > 100 || _CriticalLimit < 1 || _CriticalLimit > 100 || _CriticalDamagePercent < 100 || _CriticalDamagePercent > 300)
		return false;
	if (CriticalChance != _CriticalChance || CriticalStep != _CriticalStep || CriticalLimit != _CriticalLimit || CriticalDamagePercent != _CriticalDamagePercent)
		CriticalFailCount = 0;
	CriticalChance = _CriticalChance;
	CriticalStep = _CriticalStep;
	CriticalLimit = _CriticalLimit;
	CriticalDamagePercent = _CriticalDamagePercent;
	return true;
}

// 확률 0의 비활성 규칙을 횟수 보장보다 먼저 적용한다.
int32 ULMCombatAttributeSet::GetCriticalChance() const
{
	if (CriticalChance == 0) return 0;
	if (CriticalFailCount >= CriticalLimit - 1) return 100;
	return static_cast<int32>(FMath::Min<int64>(100, CriticalChance + static_cast<int64>(CriticalFailCount) * CriticalStep));
}

// 확정과 비활성은 난수를 소비하지 않고 중간 확률에서만 한 번 뽑는다.
bool ULMCombatAttributeSet::RollCritical() const
{
	const int32 Chance = GetCriticalChance();
	return Chance >= 100 || (Chance > 0 && FMath::RandRange(0, 99) < Chance);
}

// 무효 타격은 호출부에서 제외하며 누적 상한을 보장 직전 횟수로 제한한다.
void ULMCombatAttributeSet::UpdateCriticalCount(bool bCritical)
{
	if (CriticalChance == 0) return;
	CriticalFailCount = bCritical ? 0 : FMath::Min(CriticalFailCount + 1, CriticalLimit - 1);
}

// GAS의 float 저장 경계에서 HP를 유효한 정수로 맞추고 잘못된 입력을 알린다.
void ULMCombatAttributeSet::ClampHealth(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute != GetHealthAttribute() && Attribute != GetMaxHealthAttribute())
		return;
	const bool bMaximum = Attribute == GetMaxHealthAttribute();
	const float LowerBound = bMaximum ? 1.0f : 0.0f;
	const float UpperBound = bMaximum ? 100000.0f : FMath::Clamp(GetMaxHealth(), 1.0f, 100000.0f);
	const float PreviousValue = bMaximum ? GetMaxHealth() : GetHealth();
	const float SafeValue = FMath::RoundToFloat(FMath::Clamp(FMath::IsFinite(NewValue) ? NewValue : PreviousValue, LowerBound, UpperBound));
	UE_CLOG(NewValue != SafeValue, LogTemp, Warning, TEXT("[LMCombat] %s: Invalid %s input %f, using integer %f."), *GetNameSafe(GetOwningActor()), *Attribute.GetName(), NewValue, SafeValue);
	NewValue = SafeValue;
}

// 외부 속성 효과와 공통 API가 같은 충전 범위를 사용한다.
void ULMCombatAttributeSet::ClampSpecialCharge(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetMaxSpecialChargeAttribute())
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(1.0f, NewValue) : GetMaxSpecialCharge();
	else if (Attribute == GetSpecialChargeAttribute())
		NewValue = FMath::Clamp(FMath::IsFinite(NewValue) ? NewValue : GetSpecialCharge(), 0.0f, GetMaxSpecialCharge());
}

// 현재값 변경 전에 잘못된 게이지 수치를 제한한다.
void ULMCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampHealth(Attribute, NewValue);
	ClampSpecialCharge(Attribute, NewValue);
}

// 기본값 변경에도 현재값과 동일한 제한을 적용한다.
void ULMCombatAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampHealth(Attribute, NewValue);
	ClampSpecialCharge(Attribute, NewValue);
}

// 최대값 감소 시 기존 충전이 상한을 초과한 상태로 남지 않게 한다.
void ULMCombatAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
		SetHealth(NewValue);
	if (Attribute == GetMaxSpecialChargeAttribute() && GetSpecialCharge() > NewValue)
		SetSpecialCharge(NewValue);
}

/** 전달받은 피해를 한 번 소비하고 실제 HP 감소 결과를 알린다. */
void ULMCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 다른 속성에 적용된 효과를 피해로 처리하지 않는다.
	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		return;
	}

	// 다음 피해에 이전 값이 누적되지 않도록 먼저 소비한다.
	const float RequestedDamage = GetIncomingDamage();
	SetIncomingDamage(0.0f);

	int32 DamageValue = 0;
	if (!TryGetDamage(RequestedDamage, 100, DamageValue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LMCombat] Invalid incoming integer damage: %f"), RequestedDamage);
		return;
	}
	if (DamageValue == 0) return;

	const float OldHealth = GetHealth();
	const float MaximumHealth = GetMaxHealth();

	// 초기화 또는 다른 변경 경로에서 잘못된 체력이 들어왔다면 계산하지 않는다.
	const bool bValidHealth = FMath::IsFinite(OldHealth) && FMath::IsFinite(MaximumHealth) && MaximumHealth >= 1.0f && MaximumHealth <= 100000.0f && OldHealth >= 0.0f && OldHealth <= MaximumHealth && FMath::FloorToFloat(OldHealth) == OldHealth && FMath::FloorToFloat(MaximumHealth) == MaximumHealth;

	if (!ensureMsgf(bValidHealth, TEXT("%s: Invalid Health or MaxHealth."), *GetNameSafe(Data.Target.GetAvatarActor())))
	{
		return;
	}

	if (OldHealth <= 0.0f) return;

	UAbilitySystemComponent* SourceASC = Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
	ULMCombatAttributeSet* SourceStats = SourceASC ? const_cast<ULMCombatAttributeSet*>(SourceASC->GetSet<ULMCombatAttributeSet>()) : nullptr;
	ALMBaseCharacter* SourceBody = SourceASC ? Cast<ALMBaseCharacter>(SourceASC->GetAvatarActor()) : nullptr;
	const ULMAttackData* HitData = Cast<ULMAttackData>(Data.EffectSpec.GetContext().GetSourceObject());
	const FGameplayTagContainer& HitTags = Data.EffectSpec.GetDynamicAssetTags();
	const bool bSceneDamage = HitTags.HasTagExact(LMGameplayTags::Damage_SceneHit) || HitTags.HasTagExact(LMGameplayTags::Damage_SceneFinish);
	bool bAllowCritical = SourceStats && SourceBody && HitData && HitData->bAllowCritical && !bSceneDamage && SourceStats->GetCriticalChance() > 0;

	// 장갑은 피해 숫자가 아니라 명시적인 참여 정책으로 치명타 누적을 제외한다.
	ALMBaseCharacter* DamageReceiver = Cast<ALMBaseCharacter>(Data.Target.GetAvatarActor());
	const float FinalDamage = DamageReceiver ? DamageReceiver->ChangeIncomingDamage(Data.EffectSpec, RequestedDamage, OldHealth, MaximumHealth, bAllowCritical) : RequestedDamage;
	int32 FinalDamageValue = 0;
	if (!ensureMsgf(TryGetDamage(FinalDamage, 100, FinalDamageValue), TEXT("%s: Invalid final integer damage."), *GetNameSafe(DamageReceiver)))
	{
		return;
	}

	const float NewHealth = static_cast<float>(FMath::Max(0, static_cast<int32>(OldHealth) - FinalDamageValue));
	const float ActualDamage = OldHealth - NewHealth;

	if (ActualDamage <= 0.0f)
	{
		// 원래 유효피해가 있었다면 보호 후 감소가 0이어도 각성 예약을 완료한다.
		if (IsValid(DamageReceiver))
			DamageReceiver->FinishCombatDamage();
		return;
	}

	// 위치·종류를 사망 반응 전에 복사하여 파괴된 몸체를 다시 조회하지 않는다.
	FLMDamageResult Result;
	Result.Source = SourceBody;
	Result.Target = DamageReceiver;
	Result.AttackData = HitData;
	Result.ActualDamage = static_cast<int32>(ActualDamage);
	Result.bAllowCritical = bAllowCritical;
	Result.bCritical = bAllowCritical && HitTags.HasTagExact(LMGameplayTags::Damage_Critical);
	Result.bHealthDepleted = NewHealth <= 0.0f;
	Result.bSceneDamage = bSceneDamage;
	Result.bPlayerAttack = Cast<ALMPlayerCharacter>(SourceBody) != nullptr;
	if (DamageReceiver)
	{
		Result.HitLocation = DamageReceiver->GetMesh() ? DamageReceiver->GetMesh()->Bounds.Origin : DamageReceiver->GetActorLocation();
		if (SourceBody)
			Result.HitDirection = (Result.HitLocation - SourceBody->GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	}
	if (const FHitResult* Contact = Data.EffectSpec.GetContext().GetHitResult(); Contact && Contact->GetActor() == DamageReceiver && !Contact->ImpactPoint.ContainsNaN())
	{
		Result.HitLocation = Contact->ImpactPoint;
		Result.HitDirection = (-Contact->ImpactNormal).GetSafeNormal(UE_SMALL_NUMBER, Result.HitDirection);
	}
	const bool bChargeHit = SourceBody && DamageReceiver && DamageReceiver->IsHostileTo(SourceBody) && HitData && !bSceneDamage;
	const float ChargeGain = bChargeHit ? HitData->SpecialChargeGain : 0.0f;
	TWeakObjectPtr<ALMPlayerState> SourcePlayer = SourceASC ? Cast<ALMPlayerState>(SourceASC->GetOwnerActor()) : nullptr;
	TWeakObjectPtr<ALMPlayerController> EffectPlayer = Cast<ALMPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));

	// SetHealth의 UI 알림이 재진입하더라도 다음 타격은 갱신된 누적을 사용한다.
	if (bAllowCritical)
		SourceStats->UpdateCriticalCount(Result.bCritical);
	SetHealth(NewHealth);

	// 장갑 피해 1도 해금 뒤에는 충전한다. 치명타 참여 여부로 보상을 제한하지 않는다.
	if (SourcePlayer.IsValid() && ChargeGain > 0.0f)
		SourcePlayer->AddSpecialCharge(ChargeGain);

	// HP 변경을 완료한 피해만 알린다. UI는 별도의 GAS 속성 변경 알림을 사용한다.
	OnDamageApplied.Broadcast(ActualDamage, Result.bHealthDepleted);

	// HP 및 기존 사망 수신자가 모두 끝난 뒤 캐릭터가 다음 틱 반응을 예약한다.
	if (IsValid(DamageReceiver) && !DamageReceiver->IsActorBeingDestroyed())
		DamageReceiver->FinishCombatDamage();
	if (EffectPlayer.IsValid())
		EffectPlayer->PlayHitEffect(Result);
}
