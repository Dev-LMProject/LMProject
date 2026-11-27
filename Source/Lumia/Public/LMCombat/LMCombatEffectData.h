#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LMCombatEffectData.generated.h"

class UNiagaraSystem;
class UCameraShakeBase;

// 전투 시작 시 함께 준비할 공용 연출만 묶으며 개체별 전투 상태를 보관하지 않는다.
UCLASS(BlueprintType)
class LUMIA_API ULMCombatEffectData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 애셋 이름과 고정 타입으로 스캔·쿠킹·비동기 로드에 같은 ID를 사용한다.
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Effects", meta = (AssetBundles = "Combat"))
	TSoftObjectPtr<UNiagaraSystem> MonsterStrike;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Effects", meta = (AssetBundles = "Combat"))
	TSoftObjectPtr<UNiagaraSystem> NormalHit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Effects", meta = (AssetBundles = "Combat"))
	TSoftObjectPtr<UNiagaraSystem> CriticalHit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Effects", meta = (AssetBundles = "Combat"))
	TSoftClassPtr<UCameraShakeBase> CriticalShake;

	// 프로젝트 원본 효과의 크기와 색을 재생할 때마다 다시 설정한다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Effects", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float NormalHitScale = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Effects", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float CriticalHitScale = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Effects", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float MonsterStrikeScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Effects")
	FLinearColor HitColor = FLinearColor(1.0f, 0.65f, 0.2f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "LM|Effects")
	FLinearColor StrikeColor = FLinearColor(1.0f, 0.3f, 0.05f, 1.0f);
};
