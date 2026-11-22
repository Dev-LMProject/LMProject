#include "LMCombat/LMDodgeAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameFramework/Pawn.h"
#include "LMGameplayTags/LMGameplayTags.h"

namespace LMDodge
{
    const FName Forward(TEXT("Default"));
    const FName Back(TEXT("Dash_Back"));
    const FName JustForward(TEXT("JustDodge_Forward"));
    const FName JustBack(TEXT("JustDodge_Back"));
    const FName Recovery(TEXT("LM_DodgeRecovery"));
}

// 캐릭터마다 판정 상태를 보관하고 재생 중 재활성화를 막는다.
ULMDodgeAbility::ULMDodgeAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    bRetriggerInstancedAbility = false;

    // 기존 전투 규칙에 따라 사망하거나 기절한 캐릭터의 회피 시작을 막는다.
    ActivationBlockedTags.AddTag(LMGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(LMGameplayTags::State_Stunned);
}

// 자식 BP가 회피를 재생한 뒤, 그 실행의 몽타주 인스턴스 ID를 기록한다.
void ULMDodgeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 자식 BP가 ActivateAbility 안에서 지연 없이 몽타주를 시작한다.
    DodgeState = ELMDodgeState::Ready;
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // BP에서 이미 종료했거나 새로운 실행이 추적을 시작한 경우에는 그대로 둔다.
    if (!IsActive() || bIsAbilityEnding || DodgeState != ELMDodgeState::Ready || DodgeMontageInstanceID != INDEX_NONE)
    {
        return;
    }

    const FGameplayAbilityActorInfo* const DodgeActorInfo = GetCurrentActorInfo();
    UAnimInstance* const PlayingAnim = DodgeActorInfo ? DodgeActorInfo->GetAnimInstance() : nullptr;
    const UAnimMontage* const PlayingMontage = GetCurrentMontage();
    const FAnimMontageInstance* const PlayingInstance = IsValid(PlayingAnim) && IsValid(PlayingMontage) ? PlayingAnim->GetActiveInstanceForMontage(PlayingMontage) : nullptr;
    if (!PlayingInstance)
    {
        // 재생을 찾지 못하면 판정이 없는 회피 GA가 계속 활성 상태로 남지 않게 한다.
        AbortDodge();
        return;
    }

    // 같은 몽타주 애셋을 다시 재생해도 이전 회피의 End와 구별한다.
    DodgeAnim = PlayingAnim;
    DodgeMontageInstanceID = PlayingInstance->GetInstanceID();
    PlayingAnim->OnMontageStarted.AddUniqueDynamic(this, &ThisClass::HandleMontageStarted);
    PlayingAnim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::HandleDodgeNotify);

    // BP가 앞 대시에서 시작한 경우, 캐릭터 뒤쪽으로 이동하는 입력은 뒤 대시로 보낸다.
    const APawn* const Pawn = Cast<APawn>(DodgeActorInfo->AvatarActor.Get());
    if (Pawn && PlayingInstance->GetCurrentSection() == LMDodge::Forward && PlayingMontage->IsValidSectionName(LMDodge::Back))
    {
        FVector MoveDirection = Pawn->GetPendingMovementInputVector();
        if (MoveDirection.IsNearlyZero())
            MoveDirection = Pawn->GetLastMovementInputVector();
        if (FVector::DotProduct(MoveDirection.GetSafeNormal2D(), Pawn->GetActorForwardVector()) < -0.1f)
            MontageJumpToSection(LMDodge::Back);
    }
}

// 현재 몸체의 같은 몽타주 재생에서 온 판정 구간만 처리한다.
bool ULMDodgeAbility::SetJustDodgeWindow(const FBranchingPointNotifyPayload& Payload, bool bOpen)
{
    if (!IsActive() || bIsAbilityEnding) return false;
    if (DodgeMontageInstanceID == INDEX_NONE || DodgeMontageInstanceID != Payload.MontageInstanceID)
    {
        return false;
    }

    const FGameplayAbilityActorInfo* const DodgeActorInfo = GetCurrentActorInfo();
    const UAnimInstance* const PlayingAnim = DodgeAnim.Get();

    // 몸체나 애님 인스턴스가 교체되었으면 이전 재생이 새 판정에 영향을 주지 않는다.
    if (!DodgeActorInfo || !IsValid(PlayingAnim) || PlayingAnim != DodgeActorInfo->GetAnimInstance())
    {
        return false;
    }

    // 종료 시점은 캡처한 ID를 비교하므로 블렌드 아웃으로 활성 목록에서 빠져도 닫을 수 있다.
    if (Payload.SkelMeshComponent != PlayingAnim->GetSkelMeshComponent() || !IsValid(Payload.SequenceAsset) || Payload.SequenceAsset != GetCurrentMontage())
    {
        return false;
    }

    if (bOpen && DodgeState == ELMDodgeState::Ready)
        DodgeState = ELMDodgeState::WindowOpen;

    // 타격 없이 닫힌 경우만 Ready로 되돌리고 성공 상태는 몽타주 종료까지 유지한다.
    if (!bOpen && DodgeState == ELMDodgeState::WindowOpen)
        DodgeState = ELMDodgeState::Ready;

    return true;
}

