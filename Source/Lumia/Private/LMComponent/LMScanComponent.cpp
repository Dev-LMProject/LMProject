// Fill out your copyright notice in the Description page of Project Settings.


#include "LMComponent/LMScanComponent.h"
#include "LMComponent/LMInteractionComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

// Sets default values for this component's properties
ULMScanComponent::ULMScanComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled=false;
	// ...
}

void ULMScanComponent::StartScan()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled() || !GetWorld() ||
		!ScanCamera.IsValid() || !IsValid(ScanMID.Get()) ||
		!FMath::IsFinite(ScanRadius) || ScanRadius <= 0.0f ||
		!FMath::IsFinite(ScanDuration) || ScanDuration <= 0.0f ||
		ScanStencilValue < 1 || ScanStencilValue > 255)
	{
		return;
	}

	StopScan();
	for (AActor* Target : CollectScanTargets())
	{
		HighlightActor(Target);
	}

	ScanMID->SetScalarParameterValue(TEXT("ScanStencilValue"), static_cast<float>(ScanStencilValue));
	ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), 1.0f);
	ScanCamera->AddOrUpdateBlendable(ScanMID.Get(), 1.0f);
	bIsScanning = true;
	SetComponentTickEnabled(true);
}

void ULMScanComponent::StopScan()
{
	bIsScanning = false;
	SetComponentTickEnabled(false);
	if (IsValid(ScanMID.Get()))
	{
		ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), 0.0f);
		if (ScanCamera.IsValid())
		{
			ScanCamera->RemoveBlendable(ScanMID.Get());
		}
	}
	RestoreHighlights();
	ElapsedTime = 0.0f;
}

bool ULMScanComponent::InitializeScan(UCameraComponent* InCamera)
{
	StopScan();
	ScanCamera.Reset();
	ScanMID = nullptr;
	const UMaterial* Material = IsValid(ScanMaterial.Get()) ? ScanMaterial->GetMaterial() : nullptr;
	if (!IsValid(InCamera) || !IsValid(Material) || Material->MaterialDomain != MD_PostProcess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Scan requires a camera and a post-process material or material instance."));
		return false;
	}
	
	ScanCamera = InCamera;
	ScanMID = UMaterialInstanceDynamic::Create(ScanMaterial.Get(), this);
	if (!IsValid(ScanMID.Get()))
	{
		return false;
	}

	ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), 0.0f);
	ScanMID->SetScalarParameterValue( TEXT("ScanStencilValue"),static_cast<float>(ScanStencilValue));
	return true;
}


void ULMScanComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopScan();
	Super::EndPlay(EndPlayReason);
}


// Called every frame
void ULMScanComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsScanning)
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled() ||
		!ScanCamera.IsValid() || !IsValid(ScanMID.Get()))
	{
		StopScan();
		return;
	}

	ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(1.0f - ElapsedTime / ScanDuration, 0.0f, 1.0f);
	ScanMID->SetScalarParameterValue(TEXT("ScanAlpha"), Alpha);
	if (ElapsedTime >= ScanDuration)
	{
		StopScan();
	}
}

TArray<AActor*> ULMScanComponent::CollectScanTargets() const
{
	TArray<AActor*> Targets;

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	
	if (!World || !IsValid(OwnerActor) ||!FMath::IsFinite(ScanRadius) || ScanRadius <= 0.0f)
	{
		return Targets;
	}
	
	const FVector ScanOrigin = OwnerActor->GetActorLocation();
	
	
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LMScan),false,OwnerActor);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps,ScanOrigin,FQuat::Identity,ObjectParams,FCollisionShape::MakeSphere(ScanRadius),QueryParams);
	
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();

		if (!IsValid(Target) || Target == OwnerActor)
		{
			continue;
		}

		const ULMInteractionComponent* Interaction = Target->FindComponentByClass<ULMInteractionComponent>();

		if (!Interaction || Overlap.GetComponent() != Interaction->GetDetectionCollision())
		{
			continue;
		}

		Targets.AddUnique(Target);
	}

	return Targets;
}

void ULMScanComponent::HighlightActor(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	Target->GetComponents<UMeshComponent>(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) ||
			(!Mesh->IsA<UStaticMeshComponent>() &&
			 !Mesh->IsA<USkeletalMeshComponent>()))
		{
			continue;
		}

		// shortcut: 스캔 중 CustomDepth 단독 사용, 동시 효과가 생기면 소유권 처리 추가.
		FLMScanMeshState& State = SavedMeshStates.AddDefaulted_GetRef();

		State.Mesh = Mesh;
		State.bRenderCustomDepth = Mesh->bRenderCustomDepth;
		State.StencilValue = Mesh->CustomDepthStencilValue;
		State.WriteMask = Mesh->CustomDepthStencilWriteMask;

		Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		Mesh->SetCustomDepthStencilValue(ScanStencilValue);
		Mesh->SetRenderCustomDepth(true);
	}
}

void ULMScanComponent::RestoreHighlights()
{
	for (const FLMScanMeshState& State : SavedMeshStates)
	{
		if (UMeshComponent* Mesh = State.Mesh.Get())
		{
			Mesh->SetCustomDepthStencilValue(State.StencilValue);
			Mesh->SetCustomDepthStencilWriteMask(State.WriteMask);
			Mesh->SetRenderCustomDepth(State.bRenderCustomDepth);
		}
	}
	SavedMeshStates.Reset();
}

