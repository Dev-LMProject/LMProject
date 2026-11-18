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
#include "Navigation/PathFollowingComponent.h"

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
	WaitSlots.Reset();
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

	// 대상 전달을 마친 뒤 대기 위치 배정과 공격 허가를 진행함.
	SetCombatPlaces();
	FillAttackSlots();
}

// 실제 종료 대기를 보존하여 빠른 재입장에도 이전 공격과 정원이 겹치지 않게 한다.
void ALMCombatArea::EndCombat()
{
	bIsSight = false;
	TargetPlayer = nullptr;
	AttackQueue.Reset();
	WaitSlots.Reset();

	for (FLMAttackTurn& Turn : ActiveAttacks)
		Turn.bStopping = true;

	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsValid(Monster))
			Monster->SetTargetPlayer(this, nullptr);
	}
}

// 배정된 슬롯을 읽고 무효해졌으면 같은 링의 빈 슬롯으로 옮긴다.
bool ALMCombatArea::ReadCombatLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location)
{
	Location = IsValid(Monster) ? Monster->GetNavAgentLocation() : FVector::ZeroVector;
	if (bEndingPlay || !bIsSight || !IsValid(TargetPlayer) || TargetPlayer->IsActorBeingDestroyed() || !IsValid(CombatBounds))
		return false;
	if (!IsQueueMember(Monster) || Monster->GetTargetPlayer() != TargetPlayer)
		return false;

	if (bAttack)
	{
		// 내부 링은 현재 교전의 유효한 공격권 소유자만 사용한다.
		const int32 TurnIndex = FindAttackTurn(Monster);
		if (TurnIndex == INDEX_NONE) return false;

		FLMAttackTurn& Turn = ActiveAttacks[TurnIndex];
		if (Turn.CombatId != CombatId || Turn.bStopping)
			return false;
		if (GetSlotLocation(Monster, true, Turn.SlotIndex, Location))
			return true;

		// 기존 자리가 무효일 때만 다른 빈 자리를 찾는다.
		const int32 NewSlot = FindFreeSlot(Monster, true, Location);
		if (NewSlot == INDEX_NONE) return false;

		Turn.SlotIndex = NewSlot;
		return true;
	}

	const int32* SavedSlot = WaitSlots.Find(Monster);
	if (SavedSlot && GetSlotLocation(Monster, false, *SavedSlot, Location))
		return true;

	const int32 NewSlot = FindFreeSlot(Monster, false, Location);
	if (NewSlot == INDEX_NONE) return false;

	WaitSlots.Add(Monster, NewSlot);
	return true;
}

