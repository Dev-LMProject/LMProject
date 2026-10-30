#include "LMCamera/LMPostProcessCameraModifier.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ULMPostProcessCameraModifier::ULMPostProcessCameraModifier()
{
	bDisabled = true;

	// 기존 연결을 기본값으로 유지하며 Modifier BP에서 머테리얼이나 MI로 교체할 수 있다.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultScanMaterial(
		TEXT("/Game/Material/M_LM_ScanHighlight.M_LM_ScanHighlight"));
	ScanMaterial = DefaultScanMaterial.Object;
}

bool ULMPostProcessCameraModifier::StartScanPostProcess(int32 StencilValue)
{
	const APlayerController* PlayerController = IsValid(CameraOwner.Get())
		? CameraOwner->GetOwningPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() ||
		StencilValue < 1 || StencilValue > 255)
	{
		return false;
	}

	const UMaterial* Material = IsValid(ScanMaterial.Get()) ? ScanMaterial->GetMaterial() : nullptr;
	if (!IsValid(Material) || Material->MaterialDomain != MD_PostProcess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Assign a post-process material or material instance to ScanMaterial."));
		return false;
	}

	if (!IsValid(ScanMID.Get()))
	{
		ScanMID = UMaterialInstanceDynamic::Create(ScanMaterial.Get(), this);
	}
	if (!IsValid(ScanMID.Get()))
	{
		return false;
	}

	ScanMID->SetScalarParameterValue(TEXT("ScanStencilValue"), static_cast<float>(StencilValue));
	ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), 1.0f);
	ActivePostProcessSettings.AddBlendable(ScanMID.Get(), 1.0f);
	bScanPostProcessActive = true;
	EnableModifier();
	return true;
}

void ULMPostProcessCameraModifier::UpdateScanPostProcess(float Progress)
{
	if (!bScanPostProcessActive || !IsValid(ScanMID.Get()) || !FMath::IsFinite(Progress))
	{
		return;
	}

	ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), 1.0f - FMath::Clamp(Progress, 0.0f, 1.0f));
}

void ULMPostProcessCameraModifier::StopScanPostProcess()
{
	bScanPostProcessActive = false;
	if (IsValid(ScanMID.Get()))
	{
		ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), 0.0f);
		ActivePostProcessSettings.RemoveBlendable(ScanMID.Get());
	}

	// 스캔 종료가 다른 효과까지 중단하지 않도록 남은 효과를 확인한다.
	if (ActivePostProcessSettings.WeightedBlendables.Array.IsEmpty())
	{
		DisableModifier(true);
	}
}

void ULMPostProcessCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight,
	FPostProcessSettings& PostProcessSettings)
{
	Super::ModifyPostProcess(DeltaTime, PostProcessBlendWeight, PostProcessSettings);
	PostProcessSettings = ActivePostProcessSettings;
	PostProcessBlendWeight = ActivePostProcessSettings.WeightedBlendables.Array.IsEmpty() ? 0.0f : 1.0f;
}
