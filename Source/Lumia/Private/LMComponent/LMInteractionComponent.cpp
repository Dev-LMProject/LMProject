#include "LMComponent/LMInteractionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "LMComponent/LMInteractorComponent.h"
#include "LMPlayer/LMPlayerController.h"
#include "LMUI/LMLoreData.h"
//scope 종료 시 가드를 자동 복원
#include "Templates/UnrealTemplate.h"

ULMInteractionComponent::ULMInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    InteractionId = FGuid::NewGuid();
    bIsBusy = false;
    
    // 개별 이름을 지정하기 전에도 목록에 읽을 수 있는 문구를 표시한다.
    InteractionName = NSLOCTEXT("LMInteraction", "DefaultInteractionName", "상호작용");
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

// 소유한 로어 창과 감지 연결을 정리한 뒤 컴포넌트를 종료한다.
void ULMInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bIsEndingPlay = true;
	// 대상이 사라져도 이 대상이 연 화면과 일시정지는 남기지 않는다.
	if (ALMPlayerController* ClosingController = LoreController.Get())
		ClosingController->CloseLoreFor(this);
	LoreController.Reset();
    ClearDetection();
    Super::EndPlay(EndPlayReason);
}

bool ULMInteractionComponent::MatchesInteractionTags(const FGameplayTagContainer& ActionTags) const
{
    return ActionTags.IsValid() && HasTagContainer.HasAny(ActionTags);
}

// 장치의 표시 설정을 읽어 스캔 색상과 상호작용 가능 상태를 독립적으로 유지한다.
bool ULMInteractionComponent::IsPromptHidden() const
{
    static const FName HidePromptTag(TEXT("LM.HideInteractionPrompt"));
    return ComponentHasTag(HidePromptTag);
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
    const bool bHasInterface = TargetActor->GetClass()->ImplementsInterface(ULMInteractable::StaticClass());
    // 로어가 지정된 대상은 별도 액터 인터페이스 없이도 기존 감지와 태그를 사용할 수 있다.
    if (!bHasInterface && !IsValid(LoreData))
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
    const bool bReserved = bHasInterface && Execute_IsInteractionBusy(TargetActor);
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

// 사전 요청을 처리한 뒤 로어 창 또는 기존 인터페이스 상호작용을 실행한다.
FLMInteractionResult ULMInteractionComponent::RequestInteraction(AActor* RequestActor, FGameplayTagContainer& ActionTags)
{
    FLMInteractionResult R = EvaluateInteraction(RequestActor, ActionTags);
    if (!R.bCanInteraction)
    {
        return R;
    }
	AActor* TargetActor = GetOwner();
    
	// BP 사전 처리나 로어 화면 생성 중 같은 액터의 중복 요청을 막는다.
    TGuardValue<bool> DispatchGuard(bIsDispatchingRequest, true);
    const bool bHasInterface = TargetActor->GetClass()->ImplementsInterface(ULMInteractable::StaticClass());
    const ELMInteractionRequestHandling Handling = bHasInterface ? Execute_HandleInteractionRequest(TargetActor, RequestActor) : ELMInteractionRequestHandling::PassThrough;

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

	// 사전 처리를 통과한 로어 대상은 일반 Interact 대신 대화 창을 연다.
	if (IsValid(LoreData))
	{
		APawn* RequestPawn = Cast<APawn>(RequestActor);
		ALMPlayerController* OpeningController = IsValid(RequestPawn) ? Cast<ALMPlayerController>(RequestPawn->GetController()) : nullptr;
		// 열기 도중 카드가 제거되어도 닫을 수 있게 하되, 기존 창의 참조는 유지한다.
		const bool bAddedLoreLink = !LoreController.IsValid() && IsValid(OpeningController);
		if (bAddedLoreLink)
			LoreController = OpeningController;

		if (!IsValid(OpeningController) || !OpeningController->OpenLoreWidget(this, LoreData))
		{
			// 실패한 요청이 새로 연결한 참조만 해제한다.
			if (bAddedLoreLink)
				LoreController.Reset();
			R.bCanInteraction = false;
			R.Failure = EInteractionFailure::ExecutionFailed;
		}
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



