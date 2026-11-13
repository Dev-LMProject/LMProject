#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "LMAbilityTaskAutoFocus.generated.h"

class ALMBaseCharacter;
class ALMPlayerController;

// 캐릭터나 Controller 연결을 잃어 Task를 끝내야 할 때 GA에 보내는 알림이다.
DECLARE_MULTICAST_DELEGATE(FLMFocusStoppedDelegate);

// Controller의 카메라 계산 완료 알림을 받을 때마다 후보를 다시 검사한다.
UCLASS()
class LUMIA_API ULMAbilityTaskAutoFocus : public UAbilityTask
{
	GENERATED_BODY()

public:
	FLMFocusStoppedDelegate OnStopped;

	// OwningAbility를 소유자로 하는 Task를 만들고 두 설정값을 복사한다.
	static ULMAbilityTaskAutoFocus* CreateAutoFocus(UGameplayAbility* OwningAbility, float _FocusRange, float _SwitchMargin);

	// 사용할 Controller와 캐릭터를 찾고 카메라 완료 알림에 함수를 연결한다.
	virtual void Activate() override;

protected:
	// 카메라 알림에서 자신을 제거하고 Controller의 후보·선택·표식을 비운다.
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	// 매번 전역 검색으로 Controller를 찾지 않고 처음 연결된 Controller를 기억한다.
	// 약한 참조이므로 이 변수 때문에 Controller가 삭제되지 못하는 일은 없다.
	TWeakObjectPtr<ALMPlayerController> FocusController;

	// 이 Task를 시작한 실제 플레이어 캐릭터를 기억한다.
	TWeakObjectPtr<ALMBaseCharacter> FocusPawn;

	// GA에서 전달받은 검색 거리와 전환 기준이다.
	float FocusRange = 0.0f;
	float SwitchMargin = 0.0f;

	// 현재 프레임의 후보와 선택을 계산해 Controller에 전달한다.
	void UpdateFocus();

	// 진행 불가를 GA에 알리고 이 Task를 끝낸다.
	void StopFocus();
};