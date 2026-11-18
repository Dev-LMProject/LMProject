#include "LMUI/LMMonsterHealthWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

/** 실제 체력을 수정하지 않고 표시용 비율과 숫자만 계산한다. */
void ULMMonsterHealthWidget::UpdateHealth(float CurrentHealth, float MaxHealth)
{
	// 필수 BP 연결과 숫자를 확인하여 잘못된 설정을 조기에 드러낸다.
	if (!ensureMsgf(IsValid(HealthBar.Get()) && IsValid(HealthText.Get()), TEXT("%s: Check HealthBar/HealthText bindings."), *GetName()))
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

	// 소수 피해도 확인하되 화면에 표시하는 숫자만 최대 한 자리로 반올림한다.
	FNumberFormattingOptions Format;
	Format.SetMinimumFractionalDigits(0);
	Format.SetMaximumFractionalDigits(1);

	HealthText->SetText(FText::Format(NSLOCTEXT("LMCombat", "MonsterHealthValue", "{0} / {1}"), FText::AsNumber(DisplayHealth, &Format), FText::AsNumber(MaxHealth, &Format)));
}