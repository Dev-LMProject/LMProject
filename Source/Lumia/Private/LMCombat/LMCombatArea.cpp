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
	constexpr float PlaceMove = 80.0f;
	constexpr float PlaceAngles[] = {0.0f, 30.0f, -30.0f, 60.0f, -60.0f};
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
	WaitOffsets.Reset();
	AttackQueue.Reset();
	const FVector EncounterPlayerPos = TargetPlayer->GetNavAgentLocation();

	// 복귀 중인 몬스터도 새 교전 대상을 받도록 고정 명단의 생존 몬스터를 큐에 넣는다.
	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsQueueMember(Monster))
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
	WaitOffsets.Reset();

	for (FLMAttackTurn& Turn : ActiveAttacks)
		Turn.bStopping = true;

	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsValid(Monster))
			Monster->SetTargetPlayer(this, nullptr);
	}
}

// 대기 위치 또는 허가자와 간격을 둔 공격 후보를 NavMesh에 보정해 반환함.
bool ALMCombatArea::ReadCombatLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location) const
{
    Location = IsValid(Monster) ? Monster->GetNavAgentLocation() : FVector::ZeroVector;

    if (bEndingPlay || !bIsSight || !IsValid(TargetPlayer) || TargetPlayer->IsActorBeingDestroyed() || !IsValid(CombatBounds))
        return false;
    if (!IsQueueMember(Monster) || Monster->GetTargetPlayer() != TargetPlayer)
        return false;

    const FVector* WaitOffset = WaitOffsets.Find(Monster);
    if (!WaitOffset) return false;

    const int32 OwnTurn = FindAttackTurn(Monster);
    if (bAttack && OwnTurn != INDEX_NONE && (ActiveAttacks[OwnTurn].CombatId != CombatId || ActiveAttacks[OwnTurn].bStopping))
        return false;

    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSystem) return false;

    const ANavigationData* NavData = NavSystem->GetNavDataForProps(Monster->GetNavAgentPropertiesRef(), Monster->GetNavAgentLocation());
    if (!NavData) return false;

    const FVector PlayerLocation = TargetPlayer->GetNavAgentLocation();
    const float PlayerRadius = TargetPlayer->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Radius = Monster->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float InnerDistance = FMath::Max(AttackDistance, PlayerRadius + Radius + PlaceReach + PlaceShift + PlaceGap);
    const int32 TryCount = bAttack && OwnTurn == INDEX_NONE ? UE_ARRAY_COUNT(PlaceAngles) : 1;

    for (int32 Try = 0; Try < TryCount; ++Try)
    {
        FVector Offset = *WaitOffset;
        if (bAttack)
            Offset = OwnTurn != INDEX_NONE ? ActiveAttacks[OwnTurn].AttackOffset : WaitOffset->GetSafeNormal2D().RotateAngleAxis(PlaceAngles[Try], FVector::UpVector) * InnerDistance;

        const FVector Candidate = PlayerLocation + Offset;
        FNavLocation Projected;

        // 좁은 통로에서 후보들이 같은 점으로 몰리지 않도록 보정 거리를 제한함.
        if (!NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(PlaceShift, PlaceShift, PlaceHeight), NavData))
            continue;
        if (FVector::DistSquared2D(Candidate, Projected.Location) > FMath::Square(PlaceShift) || FMath::Abs(Candidate.Z - Projected.Location.Z) > PlaceHeight)
            continue;

        const FVector Local = CombatBounds->GetComponentTransform().InverseTransformPosition(Projected.Location);
        const FVector Extent = CombatBounds->GetUnscaledBoxExtent();

        if (FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || FMath::Abs(Local.Z) > Extent.Z)
            continue;

        bool bFree = true;
        if (!bAttack)
        {
            // 간보기 이동과 도착 오차를 포함한 대기 구역 간격을 유지함.
            if (FVector::Dist2D(Projected.Location, PlayerLocation) < CurrentWaitDistance - PlaceShift)
                continue;

            for (const auto& Pair : WaitOffsets)
            {
                ALMFieldMonsterCharacter* Other = Pair.Key.Get();
                if (Other == Monster || !IsQueueMember(Other)) continue;

                const float OtherRadius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
                const float Gap = Radius + OtherRadius + 2.0f * (PlaceReach + PlaceShift + PlaceMove) + PlaceGap;

                if (FVector::DistSquared2D(Projected.Location, PlayerLocation + Pair.Value) < FMath::Square(Gap))
                {
                    bFree = false;
                    break;
                }
            }
        }
        else
        {
            // 종료 중인 옛 허가는 현재 몸체 위치를 기준으로 공간을 남김.
            if (FVector::Dist2D(Projected.Location, PlayerLocation) < PlayerRadius + Radius + PlaceReach + PlaceGap)
                continue;

            for (const FLMAttackTurn& Turn : ActiveAttacks)
            {
                ALMFieldMonsterCharacter* Other = Turn.Monster.Get();
                if (!IsValid(Other) || Other == Monster) continue;

                const float OtherRadius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
                const FVector OtherLocation = Turn.CombatId == CombatId && !Turn.bStopping ? PlayerLocation + Turn.AttackOffset : Other->GetNavAgentLocation();
                const float Gap = Radius + OtherRadius + 2.0f * (PlaceReach + PlaceShift) + PlaceGap;

                if (FVector::DistSquared2D(Projected.Location, OtherLocation) < FMath::Square(Gap))
                {
                    bFree = false;
                    break;
                }
            }
        }

        if (!bFree) continue;

        Location = Projected.Location;
        return true;
    }

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
	WaitOffsets.Reset();

	if (!IsValid(TargetPlayer) || !IsValid(CombatBounds)) return;
	
	const FVector PlayerLocation = TargetPlayer->GetNavAgentLocation();
	const FTransform BoundsTransform = CombatBounds->GetComponentTransform();
	const FVector BoundsExtent = CombatBounds->GetUnscaledBoxExtent();
	
	float MaxRadius = 0.0f;

	// 이번 큐의 참가자만 사용하고 ASC 준비 전 참가자도 자리를 보존함.
	for (const FLMQueueEntry& Entry : AttackQueue)
	{
		ALMFieldMonsterCharacter* Monster = Entry.Monster.Get();
		if (IsQueueMember(Monster))
			MaxRadius = FMath::Max(MaxRadius, Monster->GetCapsuleComponent()->GetScaledCapsuleRadius());
	}

	const float PlayerRadius = TargetPlayer->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float InnerDistance = FMath::Max(AttackDistance, PlayerRadius + MaxRadius + PlaceReach + PlaceShift + PlaceGap);

	// 지연된 Nav 보정 이후에도 간격 검사에 막히지 않도록 여유를 더함.
	const float Spacing = 2.0f * MaxRadius + 2.0f * (PlaceReach + PlaceShift + PlaceMove) + PlaceGap + PlaceShift;
	CurrentWaitDistance = FMath::Max(WaitDistance, InnerDistance + Spacing);
	

	for (const FLMQueueEntry& Entry : AttackQueue)
	{
		ALMFieldMonsterCharacter* Monster = Entry.Monster.Get();
		if (!IsQueueMember(Monster)) continue;

		FVector Direction = (Monster->GetNavAgentLocation() - PlayerLocation).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
			Direction = Monster->GetActorForwardVector().GetSafeNormal2D();
		if (Direction.IsNearlyZero())
			Direction = FVector::ForwardVector;

		const float BaseDistance = FMath::Max(CurrentWaitDistance, FVector::Dist2D(Monster->GetNavAgentLocation(), PlayerLocation));
		FVector FallbackOffset = FVector::ZeroVector;
		bool bHasFallback = false;
		bool bAssigned = false;
		
		for (int32 depth = 0; depth < 3 && !bAssigned; ++depth)
		{
			for (float Angle : PlaceAngles)
			{
				const FVector Offset = Direction.RotateAngleAxis(Angle, FVector::UpVector) * (BaseDistance + depth * Spacing);
				const FVector Candidate = PlayerLocation + Offset;
				const FVector Local = BoundsTransform.InverseTransformPosition(Candidate);

				if (FMath::Abs(Local.X) > BoundsExtent.X || FMath::Abs(Local.Y) > BoundsExtent.Y || FMath::Abs(Local.Z) > BoundsExtent.Z)
					continue;

				bool bFree = true;
				for (const auto& Pair : WaitOffsets)
				{
					if (FVector::DistSquared2D(Offset, Pair.Value) < FMath::Square(Spacing))
					{
						bFree = false;
						break;
					}
				}
				if (!bFree) continue;

				// Nav 준비가 늦어도 안전 간격을 확보한 후보를 보존함.
				if (!bHasFallback)
				{
					FallbackOffset = Offset;
					bHasFallback = true;
				}

				WaitOffsets.Add(Monster, Offset);
				FVector Projected;
				if (ReadCombatLocation(Monster, false, Projected))
				{
					WaitOffsets[Monster] = Projected - PlayerLocation;
					bAssigned = true;
					break;
				}
				WaitOffsets.Remove(Monster);
			}
		}

		if (!bAssigned && bHasFallback)
			WaitOffsets.Add(Monster, FallbackOffset);
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

		// 유효한 공격 위치가 있는 몬스터에게만 허가를 배정함.
		FVector AttackLocation;
		if (!ReadCombatLocation(Monster, true, AttackLocation))
		{
			++Index;
			continue;
		}

		// 허가와 함께 공격 위치를 확정해 다른 허가자의 자리 계산에 사용함.
		ActiveAttacks.Add({Entry.Monster, CombatId, ++NextAttackId, false, AttackLocation - TargetPlayer->GetNavAgentLocation()});
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

