#include "LMCombat/LMWeaponPresentationComponent.h"

#include "AlphaBlend.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

/** 별도 매 프레임 갱신 없이 사용하는 컴포넌트를 구성한다. */
ULMWeaponPresentationComponent::ULMWeaponPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

/** 몸과 검 메시를 연결하고 애니메이션 갱신 순서를 지정한다. */
void ULMWeaponPresentationComponent::Initialize(
    USkeletalMeshComponent* InBodyMesh,
    USkeletalMeshComponent* InSwordMesh)
{
    BodyMesh = InBodyMesh;
    SwordMesh = InSwordMesh;

    if (BodyMesh.IsValid() && SwordMesh.IsValid())
    {
        // 몸의 최신 몽타주 시간을 검이 뒤에서 읽도록 한다.
        SwordMesh->AddTickPrerequisiteComponent(BodyMesh.Get());
    }
}

/** 몸·검 몽타주의 길이와 섹션 시간 배치가 맞는지 검사한다. */
bool ULMWeaponPresentationComponent::CanPlayPair(
    const UAnimMontage* BodyMontage,
    const UAnimMontage* SwordMontage) const
{
    if (!BodyMesh.IsValid() ||
        !SwordMesh.IsValid() ||
        !BodyMesh->GetAnimInstance() ||
        !SwordMesh->GetAnimInstance() ||
        !BodyMontage ||
        !SwordMontage)
    {
        return false;
    }

    constexpr float TimeTolerance = 0.001f;

    if (!FMath::IsNearlyEqual(
            BodyMontage->GetPlayLength(),
            SwordMontage->GetPlayLength(),
            TimeTolerance) ||
        BodyMontage->GetNumSections() != SwordMontage->GetNumSections())
    {
        return false;
    }

    for (int32 Index = 0;
         Index < BodyMontage->GetNumSections();
         ++Index)
    {
        const FName SectionName = BodyMontage->GetSectionName(Index);
        const int32 SwordIndex =
            SwordMontage->GetSectionIndex(SectionName);

        if (SwordIndex == INDEX_NONE)
        {
            return false;
        }

        float BodyStart = 0.0f;
        float BodyEnd = 0.0f;
        float SwordStart = 0.0f;
        float SwordEnd = 0.0f;

        BodyMontage->GetSectionStartAndEndTime(
            Index, BodyStart, BodyEnd);

        SwordMontage->GetSectionStartAndEndTime(
            SwordIndex, SwordStart, SwordEnd);

        if (!FMath::IsNearlyEqual(
                BodyStart, SwordStart, TimeTolerance) ||
            !FMath::IsNearlyEqual(
                BodyEnd, SwordEnd, TimeTolerance))
        {
            return false;
        }
    }

    return true;
}

/** 이미 재생 중인 몸 몽타주를 검이 따라가도록 시작한다. */
bool ULMWeaponPresentationComponent::StartFollowingBody(
    UAnimMontage* BodyMontage,
    UAnimMontage* SwordMontage,
    uint32 ExecutionToken)
{
    if (ExecutionToken == 0 ||
        ActiveExecutionToken != 0 ||
        !CanPlayPair(BodyMontage, SwordMontage))
    {
        return false;
    }

    UAnimInstance* BodyAnim = BodyMesh->GetAnimInstance();
    UAnimInstance* SwordAnim = SwordMesh->GetAnimInstance();

    if (!BodyAnim->GetActiveInstanceForMontage(BodyMontage))
    {
        return false;
    }

    const float PlayResult = SwordAnim->Montage_Play(
        SwordMontage,
        BodyAnim->Montage_GetPlayRate(BodyMontage),
        EMontagePlayReturnType::MontageLength,
        BodyAnim->Montage_GetPosition(BodyMontage),
        false);

    if (PlayResult <= 0.0f)
    {
        return false;
    }

    FAnimMontageInstance* SwordInstance =
        SwordAnim->GetActiveInstanceForMontage(SwordMontage);

    if (!SwordInstance)
    {
        SwordAnim->Montage_Stop(0.0f, SwordMontage);
        return false;
    }

    ActiveExecutionToken = ExecutionToken;
    SwordMontageInstanceID = SwordInstance->GetInstanceID();
    PlayingSwordAnim = SwordAnim;
    PlayingSwordMontage = SwordMontage;

    SwordAnim->MontageSync_Follow(
        SwordMontage,
        BodyAnim,
        BodyMontage);

    return true;
}

/** 해당 실행이 시작한 검 몽타주만 정리한다. */
void ULMWeaponPresentationComponent::StopFollowingBody(
    uint32 ExecutionToken,
    float BlendOutSeconds)
{
    if (ExecutionToken == 0 ||
        ExecutionToken != ActiveExecutionToken)
    {
        return;
    }

    UAnimInstance* SwordAnim = PlayingSwordAnim.Get();
    UAnimMontage* SwordMontage = PlayingSwordMontage.Get();

    FAnimMontageInstance* Instance = SwordAnim
        ? SwordAnim->GetMontageInstanceForID(SwordMontageInstanceID)
        : nullptr;

    // 이전 실행의 늦은 정리가 다음 실행에 영향을 주지 않게 먼저 해제한다.
    ActiveExecutionToken = 0;
    SwordMontageInstanceID = INDEX_NONE;
    PlayingSwordAnim.Reset();
    PlayingSwordMontage.Reset();

    if (SwordAnim && SwordMontage && Instance)
    {
        SwordAnim->MontageSync_StopFollowing(SwordMontage);

        Instance->Stop(
            FAlphaBlend(FMath::Max(0.0f, BlendOutSeconds)),
            true);
    }
}

/** 컴포넌트 종료 시 남은 재생과 갱신 순서 연결을 정리한다. */
void ULMWeaponPresentationComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopFollowingBody(ActiveExecutionToken, 0.0f);

    if (BodyMesh.IsValid() && SwordMesh.IsValid())
    {
        SwordMesh->RemoveTickPrerequisiteComponent(BodyMesh.Get());
    }

    Super::EndPlay(EndPlayReason);
}