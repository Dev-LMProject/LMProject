#include "LMGAS/LMInteractInputAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "LMComponent/LMInteractorComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMPlayer/LMPlayerController.h"

ULMInteractInputAbility::ULMInteractInputAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
    bRetriggerInstancedAbility = false;
    bReplicateInputDirectly = false;

    ActivationBlockedTags.AddTag(LMGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(LMGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(LMGameplayTags::State_Input_Blocked);
}

void ULMInteractInputAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const uint64 ThisActivation = ++ActivationSerial;
    const TWeakObjectPtr<ULMInteractInputAbility> WeakThis(this);
    const TWeakObjectPtr<APawn> RequestPawn(Cast<APawn>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
    const TWeakObjectPtr<ALMPlayerController> RequestController(Cast<ALMPlayerController>(ActorInfo ? ActorInfo->PlayerController.Get() : nullptr));
    const TWeakObjectPtr<UAbilitySystemComponent> RequestASC(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
    const TWeakObjectPtr<ULMInteractorComponent> Interactor(RequestPawn.IsValid() ? RequestPawn->FindComponentByClass<ULMInteractorComponent>() : nullptr);
    const TWeakObjectPtr<AActor> Target(RequestController.IsValid() ? RequestController->GetSelectedInteractionActor() : nullptr);

    // 같은 인스턴스가 재활성화됐으면 이전 요청의 후속 처리를 중단한다.
    const auto IsCurrentActivation = [WeakThis, ThisActivation]()
    {
        return WeakThis.IsValid() && WeakThis->ActivationSerial == ThisActivation && WeakThis->IsActive();
    };

    // 콜백 전후에 같은 플레이어의 유효한 요청인지 확인한다.
    const auto HasRequestContext = [WeakThis, RequestPawn, RequestController, RequestASC, Interactor, Target]()
    {
        return RequestPawn.IsValid()
            && RequestController.IsValid()
            && RequestASC.IsValid()
            && Interactor.IsValid()
            && Target.IsValid()
            && RequestController->IsLocalPlayerController()
            && RequestController->GetPawn() == RequestPawn.Get()
            && RequestPawn->GetController() == RequestController.Get()
            && RequestASC->GetAvatarActor() == RequestPawn.Get()
            && RequestASC->GetOwnerActor() == RequestController->GetPlayerState<APlayerState>()
            && RequestASC->AbilityActorInfo.IsValid()
            && RequestASC->AbilityActorInfo->PlayerController.Get() == RequestController.Get()
            && Interactor->GetOwner() == RequestPawn.Get()
            && !RequestASC->HasAnyMatchingGameplayTags(WeakThis->ActivationBlockedTags)
            && !UGameplayStatics::IsGamePaused(RequestPawn.Get());
    };

    // 이번 요청의 활성화만 종료하고, 이미 취소됐거나 재활성화된 능력은 건드리지 않는다.
    const auto FinishActivation = [WeakThis, RequestASC, Handle, ActivationInfo, IsCurrentActivation](bool bWasCancelled)
    {
        if (!IsCurrentActivation() || !RequestASC.IsValid() || !RequestASC->AbilityActorInfo.IsValid())
        {
            return;
        }

        WeakThis->EndAbility(Handle, RequestASC->AbilityActorInfo.Get(), ActivationInfo, false, bWasCancelled);
    };

    if (!IsCurrentActivation() || !HasRequestContext())
    {
        FinishActivation(true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        FinishActivation(true);
        return;
    }

    // Commit의 콜백에서 조종 대상이 바뀌거나 이 GA가 종료될 수 있다.
    if (!IsCurrentActivation() || !HasRequestContext())
    {
        FinishActivation(true);
        return;
    }

    const FLMInteractionResult Result = Interactor->TryInteract(Target.Get());
    if (Result.Failure != EInteractionFailure::Succsess)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Interaction request failed: %d"), static_cast<int32>(Result.Failure));
    }

    // 해킹 진입의 입력 정리가 이 GA를 취소해도 해킹 세션은 종료하지 않는다.
    FinishActivation(Result.Failure != EInteractionFailure::Succsess);
}
