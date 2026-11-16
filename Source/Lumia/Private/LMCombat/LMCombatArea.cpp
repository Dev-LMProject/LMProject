// Fill out your copyright notice in the Description page of Project Settings.


#include "LMCombat/LMCombatArea.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float PlaceReach = 100.0f;
	constexpr float PlaceShift = 40.0f;
	constexpr float PlaceGap = 50.0f;
	constexpr float PlaceHeight = 150.0f;
}

// Sets default values
ALMCombatArea::ALMCombatArea()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	AreaRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AreaRoot"));
	SetRootComponent(AreaRoot);

	DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
	DetectionBox->SetupAttachment(AreaRoot);

	CombatBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CombatBounds"));
	CombatBounds->SetupAttachment(AreaRoot);
	
	 // 실제 크기와 위치는 BP에서 전투 공간에 맞춰 조정한다.
        DetectionBox->InitBoxExtent(FVector(1200.0f, 1200.0f, 400.0f));
        CombatBounds->InitBoxExtent(FVector(1800.0f, 1800.0f, 600.0f));
	
	for (UBoxComponent* Box : {DetectionBox.Get(), CombatBounds.Get()})
	{
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldDynamic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Box->SetCollisionResponseToChannel(ECC_EngineTraceChannel4, ECR_Overlap);
		
		Box->SetGenerateOverlapEvents(true);
		Box->SetCanEverAffectNavigation(false);
	}
}

void ALMCombatArea::RegisterMonsters()
{
	if (bEndingPlay||bRegistered)return;
	
	// BP BeginPlay가 엔진의 초기 Overlap 갱신보다 먼저 올 수 있어 한 번 확정한다.
	CombatBounds->UpdateOverlaps();
	DetectionBox->UpdateOverlaps();
	
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	DetectionBox->GetOverlappingComponents(PrimitiveComponents);
	ALMPlayerCharacter* InitPlayer=nullptr;
	for (auto Comp:PrimitiveComponents)
	{
		if (!IsValid(Comp))continue;
		
		if (ALMFieldMonsterCharacter* Monster=Cast<ALMFieldMonsterCharacter>(Comp->GetOwner()))
		{
			if (Comp==Monster->GetCapsuleComponent()&& Monster->RegisterCombatArea(this))
			{
				RegisteredMonsters.AddUnique(Monster);
			}
		}
		else if (ALMPlayerCharacter* Player=Cast<ALMPlayerCharacter>(Comp->GetOwner()))
		{
			if (Comp==Player->GetCapsuleComponent())InitPlayer=Player;
		}
	}
	// 전체 명단이 완성된 뒤부터 플레이어 이벤트 처리를 허용한다.
	bRegistered=true;
	if (IsValid(InitPlayer))HandlePlayerOverlap(InitPlayer,InitPlayer->GetCapsuleComponent());
}

void ALMCombatArea::HandlePlayerOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp)
{
	if (bEndingPlay||!bRegistered) return;
	ALMPlayerCharacter* Player=Cast<ALMPlayerCharacter>(OtherActor);
	
	if (!IsValid(Player)||OtherComp!=Player->GetCapsuleComponent()) return;
	
	// 퇴장 경계를 나간 경우에만 진행 중인 그룹 전투를 종료한다.
	if (!IsInside(CombatBounds,Player))
	{
		if (TargetPlayer==Player)EndCombat();
		return;
	}
	
	// 최초 발견 후에는 감지 박스 이탈로 대상을 해제하지 않는다.
	if (bIsSight)return;
	
	if (!IsInside(DetectionBox,Player))
	{
		if (TargetPlayer==Player)TargetPlayer=nullptr;
		return;
	}
	if (!Player->IsPlayerControlled())return;
	
	TargetPlayer=Player;
	for (ALMFieldMonsterCharacter* Observer: RegisteredMonsters)
	{
		if (CanSeePlayer(Observer,Player))
		{
			BeginCombat();
			return;
		}
	}
}
// 시야 성공 이벤트도 실제 후보와 현재 Sight를 확인한 뒤에만 인정한다.
void ALMCombatArea::ReportPlayerSeen(ALMFieldMonsterCharacter* Observer, AActor* Player, bool bSuccessfullySensed)
{
    if (bEndingPlay || !bRegistered || bIsSight || !bSuccessfullySensed)
    {
        return;
    }

    ALMPlayerCharacter* SeenPlayer = Cast<ALMPlayerCharacter>(Player);

    if (!IsValid(SeenPlayer) || TargetPlayer != SeenPlayer)
    {
        return;
    }

    if (!IsInside(DetectionBox, SeenPlayer) || !IsInside(CombatBounds, SeenPlayer))
    {
        return;
    }

    if (CanSeePlayer(Observer, SeenPlayer))
        BeginCombat();
}