// 창 안의 첫 타격으로 성공을 기록하고, 같은 회피에서 받는 후속 타격도 막는다.
bool ULMDodgeAbility::TryAvoidDamage()
{
    if (!IsActive() || bIsAbilityEnding) return false;
    if (DodgeState == ELMDodgeState::JustDodged) return true;
    if (DodgeState != ELMDodgeState::WindowOpen) return false;

    // 첫 타격을 무효화하기 전에 성공을 저장해 연속 타격도 같은 결과를 보게 한다.
    DodgeState = ELMDodgeState::JustDodged;
    UAnimInstance* const Anim = DodgeAnim.Get();
    const FAnimMontageInstance* const Instance = Anim ? Anim->GetMontageInstanceForID(DodgeMontageInstanceID) : nullptr;
    if (Instance && Instance->Montage && Instance->Montage == GetCurrentMontage())
    {
        const FName Section = Instance->GetCurrentSection();
        const FName SuccessSection = Section == LMDodge::Back ? LMDodge::JustBack : LMDodge::JustForward;
        if ((Section == LMDodge::Forward || Section == LMDodge::Back) && Instance->Montage->IsValidSectionName(SuccessSection))
        {
            // 일반 대시의 마무리와 판정 창이 겹쳐도 성공 시퀀스는 자기 마무리까지 재생한다.
            bCanFinishOnMove = false;
            MontageJumpToSection(SuccessSection);
        }
    }
    return true;
}

// 현재 몽타주 인스턴스의 마무리 알림만 이동 취소를 허용한다.
void ULMDodgeAbility::HandleDodgeNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
    if (NotifyName == LMDodge::Recovery && DodgeMontageInstanceID != INDEX_NONE && DodgeMontageInstanceID == Payload.MontageInstanceID && Payload.SequenceAsset == GetCurrentMontage())
        bCanFinishOnMove = true;
}

// 포즈가 블렌드 아웃되는 동안에도 이번 입력으로 캐릭터를 움직일 수 있게 한다.
void ULMDodgeAbility::FinishRecoveryOnMove()
{
    if (!bCanFinishOnMove || !IsActive() || bIsAbilityEnding) return;
    UAnimInstance* const Anim = DodgeAnim.Get();
    FAnimMontageInstance* const Instance = Anim ? Anim->GetMontageInstanceForID(DodgeMontageInstanceID) : nullptr;
    if (Instance && Instance->Montage == GetCurrentMontage())
        Instance->PushDisableRootMotion();
    AbortDodge();
}

// 자연 블렌드 아웃 도중에도 같은 그룹의 새 재생을 즉시 구별한다.
void ULMDodgeAbility::HandleMontageStarted(UAnimMontage* StartedMontage)
{
    const UAnimMontage* const OwnMontage = GetCurrentMontage();
    if (!IsValid(OwnMontage) || !IsValid(StartedMontage)) return;

    // 새 몽타주가 같은 애셋이어도 이전 회피의 판정만 정리한다.
    if (OwnMontage->GetGroupName() == StartedMontage->GetGroupName())
        StopDodgeJudgment();
}

// 이번 실행의 판정을 닫고 자신이 받던 시작 알림을 해제한다.
void ULMDodgeAbility::StopDodgeJudgment()
{
    // 같은 GA가 다시 사용될 때 이전 몽타주의 End를 받아들이지 않는다.
    DodgeState = ELMDodgeState::Stopped;
    DodgeMontageInstanceID = INDEX_NONE;
    bCanFinishOnMove = false;
    if (UAnimInstance* const BoundDodgeAnim = DodgeAnim.Get())
    {
        BoundDodgeAnim->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleMontageStarted);
        BoundDodgeAnim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::HandleDodgeNotify);
    }
    DodgeAnim.Reset();
}

// 외부 취소가 접수되면 엔진이 종료를 마치기 전에 보호를 해제한다.
void ULMDodgeAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    if (IsActive() && CanBeCanceled())
    {
        StopDodgeJudgment();
        // 부모가 태스크에 취소를 통지하는 동안 중복 종료를 막는다.
        DodgeState = ELMDodgeState::Cancelling;
    }
    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

// BP에서 전달한 재생 실패나 중단을 취소된 능력 종료로 처리한다.
void ULMDodgeAbility::AbortDodge()
{
    if (!IsActive() || bIsAbilityEnding) return;
    if (DodgeState == ELMDodgeState::Cancelling) return;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// 종료 요청 시 다음 타격이 이전 회피의 성공을 사용하지 못하게 한다.
void ULMDodgeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!IsEndAbilityValid(Handle, ActorInfo)) return;

    // GAS가 종료를 잠시 미뤄도 판정과 알림 구독은 이 시점에 정리한다.
    StopDodgeJudgment();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
