#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LMInteractInputAbility.generated.h"

/** 선택한 대상에 상호작용을 한 번 요청하는 입력 능력. 해킹 세션은 대상이 관리한다. */
UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMInteractInputAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    /** 기존 입력 GA와 같은 로컬 실행 정책과 공통 차단 태그를 설정한다. */
    ULMInteractInputAbility();

protected:
    /** 요청 당시의 플레이어와 선택 대상을 검증하고 기존 Interactor에 한 번 전달한다. */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
    // 이전 요청의 콜백이 같은 인스턴스의 새 활성화를 종료하지 못하게 구분한다.
    uint64 ActivationSerial = 0;
};
