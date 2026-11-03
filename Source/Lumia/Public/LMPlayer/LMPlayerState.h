#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "LMAbilitySystem/LMAbilitySet.h"
#include "LMPlayerState.generated.h"

class ULMAbilitySystemComponent;

/** 플레이어의 ASC를 소유하고 기본 능력 목록을 관리한다. */
UCLASS()
class LUMIA_API ALMPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    /** 싱글플레이에서 사용할 ASC를 생성한다. */
    ALMPlayerState();

    /** GAS 인터페이스를 통해 플레이어의 ASC를 반환한다. */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    /** ASC를 플레이어 캐릭터에 연결하고 기본 능력을 준비한다. */
    void InitializeAbilitySystem(AActor* PlayerCharacter);

protected:
    /** PlayerState의 플레이가 종료될 때 입력과 능력 실행을 정리한다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 플레이어의 능력·효과·태그를 관리하는 GAS 컴포넌트. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|GAS")
    TObjectPtr<ULMAbilitySystemComponent> AbilitySystemComponent;

    /** 블루프린트에서 지정하는 플레이어의 기본 능력 묶음 목록. */
    UPROPERTY(EditDefaultsOnly, Category = "LM|GAS")
    TArray<TObjectPtr<ULMAbilitySet>> StartupAbilitySets;

private:
    /** 기본 능력 묶음을 중복 없이 최초 한 번 등록한다. */
    void EnsureStartupAbilitiesGranted();

    /** AbilitySet이 실제로 부여한 능력들의 핸들 기록. */
    UPROPERTY(Transient)
    FLMAbilitySetGrantedHandles StartupGrantedAbilities;

    /** 초기화가 다시 호출되어도 능력을 중복 등록하지 않기 위한 상태. */
    bool bStartupAbilitiesGranted = false;
};