// Fill out your copyright notice in the Description page of Project Settings.


#include "LMComponent/LMScanComponent.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMCamera/LMPostProcessCameraModifier.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

// Sets default values for this component's properties
ULMScanComponent::ULMScanComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled=false;
	// ...
}

bool ULMScanComponent::StartScan()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled() || !GetWorld() ||
		!PostProcessModifier.IsValid() ||
		!FMath::IsFinite(ScanRadius) || ScanRadius <= 0.0f ||
		ScanStencilValue < 1 || ScanStencilValue > 255)
	{
		return false;
	}

	StopScan();
	const TArray<AActor*> Targets = CollectScanTargets();
	if (!PostProcessModifier->StartScanPostProcess(ScanStencilValue))
	{
		return false;
	}
	for (AActor* Target : Targets)
	{
		HighlightActor(Target);
	}

	bIsScanning = true;
	SetComponentTickEnabled(true);
	return true;
}

bool ULMScanComponent::UpdateScanIntensity(float Alpha)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bIsScanning||
		!IsValid(OwnerPawn) ||
		!OwnerPawn->IsLocallyControlled() ||
		!PostProcessModifier.IsValid() ||
		!FMath::IsFinite(Alpha))
	{
		StopScan();
		return false;
	}
	PostProcessModifier->UpdateScanPostProcess(Alpha);
	return true;
}

void ULMScanComponent::StopScan()
{
	if (bIsScanning && PostProcessModifier.IsValid())
	{
		PostProcessModifier->StopScanPostProcess();
	}
	bIsScanning = false;
	RestoreHighlights();
}

bool ULMScanComponent::InitializeScan(ULMPostProcessCameraModifier* InPostProcessModifier)
{
	StopScan();
	PostProcessModifier = InPostProcessModifier;
	return PostProcessModifier.IsValid();
}


void ULMScanComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopScan();
	Super::EndPlay(EndPlayReason);
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

