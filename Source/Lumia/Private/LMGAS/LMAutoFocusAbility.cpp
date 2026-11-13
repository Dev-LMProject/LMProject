#include "LMGAS/LMAutoFocusAbility.h"

#include "AbilitySystemComponent.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMGAS/LMAbilityTaskAutoFocus.h"
#include "LMPlayer/LMPlayerController.h"

// ASC에 부여된 이 능력 한 건이 실행 중 자신의 상태를 보관할 수 있게 설정한다.
ULMAutoFocusAbility::ULMAutoFocusAbility()
{
	// 능력이 실행되는 동안 같은 GA 객체를 사용한다.
	// 매번 새로운 실행 객체를 만드는 방식이 아니므로 Task 소유 관계를 유지하기 쉽다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 현재 프로젝트는 단일 로컬 플레이어 게임이고 선정 결과는 그 화면에서 사용한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 이미 실행 중일 때 다시 활성화 요청이 들어와도 기존 실행을 끊고 재시작하지 않는다.
	bRetriggerInstancedAbility = false;
}

// 능력이 부여됐고 사용할 캐릭터가 준비된 시점에 입력 없이 활성화를 요청한다.
void ULMAutoFocusAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// ActorInfo가 없으면 어느 ASC와 캐릭터의 능력인지 확인할 수 없다.
	if (!ActorInfo) return;

	// 현재 컴퓨터에서 조종하는 플레이어의 화면만 처리한다.
	if (!ActorInfo->IsLocallyControlled()) return;

	// 이미 실행 중인 능력에는 Task를 하나 더 만들지 않는다.
	if (Spec.IsActive()) return;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// 다른 종류의 Controller나 일반 Actor에 잘못 부여된 경우에는 시작하지 않는다.
	ALMPlayerController* Controller = Cast<ALMPlayerController>(ActorInfo->PlayerController.Get());
	ALMBaseCharacter* Pawn = Cast<ALMBaseCharacter>(ActorInfo->AvatarActor.Get());

	if (!ASC || !Controller || !Pawn) return;

	// Spec.Handle은 ASC에 등록된 이 능력 한 건의 식별자다.
	// 이 Handle을 전달해야 현재 부여된 오토포커싱 능력이 활성화된다.
	ASC->TryActivateAbility(Spec.Handle);
}

// 능력 활성화가 허용되면 오토포커싱 Task를 하나 만들어 실행한다.
void ULMAutoFocusAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// GAS가 이 함수를 호출할 때는 능력 소유자 정보가 준비되어 있어야 한다.
	check(ActorInfo);

	ALMPlayerController* Controller = Cast<ALMPlayerController>(ActorInfo->PlayerController.Get());
	ALMBaseCharacter* Pawn = Cast<ALMBaseCharacter>(ActorInfo->AvatarActor.Get());

	// 잘못된 소유자에게 실행된 능력을 활성 상태로 남겨 두지 않는다.
	if (!Controller || !Pawn)
	{
		// 마지막 true는 정상 완료가 아니라 실행 불가에 따른 취소라는 뜻이다.
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// GAS가 비용·쿨타임을 마지막으로 검사하고 적용하는 단계다.
	// 이번 GA에는 비용·쿨타임 에셋을 지정하지 않으므로 자원은 소비하지 않는다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 이 GA를 Task의 소유자로 지정하고 검색 거리와 전환 기준을 전달한다.
	ULMAbilityTaskAutoFocus* Task = ULMAbilityTaskAutoFocus::CreateAutoFocus(this, FocusRange, SwitchMargin);

	if (!Task)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// Task가 캐릭터 연결 등을 잃으면 이 GA도 끝내도록 알림을 연결한다.
	// 먼저 연결해야 Task의 Activate 중 실패해도 실패 알림을 받을 수 있다.
	Task->OnStopped.AddUObject(this, &ThisClass::OnFocusStopped);

	// Task 생성만으로는 Activate가 실행되지 않으므로 여기서 실제 시작을 요청한다.
	Task->ReadyForActivation();

	// 여기서 EndAbility를 호출하지 않는다.
	// GA를 유지해야 Task가 이후 카메라 갱신마다 계속 검색할 수 있다.
}

// Task가 더 진행할 수 없다고 알리면 현재 실행 중인 GA를 취소한다.
void ULMAutoFocusAbility::OnFocusStopped()
{
	if (!IsActive()) return;

	// 현재 능력의 Handle과 활성화 정보를 사용해 이번 실행을 종료한다.
	// GA 종료에 따라 이 GA가 소유한 Task도 종료 처리된다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}