// 플레이어 위치에 슬롯 방향과 링 반경을 더하고 NavMesh 위의 유효한 위치를 반환한다.
bool ALMCombatArea::GetSlotLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, int32 SlotIndex, FVector& Location) const
{
    Location = IsValid(Monster) ? Monster->GetNavAgentLocation() : FVector::ZeroVector;
    if (bEndingPlay || !bIsSight || !IsValid(TargetPlayer) || TargetPlayer->IsActorBeingDestroyed() || !IsValid(CombatBounds))
    {
        return false;
    }
    if (!IsQueueMember(Monster) || Monster->GetTargetPlayer() != TargetPlayer)
    {
        return false;
    }

    // 슬롯 개수와 번호를 확인한 뒤 링을 같은 각도로 나눈다.
    const int32 Count = bAttack ? AttackSlotCount : WaitSlotCount;
    const float Radius = bAttack ? AttackDistance : WaitDistance;
    if (Count <= 0 || SlotIndex < 0 || SlotIndex >= Count || Radius <= 0.0f)
    {
        return false;
    }
    const float Angle = 360.0f * SlotIndex / Count;
    const FVector Direction = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector);
    const FVector Candidate = TargetPlayer->GetNavAgentLocation() + Direction * Radius;

    // 몬스터와 같은 Agent 설정으로 NavMesh 위의 위치를 찾는다.
    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Projected;
    if (!NavSystem || !NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(PlaceShift, PlaceShift, PlaceHeight), &Monster->GetNavAgentPropertiesRef()))
    {
        return false;
    }
    if (FVector::DistSquared2D(Candidate, Projected.Location) > FMath::Square(PlaceShift) || FMath::Abs(Candidate.Z - Projected.Location.Z) > PlaceHeight)
    {
        return false;
    }

    // 전투 영역 밖의 슬롯은 반환하지 않아 다른 빈 슬롯을 선택할 수 있게 한다.
    const FVector Local = CombatBounds->GetComponentTransform().InverseTransformPosition(Projected.Location);
    const FVector Extent = CombatBounds->GetUnscaledBoxExtent();
    if (FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || FMath::Abs(Local.Z) > Extent.Z)
    {
        return false;
    }
    Location = Projected.Location;
    return true;
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
// 최초 거리순 큐의 몬스터에게 서로 다른 외부 복귀 자리를 예약한다.
void ALMCombatArea::SetCombatPlaces()
{
	WaitSlots.Reset();

	for (const FLMQueueEntry& Entry : AttackQueue)
	{
		ALMFieldMonsterCharacter* Monster = Entry.Monster.Get();
		FVector WaitLocation;
		const int32 NewSlot = FindFreeSlot(Monster, false, WaitLocation);
		if (NewSlot != INDEX_NONE)
			WaitSlots.Add(Monster, NewSlot);
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
	const TWeakObjectPtr<ALMFieldMonsterCharacter> OwnerMonster(Monster);
	return ActiveAttacks.IndexOfByPredicate([&OwnerMonster](const FLMAttackTurn& Turn) { return Turn.Monster.HasSameIndexAndSerialNumber(OwnerMonster); });
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

		// 공격 중에도 돌아올 외부 자리를 예약하며 자리가 없으면 이번 항목은 기다린다.
		FVector WaitLocation;
		if (!ReadCombatLocation(Monster, false, WaitLocation))
		{
			++Index;
			continue;
		}
		// 다시 줄을 선 몬스터는 바깥 위치에 도착해야 재허가한다.
		if (Entry.WaitId > 0 && !IsAtPlace(Monster, WaitLocation))
		{
			++Index;
			continue;
		}
		
		FVector AttackLocation;
		const int32 AttackSlot = FindFreeSlot(Monster, true, AttackLocation);
		if (AttackSlot == INDEX_NONE)
		{
			++Index;
			continue;
		}

		// 내부 슬롯과 공격권을 같은 기록에 확정한 뒤 큐에서 꺼낸다.
		ActiveAttacks.Add({Entry.Monster, CombatId, ++NextAttackId, false, AttackSlot});
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
	if (AttackQueue[Index].bWaitRequired == !bReady) return;
	AttackQueue[Index].bWaitRequired = !bReady;
	FillAttackSlots();
}

// 같은 링의 빈 유효 슬롯 중 현재 몬스터와 가장 가까운 번호를 찾는다.
int32 ALMCombatArea::FindFreeSlot(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location) const
{
	Location = IsValid(Monster) ? Monster->GetNavAgentLocation() : FVector::ZeroVector;
	if (!IsQueueMember(Monster)) return INDEX_NONE;

	const int32 SlotCount = bAttack ? AttackSlotCount : WaitSlotCount;
	int32 BestSlot = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();

	for (int32 CandidateSlot = 0; CandidateSlot < SlotCount; ++CandidateSlot)
	{
		bool bUsed = false;
		if (bAttack)
		{
			// 중단 중인 기록도 실제 종료 전까지 내부 자리를 점유한다.
			for (const FLMAttackTurn& Turn : ActiveAttacks)
			{
				if (Turn.Monster != Monster && Turn.SlotIndex == CandidateSlot)
				{
					bUsed = true;
					break;
				}
			}
		}
		else
		{
			// 죽거나 소속을 잃은 몬스터의 외부 예약은 재사용한다.
			for (const auto& Pair : WaitSlots)
			{
				ALMFieldMonsterCharacter* Other = Pair.Key.Get();
				if (Other != Monster && IsQueueMember(Other) && Other->GetTargetPlayer() == TargetPlayer && Pair.Value == CandidateSlot)
				{
					bUsed = true;
					break;
				}
			}
		}

		FVector CandidateLocation;
		if (bUsed || !GetSlotLocation(Monster, bAttack, CandidateSlot, CandidateLocation))
			continue;

		const double CandidateDistance = FVector::DistSquared2D(Monster->GetNavAgentLocation(), CandidateLocation);
		if (CandidateDistance < BestDistance)
		{
			BestDistance = CandidateDistance;
			BestSlot = CandidateSlot;
			Location = CandidateLocation;
		}
	}
	return BestSlot;
}

// 경로 추적 컴포넌트의 도착 판정을 그대로 재사용한다.
bool ALMCombatArea::IsAtPlace(ALMFieldMonsterCharacter* Monster, const FVector& Location) const
{
	if (!IsValid(Monster)) return false;

	const AAIController* MonsterController = Cast<AAIController>(Monster->GetController());
	if (!IsValid(MonsterController)) return false;

	const UPathFollowingComponent* PathFollowing = MonsterController->GetPathFollowingComponent();


	// ExactLocation은 도착 반경에 캐릭터 반경을 추가하지 않는다.
	//HasReached => 엔진의 도착 검사.
	return IsValid(PathFollowing) && PathFollowing->HasReached(Location, EPathFollowingReachMode::ExactLocation, MoveReach);
}