#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMMonsterHealthWidget.generated.h"

class UProgressBar;

/** 몬스터의 실제 체력을 전달받아 표시만 담당한다. */
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMMonsterHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 현재 체력의 비율만 표시하며 숫자를 노출하지 않는다.
	void UpdateHealth(float CurrentHealth, float MaxHealth);

private:
	/** 위젯 BP의 같은 이름을 가진 Progress Bar와 연결한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

};
