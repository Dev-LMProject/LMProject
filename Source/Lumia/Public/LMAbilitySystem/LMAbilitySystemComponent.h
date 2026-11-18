#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "LMAbilitySystemComponent.generated.h"

/** 정상 누름·정상 해제·강제 취소를 구분한다. */
enum class ELMAbilityInputPhase : uint8
{
	Pressed,
	Released,
	Canceled
};

/** 처리할 능력 입력 하나를 기록한다. */
struct FLMAbilityInputEvent
{
	FGameplayAbilitySpecHandle Handle;
	ELMAbilityInputPhase Phase = ELMAbilityInputPhase::Pressed;
};

/**
 * 공통 입력을 처리하는 ASC. 어떤 InputTag가 들어왔는지와 어떤 능력이 연결되어 있는지만 처리.
 * 
 * 빠른 클릭의 누름 / 해제 순서를 유지하기 위해서.
 * 같은 프레임에 취소된 입력이 공격으로 실행되는 것을 막기 위해서.
 * 첫 클릭이 능력 활성화와 추가 입력으로 두 번 소비되지 않게 하기 위해서.
 */
UCLASS()
class LUMIA_API ULMAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** 입력 태그에 연결된 능력의 최초 누름을 기록한다. */
	void AbilityInputTagPressed(FGameplayTag InputTag);
	
	/** 정상적으로 놓은 입력을 기록한다. */
	void AbilityInputTagReleased(FGameplayTag InputTag);
	
	/** 입력 중단을 정상 해제와 구분해서 기록한다. */
	void AbilityInputTagCanceled(FGameplayTag InputTag);
	
	/** 이번 프레임에 수집한 입력을 순서대로 처리한다. */
	void ProcessAbilityInput(bool bGamePaused);
	
	/** 남은 입력을 비운다. ExpectedAvatar가 있으면 해당 몸체의 연결이 유지되는 동안만 취소한다. */
	void ClearAbilityInput(const AActor* ExpectedAvatar = nullptr);
	
protected:
	/** 실행 중인 능력의 WaitInputPress Task에 누름을 전달한다. */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	
	/** 실행 중인 능력의 WaitInputRelease Task에 해제를 전달한다. */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	
	/** 능력 회수 시 해당 능력의 입력 기록도 제거한다. */
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	
private:
	/** 입력 태그에 대응하는 Spec을 찾아 입력 목록에 넣는다. */
	void QueueAbilityInput(FGameplayTag InputTag, ELMAbilityInputPhase Phase);
	
	/** 아직 처리하지 않은 누름·해제·취소 입력. */
	TArray<FLMAbilityInputEvent> PendingInputs;
	
	/** 현재 누르고 있다고 기록된 능력 입력. */
	TSet<FGameplayAbilitySpecHandle> HeldInputs;
	
	/** 처리 도중 입력이 초기화 되었는지 구분한다. */
	uint32 InputGeneration = 0;
};
