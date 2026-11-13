#include "LMGAS/LMAbilityTaskAutoFocus.h"

#include "Abilities/GameplayAbility.h"
#include "LMCamera/LMTargetQueryLibrary.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "LMPlayer/LMPlayerController.h"

// GA가 소유하는 Task를 만들고 에디터에서 설정한 값을 Task에 복사한다.
ULMAbilityTaskAutoFocus* ULMAbilityTaskAutoFocus::CreateAutoFocus(UGameplayAbility* OwningAbility, float _FocusRange, float _SwitchMargin)
{
	if (!IsValid(OwningAbility)) return nullptr;

	// 일반 NewObject 대신 AbilityTask 생성 함수를 사용해 소유 GA와 연결한다.
	ULMAbilityTaskAutoFocus* Task = NewAbilityTask<ULMAbilityTaskAutoFocus>(OwningAbility);

	// Task가 실행되는 동안 사용할 값을 저장한다.
	Task->FocusRange = _FocusRange;
	Task->SwitchMargin = _SwitchMargin;

	return Task;
}

// Task를 시작한 GA의 실제 Controller와 캐릭터를 찾고 카메라 알림을 구독한다.
void ULMAbilityTaskAutoFocus::Activate()
{
	Super::Activate();

	// 전역의 첫 번째 플레이어를 찾지 않고 이 능력에 연결된 플레이어 정보를 사용한다.
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	ALMPlayerController* Controller = ActorInfo ? Cast<ALMPlayerController>(ActorInfo->PlayerController.Get()) : nullptr;
	ALMBaseCharacter* Pawn = ActorInfo ? Cast<ALMBaseCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	if (!Controller || !Pawn)
	{
		StopFocus();
		return;
	}

	// 현재 프로젝트는 Pawn::PossessedBy 안에서 기본 GA를 부여한다.
	// 이 시점에는 Pawn의 Controller는 설정됐지만 Controller::SetPawn은 아직 실행 전일 수 있다.
	if (Pawn->GetController() != Controller)
	{
		StopFocus();
		return;
	}

	// 0 이하 거리나 NaN·무한대가 공간 조회 함수에 들어가지 않도록 막는다.
	if (!FMath::IsFinite(FocusRange) || FocusRange <= 0.0f)
	{
		StopFocus();
		return;
	}

	if (!FMath::IsFinite(SwitchMargin) || SwitchMargin < 0.0f)
	{
		StopFocus();
		return;
	}

	FocusController = Controller;
	FocusPawn = Pawn;

	// Controller가 카메라 계산을 끝낸 뒤 Broadcast하면 UpdateFocus가 호출된다.
	// 일반 Task Tick은 사용하지 않으므로 같은 프레임에 검색이 두 번 실행되지 않는다.
	Controller->OnFocusFrame.AddUObject(this, &ThisClass::UpdateFocus);
}

// 현재 플레이 상태를 확인하고 이번 프레임의 후보와 선택을 다시 계산한다.
void ULMAbilityTaskAutoFocus::UpdateFocus()
{
	// 소유 GA가 이미 끝났다면 이 Task도 더 이상 결과를 전달하지 않는다.
	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		EndTask();
		return;
	}

	ALMPlayerController* Controller = FocusController.Get();
	ALMBaseCharacter* Pawn = FocusPawn.Get();

	// 약한 참조가 null이면 캐릭터 또는 Controller가 제거된 상태다.
	if (!Controller || !Pawn)
	{
		StopFocus();
		return;
	}

	// Activate 이후 첫 카메라 갱신에서는 Controller::SetPawn까지 완료되어 있어야 한다.
	// 현재 조종하는 캐릭터나 GA의 Avatar가 달라졌다면 이전 Task를 끝낸다.
	if (Controller->GetPawn() != Pawn || Ability->GetAvatarActorFromActorInfo() != Pawn)
	{
		StopFocus();
		return;
	}

	// 해킹·연출·일시정지 또는 플레이어 사망 중에는 아무 대상도 선택하지 않는다.
	// Task를 끝내지는 않으므로 정상 상태로 돌아오면 다음 갱신에서 다시 검색한다.
	if (!Controller->CanUpdateFocus() || !Pawn->IsAliveForCombat())
	{
		Controller->ClearFocus();
		return;
	}

	// 공격 중에도 매번 새로 검색해야 시야·거리 밖으로 나간 대상이 후보에서 즉시 빠진다.
	TArray<TWeakObjectPtr<AActor>> Candidates;

	AActor* Target = ULMTargetQueryLibrary::FindFocusTargets(Controller, FocusRange, SwitchMargin, Controller->GetFocusTarget(), Candidates);

	// 이번 타가 진행 중이면 화면 중앙 점수로 다른 대상을 고르지 않는다.
	if (Controller->IsAttackFocusActive())
	{
		Target = Controller->GetAttackFocusTarget();

		// 기존 공격 대상도 이번 검색을 통과하지 못하면 유지하지 않는다.
		if (!Target || !Candidates.Contains(TWeakObjectPtr<AActor>(Target)))
			Target = nullptr;
	}

	// 후보 목록과 표식 대상을 함께 갱신한다.
	Controller->SetFocusResult(Candidates, Target);
}

// Task가 더 진행할 수 없음을 GA에 알리고 Task를 종료한다.
void ULMAbilityTaskAutoFocus::StopFocus()
{
	// GA가 아직 실행 중일 때만 GA의 OnFocusStopped를 호출한다.
	if (ShouldBroadcastAbilityTaskDelegates())
		OnStopped.Broadcast();

	// 위 알림을 받은 GA가 먼저 이 Task를 끝냈을 수도 있으므로 상태를 확인한다.
	if (!IsFinished())
		EndTask();
}

// Task 종료 후 카메라 알림이 이 객체를 다시 호출하지 않도록 연결을 제거한다.
void ULMAbilityTaskAutoFocus::OnDestroy(bool bInOwnerFinished)
{
	ALMPlayerController* Controller = FocusController.Get();

	// ClearFocus가 다른 이벤트를 부를 수 있으므로 내부 참조부터 비운다.
	// 정리 중 같은 함수가 다시 호출돼도 같은 Controller를 반복 정리하지 않게 한다.
	FocusController.Reset();
	FocusPawn.Reset();

	if (Controller)
	{
		// 이 Task가 등록했던 카메라 갱신 콜백만 제거한다.
		Controller->OnFocusFrame.RemoveAll(this);

		// 마지막으로 선택했던 대상과 화면 표식이 남지 않게 한다.
		Controller->ClearFocus();
	}

	// 엔진이 관리하는 Task 등록과 소유 GA 연결도 정리한다.
	Super::OnDestroy(bInOwnerFinished);
}