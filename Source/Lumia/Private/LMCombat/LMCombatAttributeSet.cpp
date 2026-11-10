#include "LMCombat/LMCombatAttributeSet.h"

#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"

/** 새로 생성되는 전투 속성의 기본 체력과 임시 피해량을 설정한다. */
ULMCombatAttributeSet::ULMCombatAttributeSet()
{
	// 첫 구현 검증에 사용할 최대 체력이다.
	InitMaxHealth(100.0f);

	// ASC 재연결과 분리하여 새 객체의 초기 체력만 설정한다.
	InitHealth(GetMaxHealth());

	// 아직 전달받은 피해가 없으므로 임시 피해량은 0에서 시작한다.
	InitIncomingDamage(0.0f);
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

	// 음수 피해로 회복되거나 잘못된 수치가 HP에 들어가는 것을 막는다.
	if (!FMath::IsFinite(RequestedDamage) || RequestedDamage <= 0.0f)
	{
		return;
	}

	const float OldHealth = GetHealth();
	const float MaximumHealth = GetMaxHealth();

	// 초기화 또는 다른 변경 경로에서 잘못된 체력이 들어왔다면 계산하지 않는다.
	const bool bValidHealth = FMath::IsFinite(OldHealth) && FMath::IsFinite(MaximumHealth) && MaximumHealth >= 0.0f && OldHealth >= 0.0f && OldHealth <= MaximumHealth;

	if (!ensureMsgf(bValidHealth, TEXT("%s: Invalid Health or MaxHealth."), *GetNameSafe(Data.Target.GetAvatarActor())))
	{
		return;
	}

	// 남은 HP보다 큰 피해를 받아도 체력이 음수가 되지 않도록 제한한다.
	const float NewHealth = FMath::Clamp(OldHealth - RequestedDamage, 0.0f, MaximumHealth);
	const float ActualDamage = OldHealth - NewHealth;

	if (ActualDamage <= 0.0f)
	{
		return;
	}

	// 실제 감소량과 소진 여부를 확정한 뒤 HP를 한 번 변경한다.
	const bool bHealthDepleted = NewHealth <= 0.0f;
	SetHealth(NewHealth);

	UE_LOG(LogTemp, Display, TEXT("[LMDamage] %s Requested=%.1f Actual=%.1f Health=%.1f->%.1f Depleted=%s"), *GetNameSafe(Data.Target.GetAvatarActor()), RequestedDamage, ActualDamage, OldHealth, NewHealth, bHealthDepleted ? TEXT("true") : TEXT("false"));

	// HP 변경을 완료한 피해만 알린다. UI는 별도의 GAS 속성 변경 알림을 사용한다.
	OnDamageApplied.Broadcast(ActualDamage, bHealthDepleted);
}