// 감지용 메시나 센서가 아닌 플레이어 본체의 겹침만 인정한다.
bool ALMCombatArea::IsInside(const UBoxComponent* Box, const ALMPlayerCharacter* Player) const
{
    return IsValid(Box) && IsValid(Player) && !Player->IsActorBeingDestroyed() && Box->IsOverlappingComponent(Player->GetCapsuleComponent());
}

// 이벤트 시점의 현재 Sight를 조회하며 ASC 준비를 전제하지 않는다.
bool ALMCombatArea::CanSeePlayer(ALMFieldMonsterCharacter* Observer, ALMPlayerCharacter* Player) const
{
    if (!IsValid(Observer) || !IsValid(Player)) return false;

    if (Observer->IsDead() || Observer->IsActorBeingDestroyed())
    {
        return false;
    }

    if (Observer->GetCombatArea() != this || !RegisteredMonsters.Contains(Observer))
    {
        return false;
    }

    AAIController* Controller = Cast<AAIController>(Observer->GetController());
    UAIPerceptionComponent* Perception = IsValid(Controller) ? Controller->FindComponentByClass<UAIPerceptionComponent>() : nullptr;

    if (!IsValid(Perception)) return false;

    TArray<AActor*> SeenActors;
    Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), SeenActors);

    return SeenActors.Contains(Player);
}

// 발견 상태를 먼저 확정하여 중복 Sight 보고가 전투를 다시 시작하지 않게 한다.
void ALMCombatArea::BeginCombat()
{
    if (bEndingPlay ||bIsSight || !IsValid(TargetPlayer)) return;

    bIsSight = true;
	++CombatId;
	PlaceDirections.Reset();
	AttackQueue.Reset();
	const FVector EncounterPlayerPos = TargetPlayer->GetNavAgentLocation();

	// 고정 명단 중 현재 전투 박스 안의 생존 몬스터만 최초 큐에 넣는다.
	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsQueueMember(Monster) && CombatBounds->IsOverlappingComponent(Monster->GetCapsuleComponent()))
		{
			const int32 OldTurn = FindAttackTurn(Monster);
			AttackQueue.Add({Monster, OldTurn == INDEX_NONE ? 0 : ActiveAttacks[OldTurn].AttackId, OldTurn != INDEX_NONE});
		}
	}

	// 최초 한 번만 거리순으로 정렬하며 동점은 등록 순서를 유지한다.
	const auto IsCloser = [EncounterPlayerPos](const FLMQueueEntry& A, const FLMQueueEntry& B)
	{
		return FVector::DistSquared(A.Monster->GetNavAgentLocation(), EncounterPlayerPos) < FVector::DistSquared(B.Monster->GetNavAgentLocation(), EncounterPlayerPos);
	};
	AttackQueue.StableSort(IsCloser);

	// 전체 큐가 완성된 뒤 같은 대상을 전달한다.
	for (const FLMQueueEntry& Entry : AttackQueue)
		Entry.Monster->SetTargetPlayer(this, TargetPlayer);

	FillAttackSlots();
}

// 실제 종료 대기를 보존하여 빠른 재입장에도 이전 공격과 정원이 겹치지 않게 한다.
void ALMCombatArea::EndCombat()
{
	bIsSight = false;
	TargetPlayer = nullptr;
	AttackQueue.Reset();
	PlaceDirections.Reset();

	for (FLMAttackTurn& Turn : ActiveAttacks)
		Turn.bStopping = true;

	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsValid(Monster))
			Monster->SetTargetPlayer(this, nullptr);
	}
}

