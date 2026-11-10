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

/** 몽타주의 타격 구간 하나에 해당하는 설정과 실행 기록이다. 각 공격 Task가 독립적으로 소유한다. */
struct FLMWeaponHitWindowState
{
    TWeakObjectPtr<ULMAnimNotifyStateAttackHitWindow> Notify;
    TWeakObjectPtr<ULMAttackData> AttackData;

    float StartTime = 0.0f;
    float EndTime = 0.0f;

    FVector PreviousBladeStart = FVector::ZeroVector;
    FVector PreviousBladeEnd = FVector::ZeroVector;

    /** 같은 타격 구간에서 피해를 요청한 대상을 기억한다. */
    TSet<TWeakObjectPtr<AActor>> ProcessedReceivers;

    bool bOpen = false;
    bool bPendingLastPose = false;
};

/** 기존 공격 몽타주의 여러 타격 구간에서 검 경로를 검사한다. */
UCLASS()
class LUMIA_API ULMAbilityTaskWeaponHitTrace : public UAbilityTask
{
    GENERATED_BODY()

public:
    /** 몽타주와 기본 공격 데이터를 보관한 Task를 생성한다. */
    static ULMAbilityTaskWeaponHitTrace* CreateWeaponHitTraceTask(UGameplayAbility* OwningAbility, UAnimMontage* InAttackMontage, ULMAttackData* InAttackData);

    /** 타격 구간을 검증하고 GameplayEvent와 완료 포즈 알림을 구독한다. */
    virtual void Activate() override;

    /** 섹션 이동 전에 이전 타격 상태와 검 위치를 비워 잘못된 연결 궤적을 막는다. */
    void ResetForMontageJump();

protected:
    /** 판정을 먼저 중단하고 직접 등록한 이벤트와 포즈 알림을 해제한다. */
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    /** 이번 공격의 캐릭터·ASC·몽타주 실행이 유지되는지 확인한다. 일시정지는 허용한다. */
    FAnimMontageInstance* GetValidAttackInstance() const;

    /** 이번 몽타주 실행에서 발생한 타격 구간의 시작·종료·취소를 처리한다. */
    void HandleHitWindowEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

    /** 완료된 포즈 사이에서 각 타격 구간에 해당하는 검 경로만 검사한다. */
    void HandlePoseFinalized();

    /** 검의 이동 경로를 검사하고 해당 타격 구간에서 대상마다 한 번 피해를 요청한다. */
    bool TraceWeaponMovement(FLMWeaponHitWindowState& Window, const FVector& PreviousStart, const FVector& PreviousEnd, const FVector& CurrentStart, const FVector& CurrentEnd);

    /** 잘못된 설정을 로그로 알리고 이번 판정 Task를 종료한다. */
    void StopTraceWithWarning(const TCHAR* Reason);

    UPROPERTY()
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY()
    TObjectPtr<ULMAttackData> AttackData;

    TWeakObjectPtr<ALMBaseCharacter> AttackerCharacter;
    TWeakObjectPtr<USkeletalMeshComponent> AttackerMesh;
    TWeakObjectPtr<UAnimInstance> AttackerAnimInstance;

    /** 각 타격 구간의 중복 피격 기록과 이전 검 위치를 따로 보관한다. */
    TArray<FLMWeaponHitWindowState> HitWindows;

    FGameplayTagContainer HitWindowEventTags;
    FDelegateHandle GameplayEventHandle;
    FDelegateHandle PoseFinalizedHandle;

    int32 MontageInstanceID = INDEX_NONE;

    /** 피해 콜백 중 섹션 이동이나 취소가 발생했는지 구분한다. */
    uint32 TraceRevision = 0;

    float PreviousMontagePosition = 0.0f;
    double PreviousWorldTime = 0.0;

    bool bHasPreviousPose = false;
    bool bProcessingPose = false;
    bool bStopped = false;
};