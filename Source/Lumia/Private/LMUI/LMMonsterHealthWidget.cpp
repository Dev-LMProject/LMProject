#include "LMUI/LMMonsterHealthWidget.h"

#include "Components/ProgressBar.h"

// 실제 체력을 수정하지 않고 숫자 없는 HP바의 비율만 계산한다.
void ULMMonsterHealthWidget::UpdateHealth(float CurrentHealth, float MaxHealth)
{
	// 필수 BP 연결과 숫자를 확인하여 잘못된 설정을 조기에 드러낸다.
	if (!ensureMsgf(IsValid(HealthBar.Get()), TEXT("%s: Check HealthBar binding."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(FMath::IsFinite(CurrentHealth) && FMath::IsFinite(MaxHealth) && MaxHealth >= 0.0f, TEXT("%s: Invalid health values."), *GetName()))
	{
		return;
	}

	// 최대 체력 0을 나누지 않으며 표시용 보정은 원본 Attribute에 반영하지 않는다.
	const float DisplayHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
	HealthBar->SetPercent(MaxHealth > 0.0f ? DisplayHealth / MaxHealth : 0.0f);

}
