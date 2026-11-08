#include "LMComponent/LMInteractionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "LMComponent/LMInteractorComponent.h"
//scope 종료 시 가드를 자동 복원
#include "Templates/UnrealTemplate.h"

ULMInteractionComponent::ULMInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    InteractionId = FGuid::NewGuid();
    bIsBusy = false;
}

void ULMInteractionComponent::BeginPlay()
{
    bIsEndingPlay = false;
    Super::BeginPlay();

    if (IsValid(DetectionCollision))
    {
        SetDetectionCollision(DetectionCollision.Get());
    }
}

void ULMInteractionComponent::SetDetectionCollision(UPrimitiveComponent* InCollision)
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
            TEXT("%s: DetectionCollision must be a registered component owned by the same actor."),
            *GetNameSafe(GetOwner()));
        return;
    }

    const bool bCollisionChanged = DetectionCollision != InCollision;
    if (bCollisionChanged)
    {
        ClearDetection();
        if (bIsEndingPlay)
        {
            return;
        }
        DetectionCollision = InCollision;
    }

    if (!IsValid(DetectionCollision))
    {
        return;
    }

    UPrimitiveComponent* Collision = DetectionCollision.Get();
    if (bCollisionChanged)
    {
        FCollisionResponseContainer Responses(ECR_Ignore);
        Responses.SetResponse(ECC_GameTraceChannel1, ECR_Overlap); // LMPlayerInteraction
        Collision->SetCollisionObjectType(ECC_GameTraceChannel2); // LMInteractionSensor
        Collision->SetCollisionResponseToChannels(Responses);
        Collision->SetGenerateOverlapEvents(true);
        Collision->SetCanEverAffectNavigation(false);
        Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    if (bIsEndingPlay || !IsValid(Collision) || DetectionCollision != Collision)
    {
        return;
    }

    Collision->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &ThisClass::OnDetectionBeginOverlap);
    Collision->OnComponentEndOverlap.AddUniqueDynamic(
        this, &ThisClass::OnDetectionEndOverlap);

    Collision->UpdateOverlaps();
    RefreshDetectedInteractor();
}

UPrimitiveComponent* ULMInteractionComponent::GetDetectionCollision() const
{
    return IsValid(DetectionCollision) ? DetectionCollision.Get() : nullptr;
}

void ULMInteractionComponent::RefreshDetectedInteractor()
{
    if (bIsEndingPlay)
    {
        return;
    }

    ULMInteractorComponent* CurrentInteractor = nullptr;

    if (IsValid(DetectionCollision) &&
        DetectionCollision->IsRegistered() &&
        DetectionCollision->IsQueryCollisionEnabled() &&
        DetectionCollision->GetGenerateOverlapEvents())
    {
        TArray<UPrimitiveComponent*> OverlappingComponents;
        DetectionCollision->GetOverlappingComponents(OverlappingComponents);

        for (UPrimitiveComponent* OtherComp : OverlappingComponents)
        {
            AActor* OtherActor = IsValid(OtherComp) ? OtherComp->GetOwner() : nullptr;
            if (!IsValid(OtherActor) || OtherActor == GetOwner() ||
                !OtherComp->IsQueryCollisionEnabled() ||
                !OtherComp->GetGenerateOverlapEvents())
            {
                continue;
            }

            ULMInteractorComponent* Interactor =
                OtherActor->FindComponentByClass<ULMInteractorComponent>();

            if (IsValid(Interactor) && OtherComp == Interactor->GetInteractionCollision())
            {
                CurrentInteractor = Interactor;
                break;
            }
        }
    }

    const TWeakObjectPtr<ULMInteractorComponent> PreviousInteractor = DetectedInteractor;

    // Broadcast 중 종료 처리가 재진입해도 정리할 수 있도록 기록부터 갱신한다.
    DetectedInteractor = CurrentInteractor;

    if (PreviousInteractor != DetectedInteractor)
    {
        if (ULMInteractorComponent* Interactor = PreviousInteractor.Get())
        {
            Interactor->UnregisterInteractionTarget(GetOwner());
        }
    }

    if (!bIsEndingPlay && DetectedInteractor.Get() == CurrentInteractor)
    {
        if (ULMInteractorComponent* Interactor = DetectedInteractor.Get())
        {
            Interactor->RegisterInteractionTarget(GetOwner());
        }
    }
}

void ULMInteractionComponent::OnDetectionBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OverlappedComponent == DetectionCollision)
    {
        RefreshDetectedInteractor();
    }
}

void ULMInteractionComponent::OnDetectionEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (OverlappedComponent == DetectionCollision)
    {
        // 다른 Body가 여전히 겹치면 현재 겹침 목록에 남아 있어 해제되지 않는다.
        RefreshDetectedInteractor();
    }
}