bool ALMCombatArea::ReadCombatLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location) const
{
	return false;
}

// Area 자체가 사라질 때는 신규 허가를 막고 자기 기록과 소속을 함께 정리한다.
void ALMCombatArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	EndCombat();

	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsValid(Monster))
			Monster->UnregisterCombatArea(this);
	}

	ActiveAttacks.Reset();
	RegisteredMonsters.Reset();

	Super::EndPlay(EndPlayReason);
}

// 같은 교전 동안 유지할 방향과 안전한 바깥 대기 반경을 정한다.
void ALMCombatArea::SetCombatPlaces()
{
	PlaceDirections.Reset();

	TArray<ALMFieldMonsterCharacter*> Members;
	float MaxRadius = 0.0f;

	// ASC 준비가 늦어도 소속이 맞는 살아 있는 몬스터의 자리를 확보한다.
	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (!IsValid(Monster) || Monster->IsActorBeingDestroyed() || Monster->IsDead() || Monster->GetCombatArea() != this)
			continue;

		Members.Add(Monster);
		MaxRadius = FMath::Max(MaxRadius, Monster->GetCapsuleComponent()->GetScaledCapsuleRadius());
	}

	const int32 Count = Members.Num();
	if (Count == 0) return;

	// 도착 오차와 작은 NavMesh 보정을 포함해 서로의 공간을 남긴다.
	const float PlayerRadius = TargetPlayer->GetCapsuleComponent()->GetScaledCapsuleRadius();
	CurrentWaitDistance = FMath::Max(WaitDistance, PlayerRadius + MaxRadius + PlaceReach + PlaceShift + PlaceGap);

	if (Count > 1)
	{
		const float Spacing = 2.0f * MaxRadius + 2.0f * PlaceReach + 2.0f * PlaceShift + PlaceGap;
		CurrentWaitDistance = FMath::Max(CurrentWaitDistance, Spacing / (2.0f * FMath::Sin(PI / Count)));
	}

	// 플레이어가 회전해도 방향은 바뀌지 않으며 새 교전에서만 다시 배정한다.
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = 2.0f * PI * Index / Count;
		PlaceDirections.Add(Members[Index], FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f));
	}
}
// ASC 준비 전에도 참가 자격은 유지하고 실제 허가 시점에 행동 가능 여부를 검사한다.
bool ALMCombatArea::IsQueueMember(ALMFieldMonsterCharacter* Monster) const
{
	return IsValid(Monster) && !Monster->IsActorBeingDestroyed() && !Monster->IsDead() && Monster->GetCombatArea() == this && RegisteredMonsters.Contains(Monster);
}

// 같은 몬스터의 새 허가를 만들기 전에 옛 종료 대기까지 확인한다.
int32 ALMCombatArea::FindAttackTurn(ALMFieldMonsterCharacter* Monster) const
{
	const TWeakObjectPtr<ALMFieldMonsterCharacter> Owner(Monster);
	return ActiveAttacks.IndexOfByPredicate([&Owner](const FLMAttackTurn& Turn) { return Turn.Monster.HasSameIndexAndSerialNumber(Owner); });
}

