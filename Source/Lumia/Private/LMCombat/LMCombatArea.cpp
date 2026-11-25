#include "LMCombat/LMCombatArea.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

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

// 유효한 전체 목록을 먼저 확인한 뒤 사망 태그 감지를 시작한다.
bool ALMCombatArea::WatchMobs(const TArray<ALMFieldMonsterCharacter*>& Mobs)
{
	if (bEndingPlay || bWatchingMobs || Mobs.IsEmpty() || !GetWorld())
	{
		return false;
	}

	TArray<UAbilitySystemComponent*> MobSystems;
	for (ALMFieldMonsterCharacter* Mob : Mobs)
	{
		// 잘못된 대상을 빼고 더 적은 수로 완료되는 일을 막는다.
		if (!IsValid(Mob) || Mob->IsActorBeingDestroyed() || Mob->GetWorld() != GetWorld())
		{
			return false;
		}

		UAbilitySystemComponent* MobSystem = Mob->GetAbilitySystemComponent();
		if (!IsValid(MobSystem) || MobSystem->GetAvatarActor() != Mob || MobSystems.Contains(MobSystem))
		{
			return false;
		}

		MobSystems.Add(MobSystem);
	}

	bWatchingMobs = true;

	// 먼저 모든 구독을 만든 뒤 이미 죽은 대상을 확인해 조기 완료를 막는다.
	for (UAbilitySystemComponent* MobSystem : MobSystems)
	{
		const TWeakObjectPtr<UAbilitySystemComponent> MobKey(MobSystem);
		const FDelegateHandle DeathHandle = MobSystem->RegisterGameplayTagEvent(LMGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ALMCombatArea::HandleMobDeath, MobKey);
		PendingMobDeaths.Add(MobKey, DeathHandle);
	}

	for (UAbilitySystemComponent* MobSystem : MobSystems)
		HandleMobDeath(LMGameplayTags::State_Dead, MobSystem->GetGameplayTagCount(LMGameplayTags::State_Dead), TWeakObjectPtr<UAbilitySystemComponent>(MobSystem));

	return true;
}

// 사망을 받은 대상은 목록에서 제거하여 같은 사망을 두 번 세지 않는다.
void ALMCombatArea::HandleMobDeath(FGameplayTag Tag, int32 TagCount, TWeakObjectPtr<UAbilitySystemComponent> MobKey)
{
	if (bEndingPlay || !bWatchingMobs || TagCount <= 0) return;
	if (Tag != LMGameplayTags::State_Dead) return;

	FDelegateHandle DeathHandle;
	if (!PendingMobDeaths.RemoveAndCopyValue(MobKey, DeathHandle))
	{
		return;
	}

	if (UAbilitySystemComponent* MobSystem = MobKey.Get())
		MobSystem->UnregisterGameplayTagEvent(DeathHandle, LMGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);

	// 사망 함수 안에서 연출을 시작하지 않도록 완료 알림만 한 번 예약한다.
	if (PendingMobDeaths.IsEmpty())
		MobsClearedTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ALMCombatArea::FinishMobWatch);
}

// 다음 감지를 시작할 수 있도록 현재 감지를 정리한 뒤 완료를 알린다.
void ALMCombatArea::FinishMobWatch()
{
	if (bEndingPlay || !bWatchingMobs || !PendingMobDeaths.IsEmpty())
	{
		return;
	}

	StopMobWatch();
	OnMobsCleared.Broadcast();
}

