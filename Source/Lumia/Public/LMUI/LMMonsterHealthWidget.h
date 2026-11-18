#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMMonsterHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** 몬스터의 실제 체력을 전달받아 표시만 담당한다. */
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMMonsterHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 현재 체력과 최대 체력으로 HP바와 체력 숫자를 갱신한다. */
	void UpdateHealth(float CurrentHealth, float MaxHealth);

private:
	/** 위젯 BP의 같은 이름을 가진 Progress Bar와 연결한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	/** 위젯 BP의 같은 이름을 가진 Text와 연결한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;
};