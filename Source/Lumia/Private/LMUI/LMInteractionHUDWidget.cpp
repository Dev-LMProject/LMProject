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

// 감지된 장치 중 근접 UI에 표시할 대상만 모아 목록과 휠 선택을 갱신한다.
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
            // 직접 조작하는 장치는 감지 목록에 남겨 두고 HUD의 행과 선택에서 제외한다.
            if (!IsValid(Interaction) || Interaction->IsPromptHidden())
                continue;

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

// 목록이 있으면 선택에 입력을 사용하고, 비었으면 카메라 줌에 넘길 수 있게 한다.
bool ULMInteractionHUDWidget::MoveSelection(int32 Direction)
{
    if (DisplayedTargets.IsEmpty() || Direction == 0)
    {
        return false;
    }

    const int32 Step = Direction > 0 ? 1 : -1;
    const int32 PreviousIndex = DisplayedTargets.IndexOfByKey(SelectedActor);
    const int32 NextIndex = PreviousIndex == INDEX_NONE
        ? (Step > 0 ? 0 : DisplayedTargets.Num() - 1)
        : FMath::Clamp(PreviousIndex + Step, 0, DisplayedTargets.Num() - 1);
    SelectedActor = DisplayedTargets[NextIndex];
    OnInteractionSelectionChanged(SelectedActor.Get());
    return true;
}

AActor* ULMInteractionHUDWidget::GetSelectedActor() const
{
    return SelectedActor.Get();
}