// 영역 종료나 정상 완료 때 이 영역이 만든 감지만 정리한다.
void ALMCombatArea::StopMobWatch()
{
	bWatchingMobs = false;

	if (UWorld* AreaWorld = GetWorld())
		AreaWorld->GetTimerManager().ClearTimer(MobsClearedTimer);
	MobsClearedTimer.Invalidate();

	for (const auto& DeathWatch : PendingMobDeaths)
	{
		if (UAbilitySystemComponent* MobSystem = DeathWatch.Key.Get())
			MobSystem->UnregisterGameplayTagEvent(DeathWatch.Value, LMGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
	}

	PendingMobDeaths.Reset();
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

	// 대상 전달을 마친 뒤 공격 정원을 채운다.
	FillAttackSlots();
}

// 실제 종료 대기를 보존하여 빠른 재입장에도 이전 공격과 정원이 겹치지 않게 한다.
void ALMCombatArea::EndCombat()
{
	bIsSight = false;
	TargetPlayer = nullptr;
	AttackQueue.Reset();

	for (FLMAttackTurn& Turn : ActiveAttacks)
		Turn.bStopping = true;

	for (ALMFieldMonsterCharacter* Monster : RegisteredMonsters)
	{
		if (IsValid(Monster))
			Monster->SetTargetPlayer(this, nullptr);
	}
}

// 공격 접근과 저속 추적에 사용할 플레이어 위치를 읽는다.
bool ALMCombatArea::ReadCombatLocation(ALMFieldMonsterCharacter* Monster, bool bAttack, FVector& Location)
{
	Location = IsValid(Monster) ? Monster->GetNavAgentLocation() : FVector::ZeroVector;
	if (bEndingPlay || !bIsSight || !IsValid(TargetPlayer) || TargetPlayer->IsActorBeingDestroyed() || !TargetPlayer->IsAliveForCombat())
		return false;
	if (!IsQueueMember(Monster) || Monster->GetTargetPlayer() != TargetPlayer)
		return false;

	if (bAttack)
	{
		// 안쪽 각도나 슬롯 없이 현재 교전의 유효한 공격권만 확인한다.
		const int32 TurnIndex = FindAttackTurn(Monster);
		if (TurnIndex == INDEX_NONE) return false;

		const FLMAttackTurn& Turn = ActiveAttacks[TurnIndex];
		if (Turn.CombatId != CombatId || Turn.bStopping)
			return false;
	}

	Location = TargetPlayer->GetNavAgentLocation();
	return true;
}

// 반납한 몬스터에게 해당 번호의 이격 이동이 아직 필요한지 확인한다.
bool ALMCombatArea::NeedsRetreat(const ALMFieldMonsterCharacter* Monster, int64 WaitId) const
{
	return FindRetreatEntry(Monster, WaitId) != nullptr;
}

// 몬스터와 대기 번호가 일치하는 후퇴 대기 항목을 읽는다.
const ALMCombatArea::FLMQueueEntry* ALMCombatArea::FindRetreatEntry(const ALMFieldMonsterCharacter* Monster, int64 WaitId) const
{
	if (!Monster || WaitId <= 0) return nullptr;

	return AttackQueue.FindByPredicate([Monster, WaitId](const FLMQueueEntry& Entry) { return Entry.Monster.Get() == Monster && Entry.WaitId == WaitId && Entry.bWaitRequired; });
}

// 반납 위치에서 정해진 만큼 이동하고 플레이어와 주변 몬스터를 비켰는지 확인한다.
bool ALMCombatArea::CanFinishRetreat(const ALMFieldMonsterCharacter& Monster, int64 WaitId) const
{
	const FLMQueueEntry* Entry = FindRetreatEntry(&Monster, WaitId);
	if (!Entry || Entry->RetreatDistance < 0.0f || !Monster.IsOutsideAttackDistance())
		return false;

	const FVector SelfLocation = Monster.GetNavAgentLocation();
	return FVector::DistSquared2D(SelfLocation, Entry->RetreatStart) >= FMath::Square(static_cast<double>(Entry->RetreatDistance))
		&& HasRetreatSpace(Monster, SelfLocation, 0.0);
}

// 도착 반경만큼 일찍 멈춰도 실제 공격 범위를 벗어나는 지점을 찾는다.
bool ALMCombatArea::ReadRetreatLocation(const ALMFieldMonsterCharacter& Monster, const ALMPlayerCharacter& Player, float AttackRadius, float Clearance, FVector& Location) const
{
	if (!FMath::IsFinite(AttackRadius) || AttackRadius <= 0.0f || !FMath::IsFinite(Clearance) || Clearance <= 0.0f)
		return false;

	const FVector PlayerCenter = Player.GetCapsuleComponent()->GetComponentLocation();
	const FVector MonsterCenter = Monster.GetCapsuleComponent()->GetComponentLocation();
	FVector Away = (MonsterCenter - PlayerCenter).GetSafeNormal2D();
	if (Away.IsNearlyZero())
		Away = -Monster.GetActorForwardVector().GetSafeNormal2D();

	// MoveTo가 MoveReach만큼 일찍 끝날 수 있으므로 목적지를 그만큼 바깥에 둔다.
	const float MinimumDistance = AttackRadius + MoveReach;
	FVector Candidate = PlayerCenter + Away * (MinimumDistance + Clearance);
	Candidate.Z = Monster.GetNavAgentLocation().Z;

	// 투영으로 목적지가 안쪽으로 옮겨지면 도착 반경을 감안해도 이격할 수 없다.
	FVector ProjectedLocation;
	if (!ProjectCombatLocation(Monster, Candidate, ProjectedLocation) || FVector::DistSquared2D(ProjectedLocation, PlayerCenter) <= FMath::Square(MinimumDistance))
		return false;

	Location = ProjectedLocation;
	return true;
}

// 반납 위치에서 직선, 대각선, 측면 순으로 실제 이동 가능한 후퇴 목표를 찾는다.
bool ALMCombatArea::ReadAttackRetreatLocation(const ALMFieldMonsterCharacter& Monster, const ALMPlayerCharacter& Player, float AttackRadius, float Clearance, int64 WaitId, FVector& Location) const
{
	const FLMQueueEntry* Entry = FindRetreatEntry(&Monster, WaitId);
	UWorld* CombatWorld = GetWorld();
	if (!Entry || Entry->RetreatDistance < 0.0f || !CombatWorld || !FMath::IsFinite(AttackRadius) || AttackRadius <= 0.0f || !FMath::IsFinite(Clearance) || Clearance <= 0.0f)
		return false;

	const FVector SelfLocation = Monster.GetNavAgentLocation();
	const FVector PlayerCenter = Player.GetCapsuleComponent()->GetComponentLocation();
	FVector Away = (Entry->RetreatStart - PlayerCenter).GetSafeNormal2D();
	if (Away.IsNearlyZero())
		Away = -Monster.GetActorForwardVector().GetSafeNormal2D();

	// MoveTo의 도착 반경 안에서 멈춰도 두 거리 기준을 넘는 목표를 쓴다.
	const double MinTravel = Entry->RetreatDistance + MoveReach;
	const double MinPlayerDistance = AttackRadius + MoveReach;
	const FVector2D PlayerPosition(PlayerCenter);
	const FVector2D StartPosition(SelfLocation);
	const double MinPathPlayerDistanceSquared = FMath::Min(FVector2D::DistSquared(StartPosition, PlayerPosition), FMath::Square(static_cast<double>(AttackRadius)));
	for (const float Angle : {0.0f, -45.0f, 45.0f, -90.0f, 90.0f})
	{
		FVector Candidate = Entry->RetreatStart + Away.RotateAngleAxis(Angle, FVector::UpVector) * (MinTravel + Clearance);
		Candidate.Z = SelfLocation.Z;
		FVector ProjectedLocation;
		if (!ProjectCombatLocation(Monster, Candidate, ProjectedLocation)
			|| FVector::DistSquared2D(ProjectedLocation, Entry->RetreatStart) < FMath::Square(MinTravel)
			|| FVector::DistSquared2D(ProjectedLocation, PlayerCenter) <= FMath::Square(MinPlayerDistance))
			continue;

		// 방향을 바꿀 때도 플레이어 쪽으로 다시 들어가는 길은 사용하지 않는다.
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(PlayerPosition, StartPosition, FVector2D(ProjectedLocation));
		if (FVector2D::DistSquared(Closest, PlayerPosition) + UE_KINDA_SMALL_NUMBER < MinPathPlayerDistanceSquared
			|| !HasRetreatSpace(Monster, ProjectedLocation, MoveReach))
			continue;

		FVector HitLocation;
		if (UNavigationSystemV1::NavigationRaycast(CombatWorld, SelfLocation, ProjectedLocation, HitLocation, {}, Monster.GetController()))
			continue;

		Location = ProjectedLocation;
		return true;
	}

	return false;
}

// 다른 몬스터 몸체를 지나지 않고 도착 후에도 캡슐 사이에 여유가 남는지 확인한다.
bool ALMCombatArea::HasRetreatSpace(const ALMFieldMonsterCharacter& Monster, const FVector& GoalLocation, double GoalAllowance) const
{
	const UCapsuleComponent* MoveCapsule = Monster.GetCapsuleComponent();
	const FVector CapsuleStart = MoveCapsule->GetComponentLocation();
	const FVector CapsuleGoal = CapsuleStart + (GoalLocation - Monster.GetNavAgentLocation());
	const FVector2D StartPosition(CapsuleStart);
	const FVector2D GoalPosition(CapsuleGoal);
	const double MoveRadius = MoveCapsule->GetScaledCapsuleRadius();
	const double MoveHalfHeight = MoveCapsule->GetScaledCapsuleHalfHeight();
	const double PathMinZ = FMath::Min(CapsuleStart.Z, CapsuleGoal.Z);
	const double PathMaxZ = FMath::Max(CapsuleStart.Z, CapsuleGoal.Z);
	const double ExtraGap = FMath::Max(0.0f, RetreatGap) + GoalAllowance;

	for (const ALMFieldMonsterCharacter* Other : RegisteredMonsters)
	{
		if (!IsValid(Other) || Other == &Monster || Other->GetCombatArea() != this || !Other->IsAliveForCombat())
			continue;

		const UCapsuleComponent* OtherCapsule = Other->GetCapsuleComponent();
		const FVector OtherCenter = OtherCapsule->GetComponentLocation();
		const double BodyHeight = MoveHalfHeight + OtherCapsule->GetScaledCapsuleHalfHeight();
		if (OtherCenter.Z + BodyHeight <= PathMinZ || OtherCenter.Z - BodyHeight >= PathMaxZ)
			continue;

		const FVector2D OtherPosition(OtherCenter);
		const double BodyDistance = MoveRadius + OtherCapsule->GetScaledCapsuleRadius();
		if (FMath::Abs(CapsuleGoal.Z - OtherCenter.Z) < BodyHeight && FVector2D::DistSquared(GoalPosition, OtherPosition) < FMath::Square(BodyDistance + ExtraGap))
			return false;

		// 시작할 때 겹쳐 있다면 현재보다 깊이 들어가는 방향만 막는다.
		const double MinPathDistanceSquared = FMath::Min(FVector2D::DistSquared(StartPosition, OtherPosition), FMath::Square(BodyDistance));
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(OtherPosition, StartPosition, GoalPosition);
		if (FVector2D::DistSquared(Closest, OtherPosition) + UE_KINDA_SMALL_NUMBER < MinPathDistanceSquared)
			return false;
	}

	return true;
}

// 원호 위의 옆걸음 목적지를 찾고, 길이나 옆자리가 막히면 반대쪽을 확인한다.
bool ALMCombatArea::ReadWaitArcLocation(const ALMFieldMonsterCharacter& Monster, const FVector& PlayerCenter, FVector& ArcOffset, float AttackRadius, bool& bRight, FVector& Location) const
{
	UWorld* CombatWorld = GetWorld();
	const UCapsuleComponent* MoveCapsule = Monster.GetCapsuleComponent();
	const double Radius = ArcOffset.Size2D();
	if (!CombatWorld || !MoveCapsule || Radius <= UE_KINDA_SMALL_NUMBER)
		return false;

	const FVector SelfLocation = Monster.GetNavAgentLocation();
	const FVector CapsuleStart = MoveCapsule->GetComponentLocation();
	const FVector Side = (CapsuleStart - PlayerCenter).GetSafeNormal2D();
	if (Side.IsNearlyZero()) return false;

	double Angle = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(ArcOffset, Side).Z, FVector::DotProduct(ArcOffset, Side)));
	const double Limit = FMath::Clamp(static_cast<double>(WaitArcAngle), 15.0, 75.0);
	const double ReachAngle = FMath::RadiansToDegrees(2.0 * FMath::Asin(FMath::Clamp(MoveReach / (2.0 * Radius), 0.0, 1.0)));
	const double Step = FMath::Clamp(2.0 * ReachAngle + 5.0, 18.0, 30.0);

	// 플레이어가 돌아서 좌우 후보가 같은 끝점에 잘리면 현재 방향에서 같은 반경의 원호를 다시 시작한다.
	if (FMath::Abs(Angle) >= Limit + Step)
	{
		ArcOffset = Side * Radius;
		Angle = 0.0;
	}

	// MoveTo가 도착 처리하기 전에 반대편 목표를 주어 끝에서 정지하지 않게 한다.
	const double TurnAngle = Limit - FMath::Min(ReachAngle + 3.0, Limit * 0.5);
	const bool bAtArcEnd = (bRight ? -Angle : Angle) >= TurnAngle;
	const bool bFirstRight = bAtArcEnd ? !bRight : bRight;
	const double ReachSquared = FMath::Square(static_cast<double>(MoveReach));
	const FVector2D Center(PlayerCenter);

	// 지형은 NavMesh로 판단하고, 측면 캡슐은 몬스터 몸통만 검사한다.
	const FCollisionObjectQueryParams ProbeObjects(ECC_GameTraceChannel4); // LMEnemyBody
	const FCollisionQueryParams ProbeQuery(SCENE_QUERY_STAT(LMWaitArcSide), false, &Monster);
	const FCollisionShape ProbeShape = MoveCapsule->GetCollisionShape();
	const float ProbeDistance = FMath::Max(MoveReach, MoveCapsule->GetScaledCapsuleRadius());
	TArray<FHitResult> SideHits;

	for (const bool bTryRight : {bFirstRight, !bFirstRight})
	{
		const double NextAngle = FMath::Clamp(Angle + (bTryRight ? -Step : Step), -Limit, Limit);
		FVector Candidate = PlayerCenter + ArcOffset.RotateAngleAxis(NextAngle, FVector::UpVector);
		// 기본 보폭이 도착 오차 안이면 같은 방향의 끝점까지 써 본다.
		if (FVector::DistSquared2D(SelfLocation, Candidate) <= ReachSquared)
			Candidate = PlayerCenter + ArcOffset.RotateAngleAxis(bTryRight ? -Limit : Limit, FVector::UpVector);
		Candidate.Z = SelfLocation.Z;

		const FVector GoalDelta = Candidate - SelfLocation;
		if (GoalDelta.SizeSquared2D() <= ReachSquared) continue;

		// 공격 반경 안을 가로지르거나 NavMesh가 끊긴 쪽으로는 이동을 요청하지 않는다.
		const FVector2D Nearest = FMath::ClosestPointOnSegment2D(Center, FVector2D(SelfLocation), FVector2D(Candidate));
		if (FVector2D::DistSquared(Nearest, Center) <= FMath::Square(AttackRadius))
			continue;

		FVector HitLocation;
		if (UNavigationSystemV1::NavigationRaycast(CombatWorld, SelfLocation, Candidate, HitLocation, {}, Monster.GetController()))
			continue;

		const FVector ProbeEnd = CapsuleStart + GoalDelta.GetClampedToMaxSize2D(ProbeDistance);
		CombatWorld->SweepMultiByObjectType(SideHits, CapsuleStart, ProbeEnd, MoveCapsule->GetComponentQuat(), ProbeObjects, ProbeShape, ProbeQuery);
		const bool bSideBlocked = SideHits.ContainsByPredicate([&GoalDelta](const FHitResult& Hit)
		{
			if (!Hit.GetComponent()) return false;
			// 이미 겹쳤다면 더 파고드는 쪽만 막아, 반대 방향으로 빠져나올 수 있게 한다.
			return !Hit.bStartPenetrating || FVector::DotProduct(GoalDelta, Hit.ImpactNormal) < -UE_KINDA_SMALL_NUMBER;
		});
		if (bSideBlocked)
			continue;

		bRight = bTryRight;
		Location = Candidate;
		return true;
	}

	return false;
}