// 준비되지 않은 항목의 순서는 보존하고 현재 가능한 몬스터로 빈 정원만 채운다.
void ALMCombatArea::FillAttackSlots()
{
	if (bEndingPlay || !bIsSight) return;

	if (!IsValid(TargetPlayer) || TargetPlayer->IsActorBeingDestroyed())
	{
		EndCombat();
		return;
	}

	if (!TargetPlayer->IsAliveForCombat()) return;

	for (int32 Index = 0; Index < AttackQueue.Num() && ActiveAttacks.Num() < FMath::Max(1, MaxAttackers);)
	{
		const FLMQueueEntry Entry = AttackQueue[Index];
		ALMFieldMonsterCharacter* Monster = Entry.Monster.Get();

		if (!IsQueueMember(Monster) || Monster->GetTargetPlayer() != TargetPlayer)
		{
			AttackQueue.RemoveAt(Index);
			continue;
		}

		ALMPlayerCharacter* Player = nullptr;
		FVector StartLocation;
		bool bReturning = false;
		bool bCanMove = false;
		const bool bHasTarget = Monster->ReadMoveData(Player, StartLocation, bReturning, bCanMove);

		// 이동 준비만 연결한다. 공격 후보 기하와 정확한 GA 시작 조건은 다음 단위에서 덧붙인다.
		if (Entry.bWaitRequired || FindAttackTurn(Monster) != INDEX_NONE || !bHasTarget || Player != TargetPlayer || bReturning || !bCanMove || !CombatBounds->IsOverlappingComponent(Monster->GetCapsuleComponent()))
		{
			++Index;
			continue;
		}

		ActiveAttacks.Add({Entry.Monster, CombatId, ++NextAttackId, false});
		AttackQueue.RemoveAt(Index);
	}
}
// 중단 중인 옛 교전 기록도 반환하므로 호출자는 bStopping과 교전 번호를 함께 판단한다.
bool ALMCombatArea::ReadAttackTurn(ALMFieldMonsterCharacter* Monster, int64& _CombatId, int64& AttackId, bool& bStopping, int64& WaitId) const
{
	_CombatId = CombatId;
	AttackId = 0;
	bStopping = false;
	WaitId = 0;

	if (!Monster) return false;

	for (const FLMQueueEntry& Entry : AttackQueue)
	{
		if (Entry.Monster == Monster)
			WaitId = Entry.WaitId;
	}

	const int32 Index = FindAttackTurn(Monster);
	if (Index == INDEX_NONE) return false;

	const FLMAttackTurn& Turn = ActiveAttacks[Index];
	_CombatId = Turn.CombatId;
	AttackId = Turn.AttackId;
	bStopping = Turn.bStopping;
	return true;
}
// 중단 요청만으로 허가를 회수하지 않고 정확한 완료 보고를 기다린다.
bool ALMCombatArea::RequestStopAttackTurn(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 AttackId)
{
	const int32 Index = FindAttackTurn(Monster);
	if (Index == INDEX_NONE) return false;

	FLMAttackTurn& Turn = ActiveAttacks[Index];
	if (Turn.CombatId != _CombatId || Turn.AttackId != AttackId) return false;

	Turn.bStopping = true;
	return true;
}

// 옛 콜백은 옛 기록만 지우고 현재 교전의 빈 정원은 다시 보충한다.
bool ALMCombatArea::FinishAttackTurn(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 AttackId, bool bRequeue)
{
	const int32 Index = FindAttackTurn(Monster);
	if (Index == INDEX_NONE) return false;

	const FLMAttackTurn& Turn = ActiveAttacks[Index];
	if (Turn.CombatId != _CombatId || Turn.AttackId != AttackId) return false;

	ActiveAttacks.RemoveAt(Index);

	// 재삽입 전에 복귀 조건을 잠가 바로 같은 몬스터가 재허가받지 않게 한다.
	if (!bEndingPlay && bIsSight && _CombatId == CombatId && bRequeue && IsQueueMember(Monster) && Monster->GetTargetPlayer() == TargetPlayer)
	{
		const bool bQueued = AttackQueue.ContainsByPredicate([Monster](const FLMQueueEntry& Entry) { return Entry.Monster == Monster; });

		if (!bQueued)
			AttackQueue.Add({Monster, AttackId, true});
	}

	FillAttackSlots();
	return true;
}

// 이전 교전이나 이전 대기 이동의 알림은 새 복귀 조건을 변경하지 못한다.
void ALMCombatArea::ReportWaitReady(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 WaitId, bool bReady)
{
	if (bEndingPlay || !bIsSight || _CombatId != CombatId || WaitId <= 0) return;

	const int32 Index = AttackQueue.IndexOfByPredicate([Monster, WaitId](const FLMQueueEntry& Entry) { return Entry.Monster == Monster && Entry.WaitId == WaitId; });
	if (Index == INDEX_NONE) return;

	AttackQueue[Index].bWaitRequired = !bReady;
	FillAttackSlots();
}

