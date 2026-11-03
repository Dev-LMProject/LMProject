#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "LMPostProcessCameraModifier.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

/** 호출자가 전달한 효과 상태를 로컬 플레이어의 화면에 합성한다. */
UCLASS()
class LUMIA_API ULMPostProcessCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	ULMPostProcessCameraModifier();

	UFUNCTION(BlueprintCallable, Category = "LM|PostProcess|Scan")
	bool StartScanPostProcess(int32 StencilValue);
	
	/** 전달받은 표시 강도(0~1)를 스캔 머티리얼에 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "LM|PostProcess|Scan")
	void UpdateScanPostProcess(float ScanAlpha);

	UFUNCTION(BlueprintCallable, Category = "LM|PostProcess|Scan")
	void StopScanPostProcess();

protected:
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight,
		FPostProcessSettings& PostProcessSettings) override;

	UPROPERTY(EditDefaultsOnly, Category = "LM|PostProcess|Scan")
	TObjectPtr<UMaterialInterface> ScanMaterial;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScanMID;

	UPROPERTY(Transient)
	FPostProcessSettings ActivePostProcessSettings;

	bool bScanPostProcessActive = false;
};
