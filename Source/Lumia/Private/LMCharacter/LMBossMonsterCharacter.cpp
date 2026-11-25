#include "LMCharacter/LMBossMonsterCharacter.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameplayAbilitySpec.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMCombat/LMAttackAbility.h"
#include "LMCombat/LMCombatAttributeSet.h"

// 배치되거나 스폰된 시길룸에 AI가 빙의하도록 하고 몸체 수명의 전투 객체를 생성한다.
ALMBossMonsterCharacter::ALMBossMonsterCharacter()
{
	CombatTeam = ELMCombatTeam::Enemy;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	BossASC = CreateDefaultSubobject<ULMAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	BossASC->SetIsReplicated(false);
	BossAttributes = CreateDefaultSubobject<ULMCombatAttributeSet>(TEXT("CombatAttributes"));
}

// 공통 ASC 연결이 끝나면 보스 BP가 지정한 능력을 부여하고 준비를 알린다.
void ALMBossMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem(this, BossASC.Get());
	if (!HasAuthority()) return;

	// 몸체의 속성이 연결되고 공격 세트가 지정된 경우에만 전투를 시작한다.
	const bool bHasOwnAttributes = IsValid(BossASC.Get()) && IsValid(BossAttributes.Get()) && GetAbilitySystemComponent() == BossASC.Get() && BossASC->GetSet<ULMCombatAttributeSet>() == BossAttributes.Get();
	if (!ensureMsgf(bHasOwnAttributes, TEXT("%s: Combat attributes are unavailable."), *GetName()))
		return;
	if (!ensureMsgf(IsValid(AttackSet.Get()), TEXT("%s: Assign an Attack Set in the boss Blueprint."), *GetName()))
		return;

	AttackSet->GiveAbilities(BossASC.Get(), this, GrantedAttacks);
	bCombatReady = !GrantedAttacks.Handles.IsEmpty();
	if (bCombatReady)
		OnCombatReady();
}

// 실제 공격 거리와 시야를 확인하고 시길룸의 공격 세트에서 요청한 능력을 시작한다.
int64 ALMBossMonsterCharacter::TryStartMelee(ALMPlayerCharacter* _TargetPlayer, TSubclassOf<ULMAttackAbility> _AttackClass)
{
	if (!bCombatReady || ActiveAttack.IsValid() || !_AttackClass || !IsValid(_TargetPlayer))
		return 0;
	if (!_TargetPlayer->CanBeAttackTargetFor(this) || IsMovementBlockedByAbilities() || IsMovementControlledByAbilities())
		return 0;

	// 수평 거리는 캡슐 중심으로, 단차는 발 높이로 비교해 체격 차이를 제외한다.
	const FVector BossCenter = GetActorLocation();
	const FVector TargetCenter = _TargetPlayer->GetActorLocation();
	const double BossFoot = BossCenter.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const double TargetFoot = TargetCenter.Z - _TargetPlayer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (FVector::DistSquared2D(BossCenter, TargetCenter) > FMath::Square(MeleeRange) || FMath::Abs(BossFoot - TargetFoot) > 100.0)
		return 0;

	const AAIController* BossController = Cast<AAIController>(GetController());
	if (!IsValid(BossController) || !BossController->LineOfSightTo(_TargetPlayer))
		return 0;

	// 다른 출처가 같은 GA를 부여해도 이번 공격 세트의 핸들만 사용한다.
	FGameplayAbilitySpecHandle StartedHandle;
	for (const FGameplayAbilitySpecHandle GrantedHandle : GrantedAttacks.Handles)
	{
		const FGameplayAbilitySpec* AttackSpec = BossASC->FindAbilitySpecFromHandle(GrantedHandle);
		if (AttackSpec && AttackSpec->Ability && AttackSpec->Ability->GetClass() == _AttackClass.Get() && !AttackSpec->IsActive())
		{
			StartedHandle = GrantedHandle;
			break;
		}
	}
	if (!StartedHandle.IsValid()) return 0;

	const int64 StartedRun = ++MeleeRun;
	ActiveAttack = StartedHandle;
	MeleeResult = ELMBossMonsterMeleeResult::Running;

	// 활성화 호출 안에서 몽타주가 즉시 종료돼도 결과를 보존하도록 먼저 구독한다.
	const auto OnEnded = [this, StartedRun, StartedHandle](const FAbilityEndedData& Ended)
	{
		if (Ended.AbilitySpecHandle == StartedHandle)
			FinishMelee(StartedRun, Ended.bWasCancelled ? ELMBossMonsterMeleeResult::Failed : ELMBossMonsterMeleeResult::Succeeded);
	};
	EndedHandle = BossASC->OnAbilityEnded.AddWeakLambda(this, OnEnded);
	if (!BossASC->TryActivateAbility(StartedHandle, false))
	{
		FinishMelee(StartedRun, ELMBossMonsterMeleeResult::Failed);
		return 0;
	}
	return StartedRun;
}

// 이전 Task가 다음 공격의 진행 상태를 읽지 않도록 실행 번호를 대조한다.
ELMBossMonsterMeleeResult ALMBossMonsterCharacter::ReadMeleeResult(int64 _Run) const
{
	return _Run > 0 && _Run == MeleeRun ? MeleeResult : ELMBossMonsterMeleeResult::Failed;
}

// 현재 공격의 종료 구독을 해제하고 해당 Task가 읽을 결과를 남긴다.
void ALMBossMonsterCharacter::FinishMelee(int64 _Run, ELMBossMonsterMeleeResult _Result)
{
	if (_Run != MeleeRun || !ActiveAttack.IsValid()) return;

	BossASC->OnAbilityEnded.Remove(EndedHandle);
	EndedHandle.Reset();
	ActiveAttack = FGameplayAbilitySpecHandle();
	MeleeResult = _Result;
}

// 같은 실행만 취소하며 GAS의 동기 종료 알림이 결과를 다시 쓰지 않게 한다.
void ALMBossMonsterCharacter::StopMelee(int64 _Run)
{
	if (_Run != MeleeRun || !ActiveAttack.IsValid()) return;

	const FGameplayAbilitySpecHandle HandleToStop = ActiveAttack;
	FinishMelee(_Run, ELMBossMonsterMeleeResult::Failed);
	BossASC->CancelAbilityHandle(HandleToStop);
}

// 몸체가 끝날 때 새 공격을 막고 부여한 능력을 회수한 뒤 부모의 ASC 연결을 해제한다.
void ALMBossMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bCombatReady = false;
	StopMelee(MeleeRun);
	GrantedAttacks.RemoveFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}
