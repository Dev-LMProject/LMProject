#include "LMComponent/LMInteractorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "LMComponent/LMInteractionComponent.h"

ULMInteractorComponent::ULMInteractorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void ULMInteractorComponent::BeginPlay()
{
    bIsEndingPlay = false;
    Super::BeginPlay();
}

void ULMInteractorComponent::SetInteractionCollision(UPrimitiveComponent* InCollision)
{
    if (bIsEndingPlay)
    {
        return;
    }

    if (InCollision &&
        (!IsValid(InCollision) ||
         InCollision->GetOwner() != GetOwner() ||
         !InCollision->IsRegistered()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("%s: InteractionCollision must be a registered component owned by the same actor."),
            *GetNameSafe(GetOwner()));
        return;
    }

    const bool bCollisionChanged = InteractionCollision != InCollision;
    if (bCollisionChanged)
    {
        const TArray<TWeakObjectPtr<AActor>> PreviousTargets = InteractionTargets;
        InteractionCollision = nullptr;

        // 이전 센서의 역방향 기록도 정리해 콜리전 교체 후 해제 이벤트가 섞이지 않게 한다.
        for (const TWeakObjectPtr<AActor>& WeakTarget : PreviousTargets)
        {
            if (AActor* Target = WeakTarget.Get())
            {
                if (ULMInteractionComponent* Interaction =
                    Target->FindComponentByClass<ULMInteractionComponent>())
                {
                    Interaction->RefreshDetectedInteractors();
                }
            }
        }

        ClearInteractionTargets();
        if (bIsEndingPlay)
        {
            return;
        }
        InteractionCollision = InCollision;
    }

    if (!IsValid(InteractionCollision))
    {
        return;
    }

    UPrimitiveComponent* Collision = InteractionCollision.Get();
    if (bCollisionChanged)
    {
        // BP의 형태·크기·위치는 유지하고 감지용 충돌 설정만 맞춘다.
        FCollisionResponseContainer Responses(ECR_Ignore);
        Responses.SetResponse(ECC_GameTraceChannel2, ECR_Overlap); // LMInteractionSensor
        Collision->SetCollisionObjectType(ECC_GameTraceChannel1); // LMPlayerInteraction
        Collision->SetCollisionResponseToChannels(Responses);
        Collision->SetGenerateOverlapEvents(true);
        Collision->SetCanEverAffectNavigation(false);
        Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    Collision->UpdateOverlaps();

    if (bIsEndingPlay || !IsValid(Collision) || InteractionCollision != Collision)
    {
        return;
    }

    // 이미 겹친 뒤 연결되면 BeginOverlap이 다시 발생하지 않을 수 있다.
    TArray<UPrimitiveComponent*> OverlappingComponents;
    Collision->GetOverlappingComponents(OverlappingComponents);

    for (UPrimitiveComponent* OtherComp : OverlappingComponents)
    {
        if (bIsEndingPlay || InteractionCollision != Collision)
        {
            return;
        }

        AActor* OtherActor = IsValid(OtherComp) ? OtherComp->GetOwner() : nullptr;
        if (!IsValid(OtherActor) || OtherActor == GetOwner())
        {
            continue;
        }

        ULMInteractionComponent* Interaction =
            OtherActor->FindComponentByClass<ULMInteractionComponent>();

        if (IsValid(Interaction) && Interaction->GetDetectionCollision() == OtherComp)
        {
            Interaction->RefreshDetectedInteractors();
        }
    }
}

UPrimitiveComponent* ULMInteractorComponent::GetInteractionCollision() const
{
    return IsValid(InteractionCollision) ? InteractionCollision.Get() : nullptr;
}

void ULMInteractorComponent::RegisterInteractionTarget(AActor* TargetActor)
{
	if (bIsEndingPlay || //컴포넌트 종료중이거나
		!IsValid(TargetActor) || //TargetActor가 유효하지 않거나
		TargetActor == GetOwner() || //TargetActor가 자기자신이거나
		!IsValid(TargetActor->FindComponentByClass<ULMInteractionComponent>())) //TargetActor가 InteractionComponent가 없을때 등록 X
	{
		return;
	}

	const TWeakObjectPtr<AActor> Target(TargetActor);

	if (InteractionTargets.Contains(Target))
	{
		return;
	}

	InteractionTargets.Add(Target);
	InteractionTargetsChanged.Broadcast();
}

void ULMInteractorComponent::UnregisterInteractionTarget(AActor* TargetActor)
{
	const int32 RemovedCount = InteractionTargets.RemoveAll(
		[TargetActor](const TWeakObjectPtr<AActor>& Target)
		{
			return !Target.IsValid() || Target.Get() == TargetActor;
		});

	if (RemovedCount > 0)
	{
		InteractionTargetsChanged.Broadcast();
	}
}

TArray<AActor*> ULMInteractorComponent::GetInteractionTargets() const
{
	TArray<AActor*> Targets;
	Targets.Reserve(InteractionTargets.Num());
	for (const TWeakObjectPtr<AActor>& WeakTarget : InteractionTargets)
	{
		if (AActor* Target = WeakTarget.Get())
		{
			Targets.Add(Target);
		}
	}
	return Targets;
}

void ULMInteractorComponent::ClearInteractionTargets()
{
    if (!InteractionTargets.IsEmpty())
    {
        InteractionTargets.Reset();
        InteractionTargetsChanged.Broadcast();
    }
}

void ULMInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bIsEndingPlay = true;
    InteractionCollision = nullptr;
    ClearInteractionTargets();
    Super::EndPlay(EndPlayReason);
}
