#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LMAutoFocusAbility.generated.h"

// 플레이어에게 이 능력이 부여되면 오토포커싱 Task 하나를 시작한다.
// 실제 후보 검색은 Task가 실행하고, 이 GA가 끝나면 Task도 함께 끝난다.
UCLASS()
class LUMIA_API ULMAutoFocusAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// 이 능력의 객체 생성 방식과 로컬 실행 방식을 지정한다.
	ULMAutoFocusAbility();

	// ASC에 능력이 부여되고 사용할 캐릭터가 준비되면 능력 활성화를 요청한다.
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	// 능력이 활성화되면 설정값을 Task에 전달하고 반복 검색을 시작한다.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 플레이어 원점에서 대상 표적점까지 허용할 최대 거리다.
	// 언리얼 기본 거리 단위가 cm이므로 1500은 15m다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus", meta = (ClampMin = "1.0", Units = "cm"))
	float FocusRange = 1500.0f;

	// 새 대상이 기존 대상보다 얼마나 더 화면 중앙에 가까워야 전환할지 정한다.
	// 0.03은 화면의 짧은 변 길이를 기준으로 약 3%의 점수 차이를 뜻한다.
	// 화면 밖 대상을 유지하는 시간이나 거리 여유가 아니다.
	UPROPERTY(EditDefaultsOnly, Category = "LM|Focus", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float SwitchMargin = 0.03f;

private:
	// Task가 캐릭터나 Controller를 잃어 검색을 계속할 수 없을 때 GA를 종료한다.
	void OnFocusStopped();
};