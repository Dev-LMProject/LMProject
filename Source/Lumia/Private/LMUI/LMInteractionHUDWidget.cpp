#include "LMUI/LMInteractionHUDWidget.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LMComponent/LMInteractorComponent.h"

void ULMInteractionHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(
            this, &ThisClass::HandlePossessedPawnChanged);
        BindInteractor(PlayerController->GetPawn());
    }
}

void ULMInteractionHUDWidget::NativeDestruct()
{
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->OnPossessedPawnChanged.RemoveDynamic(
            this, &ThisClass::HandlePossessedPawnChanged);
    }
    BindInteractor(nullptr);
    Super::NativeDestruct();
}

void ULMInteractionHUDWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    BindInteractor(NewPawn);
}

void ULMInteractionHUDWidget::BindInteractor(APawn* Pawn)
{
    if (ULMInteractorComponent* PreviousInteractor = Interactor.Get())
    {
        PreviousInteractor->InteractionTargetsChanged.RemoveDynamic(
            this, &ThisClass::RefreshTargets);
        PreviousInteractor->InteractionActionTagsChanged.RemoveAll(this);
    }

    Interactor = IsValid(Pawn) ? Pawn->FindComponentByClass<ULMInteractorComponent>() : nullptr;
    SelectedActor.Reset();
    if (ULMInteractorComponent* CurrentInteractor = Interactor.Get())
    {
        CurrentInteractor->InteractionTargetsChanged.AddUniqueDynamic(
            this, &ThisClass::RefreshTargets);
        CurrentInteractor->InteractionActionTagsChanged.AddUObject(this, &ThisClass::RefreshTargets);
    }
    RefreshTargets();
}

void ULMInteractionHUDWidget::RefreshTargets()
{
    for (const TWeakObjectPtr<ULMInteractionComponent>& Observed : ObservedInteractions)
    {
        if (ULMInteractionComponent* Interaction = Observed.Get())
        {
            Interaction->InteractionTagsChanged.RemoveAll(this);
        }
    }
    ObservedInteractions.Reset();

    DisplayedTargets.Reset();
    TArray<AActor*> Targets;
    if (ULMInteractorComponent* CurrentInteractor = Interactor.Get())
    {
        const FGameplayTagContainer ActionTags = CurrentInteractor->GetInteractionActionTags();
        for (AActor* Target : CurrentInteractor->GetInteractionTargets())
        {
            ULMInteractionComponent* Interaction = Target->FindComponentByClass<ULMInteractionComponent>();
            if (!IsValid(Interaction))
            {
                continue;
            }

            ObservedInteractions.Add(TWeakObjectPtr<ULMInteractionComponent>(Interaction));
            Interaction->InteractionTagsChanged.AddUObject(this, &ThisClass::RefreshTargets);
            if (Interaction->MatchesInteractionTags(ActionTags))
            {
                DisplayedTargets.Add(TWeakObjectPtr<AActor>(Target));
                Targets.Add(Target);
            }
        }
    }

    if (!DisplayedTargets.Contains(SelectedActor))
    {
        SelectedActor.Reset();
    }

    OnInteractionTargetsChanged(Targets);
    OnInteractionSelectionChanged(SelectedActor.Get());
}

void ULMInteractionHUDWidget::MoveSelection(int32 Direction)
{
    if (DisplayedTargets.IsEmpty() || Direction == 0)
    {
        return;
    }

    const int32 Step = Direction > 0 ? 1 : -1;
    const int32 PreviousIndex = DisplayedTargets.IndexOfByKey(SelectedActor);
    const int32 NextIndex = PreviousIndex == INDEX_NONE
        ? (Step > 0 ? 0 : DisplayedTargets.Num() - 1)
        : FMath::Clamp(PreviousIndex + Step, 0, DisplayedTargets.Num() - 1);
    SelectedActor = DisplayedTargets[NextIndex];
    OnInteractionSelectionChanged(SelectedActor.Get());
}

AActor* ULMInteractionHUDWidget::GetSelectedActor() const
{
    return SelectedActor.Get();
}

FLMInteractionResult ULMInteractionHUDWidget::TryInteractSelected()
{
    if (ULMInteractorComponent* CurrentInteractor = Interactor.Get())
    {
        return CurrentInteractor->TryInteract(SelectedActor.Get());
    }

    FLMInteractionResult Result;
    Result.Failure = InteractionFailure::CantAccessActor;
    return Result;
}
