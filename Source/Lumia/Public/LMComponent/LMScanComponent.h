// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "LMScanComponent.generated.h"

class UMeshComponent;
class ULMPostProcessCameraModifier;

//스캔된 객체의 태두리를 따기 위한 메시 정보 구조체
struct FLMScanMeshState
{
	TWeakObjectPtr<UMeshComponent> Mesh;

	bool bRenderCustomDepth = false;
	int32 StencilValue = 0;

	ERendererStencilMask WriteMask =
		ERendererStencilMask::ERSM_Default;
};
UCLASS(ClassGroup=(Scan), meta=(BlueprintSpawnableComponent))
class LUMIA_API ULMScanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULMScanComponent();
	// Called every frame
	UFUNCTION(BlueprintCallable,Category="Scan")
	bool StartScan();
	UFUNCTION(BlueprintCallable, Category = "Scan")
	bool UpdateScanIntensity(float Alpha);
	UFUNCTION(BlueprintCallable,Category="Scan")
	void StopScan();
	bool InitializeScan(ULMPostProcessCameraModifier* InPostProcessModifier);
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category = "Scan", meta = (ClampMin = "1.0", Units = "cm"))
	float ScanRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Scan", meta = (ClampMin = "1", ClampMax = "255"))
	int32 ScanStencilValue = 1;

private:
	bool bIsScanning = false;

	TArray<AActor*> CollectScanTargets() const;
	TArray<FLMScanMeshState> SavedMeshStates;

	void HighlightActor(AActor* Target);
	void RestoreHighlights();
	
	TWeakObjectPtr<ULMPostProcessCameraModifier> PostProcessModifier;
};