void ULMInteractionComponent::ClearDetection()
{
    if (IsValid(DetectionCollision))
    {
        DetectionCollision->OnComponentBeginOverlap.RemoveDynamic(
            this, &ThisClass::OnDetectionBeginOverlap);
        DetectionCollision->OnComponentEndOverlap.RemoveDynamic(
            this, &ThisClass::OnDetectionEndOverlap);
    }
    DetectionCollision = nullptr;

    const TWeakObjectPtr<ULMInteractorComponent> PreviousInteractor = DetectedInteractor;
    DetectedInteractor.Reset();

    if (ULMInteractorComponent* Interactor = PreviousInteractor.Get())
    {
        Interactor->UnregisterInteractionTarget(GetOwner());
    }
}

void ULMInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bIsEndingPlay = true;
    ClearDetection();
    Super::EndPlay(EndPlayReason);
}

bool ULMInteractionComponent::MatchesInteractionTags(const FGameplayTagContainer& ActionTags) const
{
    return ActionTags.IsValid() && HasTagContainer.HasAny(ActionTags);
}

void ULMInteractionComponent::SetInteractionTags(FGameplayTagContainer InTags)
{
    if (bIsEndingPlay || HasTagContainer == InTags)
    {
        return;
    }

    HasTagContainer = MoveTemp(InTags);
    InteractionTagsChanged.Broadcast();
}

//자신이 요청자와 상호작용이 가능한지
FLMInteractionResult ULMInteractionComponent::EvaluateInteraction(AActor* RequestActor, const FGameplayTagContainer& ActionTags, bool bIgnoreDispatchGuard)
{
	FLMInteractionResult R;
    R.InteractionId = InteractionId;
    AActor* TargetActor = GetOwner();
    
    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }
    if (!ActionTags.IsValid())
    {
        R.Failure = EInteractionFailure::InvalidActionTags;
        return R;
    }
    if (!TargetActor->GetClass()->ImplementsInterface(ULMInteractable::StaticClass()))
    {
        R.Failure = EInteractionFailure::NotHasInterface;
        return R;
    }
    if (bIsBusy || (!bIgnoreDispatchGuard && bIsDispatchingRequest))
    {
        R.Failure = EInteractionFailure::IsBusy;
        return R;
    }

    // 액터 BP는 여기서 상태만 읽으며, 새로운 요청이나 상태 변경을 실행하지 않는다.
    const bool bReserved = Execute_IsInteractionBusy(TargetActor);
    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || GetOwner() != TargetActor || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }

    if (bIsBusy || (!bIgnoreDispatchGuard && bIsDispatchingRequest) || bReserved)
    {
        R.Failure = EInteractionFailure::IsBusy;
        return R;
    }

    if (!MatchesInteractionTags(ActionTags))
    {
        R.Failure = EInteractionFailure::NotHasAllRequireTags;
        return R;
    }

    R.bCanInteraction = true;
    return R;
}

//대상에게 인터렉션 요청->대상이 요청자와 인터렉션가능한지 확인->가능한경우 인터페이스로 구현된 interact 실행
FLMInteractionResult ULMInteractionComponent::RequestInteraction(AActor* RequestActor, FGameplayTagContainer& ActionTags)
{
    FLMInteractionResult R = EvaluateInteraction(RequestActor, ActionTags);
    if (!R.bCanInteraction)
    {
        return R;
    }
	AActor* TargetActor = GetOwner();
    
    //Execute_HandleInteractionRequest시 BP가 실행하는 다른 로직에서 같은 액터의 RequestInteraction()을 다시 호출하는것을 막기 위해 임시로 TRUE로 표시
    TGuardValue<bool> DispatchGuard(bIsDispatchingRequest, true);
    const ELMInteractionRequestHandling Handling = Execute_HandleInteractionRequest(TargetActor, RequestActor);

    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || GetOwner() != TargetActor || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.bCanInteraction = false;
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }
    switch (Handling)
    {
    case ELMInteractionRequestHandling::Accepted:
        return R;
    case ELMInteractionRequestHandling::PassThrough:
        break;
    case ELMInteractionRequestHandling::Rejected:
    default:
        R.Failure = EInteractionFailure::ExecutionFailed;
        return R;
    }
    // Handler가 태그·일반 Busy·예약을 바꿨을 수 있으므로 다시 판정한다.
    // 현재 요청이 세운 DispatchGuard만 무시하고 나머지 조건은 모두 검사한다.
    R = EvaluateInteraction(RequestActor, ActionTags, true);
    if (!R.bCanInteraction)
    {
        return R;
    }

    bIsBusy = true;
    const bool bStarted = Execute_Interact(TargetActor, RequestActor);
    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.bCanInteraction = false;
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }

    if (!bStarted)
    {
        bIsBusy = false;
        R.Failure = EInteractionFailure::ExecutionFailed;
    }

    return R;
}

bool ULMInteractionComponent::GetIsBusy() const
{
    if (bIsBusy || bIsDispatchingRequest || bIsEndingPlay || IsBeingDestroyed())
    {
        return true;
    }

    AActor* TargetActor = GetOwner();
    return IsValid(TargetActor)
        && TargetActor->GetClass()->ImplementsInterface(ULMInteractable::StaticClass())
        && ILMInteractable::Execute_IsInteractionBusy(TargetActor);
}



