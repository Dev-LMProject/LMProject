#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LMDodgeAbility.generated.h"

class UAnimInstance;
class UAnimMontage;
struct FBranchingPointNotifyPayload;

// 플레이어와 AI가 같은 회피 판정과 몽타주 종료 규칙을 사용한다.
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMDodgeAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    // 캐릭터별 실행 정책과 회피를 시작할 수 없는 상태를 지정한다.
    ULMDodgeAbility();

    // 이 회피의 실제 재생에서 온 ANS가 맞으면 창을 열거나 닫는다.
    bool SetJustDodgeWindow(const FBranchingPointNotifyPayload& Payload, bool bOpen);

    // 이번 타격을 회피로 무효화할 수 있으면 true를 반환한다.
    bool TryAvoidDamage();

    // 몽타주의 이동 가능 지점을 지난 회피를 방향 입력으로 끝낸다.
    void FinishRecoveryOnMove();

    // 외부 취소가 접수되면 피해 보호부터 해제한다.
    virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

protected:
    // 이번 회피를 준비하고 자식 BP의 활성화 이벤트를 실행한다.
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // 몽타주와 능력의 모든 종료 경로에서 이번 판정을 정리한다.
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // 재생 실패나 몽타주 중단을 BP에서 취소된 종료로 전달한다.
    UFUNCTION(BlueprintCallable, Category = "LM|Dodge")
    void AbortDodge();

private:
    // 한 번의 회피에서 열릴 수 있는 판정과 종료 경로를 구분한다.
    enum class ELMDodgeState : uint8
    {
        Stopped,
        Ready,
        WindowOpen,
        JustDodged,
        Cancelling
    };

    // 같은 그룹에서 다음 몽타주가 시작되면 이전 회피의 보호를 끝낸다.
    UFUNCTION()
    void HandleMontageStarted(UAnimMontage* StartedMontage);

    // 이번 재생에서 대시 이동이 끝났다는 알림을 받는다.
    UFUNCTION()
    void HandleDodgeNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

    // 피해 판정을 닫고 구독했던 애니메이션 인스턴스에서 알림을 해제한다.
    void StopDodgeJudgment();

    ELMDodgeState DodgeState = ELMDodgeState::Stopped;
    TWeakObjectPtr<UAnimInstance> DodgeAnim;
    int32 DodgeMontageInstanceID = INDEX_NONE;
    bool bCanFinishOnMove = false;
};
