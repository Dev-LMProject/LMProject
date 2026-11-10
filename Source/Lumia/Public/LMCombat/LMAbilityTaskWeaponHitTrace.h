#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "LMAbilityTaskWeaponHitTrace.generated.h"

class ALMBaseCharacter;
class UAnimInstance;
class UAnimMontage;
class ULMAttackData;
class ULMAnimNotifyStateAttackHitWindow;
class USkeletalMeshComponent;
struct FAnimMontageInstance;
struct FGameplayEventData;

/** 단일 공격의 타격 구간에서 검 경로를 검사하고 대상마다 한 번 피해를 요청한다. */
UCLASS()
class LUMIA_API ULMAbilityTaskWeaponHitTrace : public UAbilityTask
{
    GENERATED_BODY()

public:
    /** 공격 설정을 보관한 Task를 생성한다. 실행은 ReadyForActivation으로 시작한다. */
    static ULMAbilityTaskWeaponHitTrace* CreateWeaponHitTraceTask(UGameplayAbility* OwningAbility, UAnimMontage* InAttackMontage, ULMAttackData* InAttackData);

    /** 설정을 검증하고 타격 이벤트와 완료 포즈 알림을 연결한다. */
    virtual void Activate() override;

protected:
    /** 종료 상태를 먼저 기록한 뒤 직접 등록한 알림을 해제한다. */
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    /** 이번 공격의 캐릭터·ASC·몽타주 실행이 여전히 유효한지 확인한다. */
    FAnimMontageInstance* GetValidAttackInstance() const;

    /** 이번 몽타주 실행에서 발생한 타격 시작·종료·중단만 받아들인다. */
    void HandleHitWindowEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

    /** 완료된 두 포즈 사이에서 타격 허용 시간에 속하는 부분만 검사한다. */
    void HandlePoseFinalized();

    /** 검 전체를 덮는 구체 궤적으로 피격 후보를 찾고 공통 피해 함수를 호출한다. */
    bool TraceWeaponMovement(const FVector& PreviousStart, const FVector& PreviousEnd, const FVector& CurrentStart, const FVector& CurrentEnd);

    /** 안전하게 복원할 수 없는 설정이나 샘플을 알리고 판정을 중단한다. */
    void StopTraceWithWarning(const TCHAR* Reason);

    UPROPERTY()
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY()
    TObjectPtr<ULMAttackData> AttackData;

    TWeakObjectPtr<ALMBaseCharacter> AttackerCharacter;
    TWeakObjectPtr<USkeletalMeshComponent> AttackerMesh;
    TWeakObjectPtr<UAnimInstance> AttackerAnimInstance;
    TWeakObjectPtr<ULMAnimNotifyStateAttackHitWindow> HitWindowNotify;

    FGameplayTagContainer HitWindowEventTags;
    FDelegateHandle GameplayEventHandle;
    FDelegateHandle PoseFinalizedHandle;

    /** 겹침 종료와 관계없이 이번 공격에서 피해를 요청한 대상을 기억한다. */
    TSet<TWeakObjectPtr<AActor>> ProcessedReceivers;

    int32 MontageInstanceID = INDEX_NONE;
    float HitWindowStartTime = 0.0f;
    float HitWindowEndTime = 0.0f;

    float PreviousMontagePosition = 0.0f;
    double PreviousWorldTime = 0.0;

    FVector PreviousBladeStart = FVector::ZeroVector;
    FVector PreviousBladeEnd = FVector::ZeroVector;
    FVector PreviousAttackerLocation = FVector::ZeroVector;

    bool bHasPreviousPose = false;
    bool bHitWindowStarted = false;
    bool bHitWindowOpen = false;
    bool bFinishAfterPose = false;
    bool bStopped = false;
};