// NavMesh 보정이 후보를 다른 높이나 지나치게 먼 곳으로 옮기면 사용하지 않는다.
bool ALMCombatArea::ProjectCombatLocation(const ALMFieldMonsterCharacter& Monster, const FVector& Candidate, FVector& Location) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (!NavSystem || !NavSystem->ProjectPointToNavigation(Candidate, Projected, FVector(PlaceShift, PlaceShift, PlaceHeight), &Monster.GetNavAgentPropertiesRef()))
		return false;
	if (FVector::DistSquared2D(Candidate, Projected.Location) > FMath::Square(PlaceShift) || FMath::Abs(Candidate.Z - Projected.Location.Z) > PlaceHeight)
		return false;

	Location = Projected.Location;
	return true;
}
// Area 자체가 사라질 때는 신규 허가를 막고 자기 기록과 소속을 함께 정리한다.
void ALMCombatArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopMobWatch();
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

		// 이격을 마쳐 현재 행동할 수 있는 몬스터부터 빈 정원을 채운다.
		if (Entry.bWaitRequired || FindAttackTurn(Monster) != INDEX_NONE || !bHasTarget || Player != TargetPlayer || bReturning || !bCanMove)
		{
			++Index;
			continue;
		}

		// 정원은 유지하고 특정 안쪽 자리의 점유 기록은 만들지 않는다.
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

	// 대기 항목의 공격 번호가 일치할 때만 실제 반납 위치를 기록한다.
	if (!bEndingPlay && bIsSight && bRequeue && IsQueueMember(Monster) && Monster->GetTargetPlayer() == TargetPlayer)
	{
		FLMQueueEntry* Entry = AttackQueue.FindByPredicate([Monster](const FLMQueueEntry& Queued) { return Queued.Monster == Monster; });
		if (!Entry && _CombatId == CombatId)
			Entry = &AttackQueue.Add_GetRef({Monster, AttackId, true});

		// 재입장으로 먼저 생성된 대기 항목도 옛 공격권을 실제로 반납할 때 기준을 잡는다.
		if (Entry && Entry->WaitId == AttackId && Entry->bWaitRequired)
		{
			Entry->RetreatStart = Monster->GetNavAgentLocation();
			Entry->RetreatDistance = Monster->GetAttackRetreatDistance();
		}
	}

	FillAttackSlots();
	return true;
}

// 같은 반납에서 실제 후퇴 거리와 두 간격을 모두 확보했을 때 이격 요구를 푼다.
void ALMCombatArea::ReportWaitReady(ALMFieldMonsterCharacter* Monster, int64 _CombatId, int64 WaitId, bool bReady)
{
	if (bEndingPlay || !bIsSight || _CombatId != CombatId || WaitId <= 0 || !bReady) return;
	if (!IsQueueMember(Monster) || Monster->GetTargetPlayer() != TargetPlayer || !CanFinishRetreat(*Monster, WaitId))
		return;

	const int32 Index = AttackQueue.IndexOfByPredicate([Monster, WaitId](const FLMQueueEntry& Entry) { return Entry.Monster.Get() == Monster && Entry.WaitId == WaitId && Entry.bWaitRequired; });
	if (Index == INDEX_NONE) return;
	// 이 대기 번호의 이격은 한 번만 끝낸다. 다음 반납은 새 대기 번호로 다시 요구한다.
	AttackQueue[Index].bWaitRequired = false;
	FillAttackSlots();
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
