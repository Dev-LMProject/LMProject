#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "LMAbilitySystem/LMAbilitySet.h"
#include "LMPlayerState.generated.h"

class ULMAbilitySystemComponent;
class ULMCombatAttributeSet;
class ULMAttackAbility;
struct FOnAttributeChangeData;

// 수치·해금·준비 가능 상태가 바뀔 때 기존 HUD에 표시 갱신을 요청한다.
DECLARE_MULTICAST_DELEGATE(FLMOnSpecialGaugeChanged);

/** 플레이어의 ASC를 소유하고 기본 능력 목록을 관리한다. */
UCLASS()
class LUMIA_API ALMPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    /** 싱글플레이에서 사용할 ASC와 전투 속성을 생성한다. */
    ALMPlayerState();

    /** GAS 인터페이스를 통해 플레이어의 ASC를 반환한다. */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    /** 캐릭터의 ASC 연결이 준비된 뒤 기본 능력 묶음을 최초 한 번 등록한다. */
    void EnsureStartupAbilitiesGranted();

    // 해금된 살아 있는 플레이어에게 유효 적중 보상을 더한다.
    UFUNCTION(BlueprintCallable, Category = "LM|Special")
    bool AddSpecialCharge(float ChargeAmount);

    // 성공 판정이 끝난 패링의 보상을 공통 충전 경로에 전달한다.
    UFUNCTION(BlueprintCallable, Category = "LM|Special")
    bool AddParryCharge();

    // 특수공격 사용 권한이 있는지 기존 해금 태그로 확인한다.
    UFUNCTION(BlueprintPure, Category = "LM|Special")
    bool IsSpecialUnlocked() const;

    // 사용 권한·생존·만충 조건을 함께 확인한다.
    UFUNCTION(BlueprintPure, Category = "LM|Special")
    bool HasFullSpecialCharge() const;

    // 첫 타격 시작 조건을 만족할 때만 현재 게이지를 비운다.
    UFUNCTION(BlueprintCallable, Category = "LM|Special")
    bool TrySpendSpecialCharge();

    // 각성 시연을 위해 처음 해금할 때만 한 번 완충한다.
    UFUNCTION(BlueprintCallable, Category = "LM|Special")
    bool UnlockSpecialAttack();

    // 엘리트 처치와 카메라 복귀를 완료한 방이 HUD를 열고 한 번 완충한다.
    UFUNCTION(BlueprintCallable, Category = "LM|Special")
    bool ShowSpecialGauge();

    // 방 완료 전에는 게이지 위젯을 생성하지 않도록 표시 허용 상태를 반환한다.
    UFUNCTION(BlueprintPure, Category = "LM|Special")
    bool ShouldShowSpecialGauge() const;

    // 현재 콤보 창과 자원을 함께 검사한 실제 준비 가능 상태다.
    UFUNCTION(BlueprintPure, Category = "LM|Special")
    bool CanPrepareSpecialAttack() const;

    // 콤보 GA가 소유한 입력 창의 시작과 종료를 기록한다.
    void SetSpecialPrepareWindow(ULMAttackAbility* SourceAbility, bool bOpen);

    FLMOnSpecialGaugeChanged OnSpecialGaugeChanged;

protected:
    // ASC 등록이 끝난 뒤 수치·태그 변경을 한 번 구독한다.
    virtual void PostInitializeComponents() override;

    /** PlayerState의 플레이가 종료될 때 입력과 능력 실행을 정리한다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 플레이어의 능력·효과·태그를 관리하는 GAS 컴포넌트. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LM|GAS")
    TObjectPtr<ULMAbilitySystemComponent> AbilitySystemComponent;

    /** 블루프린트에서 지정하는 플레이어의 기본 능력 묶음 목록. */
    UPROPERTY(EditDefaultsOnly, Category = "LM|GAS")
    TArray<TObjectPtr<ULMAbilitySet>> StartupAbilitySets;
    
    /** 플레이어 ASC와 함께 유지할 실제 체력과 공통 피해 속성이다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LM|Combat")
    TObjectPtr<ULMCombatAttributeSet> CombatAttributes;

    // 패링 성공 한 번의 총 보상이며 일반 적중량을 별도로 더하지 않는다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Special", meta = (ClampMin = "0.0"))
    float ParryChargeGain = 20.0f;

private:
    // 체력 소진 시 자원을 초기화하고 HUD에 현재 상태를 전달한다.
    void HandleSpecialAttributeChanged(const FOnAttributeChangeData& Change);

    // 해금과 행동 제한의 변화만 표시 갱신에 전달한다.
    void HandleSpecialTagChanged(FGameplayTag Tag, int32 NewCount);

    // 죽은 플레이어의 충전·소비·해금 보상을 막는다.
    bool IsAliveForSpecial() const;

    TWeakObjectPtr<ULMAttackAbility> SpecialPrepareAbility;
    FDelegateHandle SpecialChargeHandle;
    FDelegateHandle MaxSpecialChargeHandle;
    FDelegateHandle SpecialHealthHandle;
    FDelegateHandle SpecialTagHandle;
    bool bUnlockChargeGiven = false;
    bool bSpecialGaugeShown = false;

    /** AbilitySet이 실제로 부여한 능력들의 핸들 기록. */
    UPROPERTY(Transient)
    FLMAbilitySetGrantedHandles StartupGrantedAbilities;

    /** 초기화가 다시 호출되어도 능력을 중복 등록하지 않기 위한 상태. */
    bool bStartupAbilitiesGranted = false;
};
