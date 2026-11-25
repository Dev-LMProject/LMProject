#include "LMCombat/LMBreakableWall.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"

// 통로 판정은 상자 하나에 맡기고 보이는 철판은 충돌 없이 움직이게 한다.
ALMBreakableWall::ALMBreakableWall()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // 절반 크기이므로 실제 문은 두께 20cm, 폭 300cm, 높이 260cm다.
    WallBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("WallBlocker"));
    SetRootComponent(WallBlocker);
    WallBlocker->InitBoxExtent(FVector(10.0, 150.0, 130.0));
    WallBlocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    WallBlocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WallBlocker->SetGenerateOverlapEvents(false);

    // 두 철판 모두 문 중앙을 기준으로 놓아 모델을 바꿀 때 위치가 튀지 않게 한다.
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    RightPiece = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPiece"));

    for (UStaticMeshComponent* PanelMesh : {BodyMesh.Get(), RightPiece.Get()})
    {
        PanelMesh->SetupAttachment(WallBlocker);
        PanelMesh->SetMobility(EComponentMobility::Movable);
        PanelMesh->SetCollisionProfileName(TEXT("NoCollision"));
        PanelMesh->SetGenerateOverlapEvents(false);
        PanelMesh->SetCanEverAffectNavigation(false);
        PanelMesh->SetEvaluateWorldPositionOffset(true);
    }

    // 3타 전에는 온전한 문만 보인다.
    RightPiece->SetVisibility(false);
}

// 블루프린트에서 지정한 모델을 컴포넌트에 넣어 시작 모습을 만든다.
void ALMBreakableWall::OnConstruction(const FTransform& PlacedTransform)
{
    Super::OnConstruction(PlacedTransform);

    BodyMesh->SetStaticMesh(WholeMesh.Get());
    RightPiece->SetStaticMesh(CrackedRightMesh.Get());
    RightPiece->SetVisibility(false);

    // 모델을 교체해도 같은 표면 설정을 사용하고, 재질 선택을 비우면 메시 기본 재질로 돌아간다.
    BodyMesh->SetMaterial(0, DoorMaterial.Get());
    RightPiece->SetMaterial(0, DoorMaterial.Get());
}

// 메시를 빠뜨린 경우 어느 문을 고쳐야 하는지 로그에 남긴다.
void ALMBreakableWall::BeginPlay()
{
    Super::BeginPlay();

    ensureMsgf(WholeMesh && CrackedLeftMesh && CrackedRightMesh, TEXT("%s needs WholeMesh, CrackedLeftMesh and CrackedRightMesh."), *GetName());
}

// 6타 전에는 문을 막는 상자가 피격 본체이고, 열린 뒤에는 잔해가 다시 맞지 않는다.
UPrimitiveComponent* ALMBreakableWall::GetBreakHitBody_Implementation() const
{
    return HitCount < BreakHit ? WallBlocker.Get() : nullptr;
}

// 타수를 따로 복사하지 않고 확정된 파괴 규칙에서 현재 상태를 읽는다.
bool ALMBreakableWall::IsBroken() const
{
    return HitCount >= BreakHit;
}

// 철문 공격음의 선택은 이 클래스에 두고 실제 전방 조회는 공통 파괴 인터페이스에 맡긴다.
bool ALMBreakableWall::ShouldSkipAttackSound(const APawn* AttackPawn)
{
    if (!IsValid(AttackPawn) || !AttackPawn->IsPlayerControlled())
        return false;

    // 플레이어 중심에서 반경 35cm의 구체를 200cm 전진시켜 소리 시작 전의 공격 방향을 살핀다.
    const AActor* FrontHitActor = ILMBreakable::FindHitActorAhead(AttackPawn, 200.0f, 35.0f, ECC_GameTraceChannel3);
    const ALMBreakableWall* FrontDoor = Cast<ALMBreakableWall>(FrontHitActor);
    return IsValid(FrontDoor) && !FrontDoor->IsBroken();
}

// 유효한 타격을 한 번 세고 눌림, 메시 교체, 통로 개방을 처리한다.
void ALMBreakableWall::ReceiveBreakHit_Implementation(const FVector& SourcePoint, const FVector& HitPoint)
{
    if (HitCount >= BreakHit) return;

    // 다른 이벤트가 문 상태를 읽기 전에 이번 타수부터 확정한다.
    ++HitCount;
    EffectTime = 0.0f;
    const bool bJustCracked = HitCount == CrackHit;
    const bool bJustBroken = HitCount == BreakHit;

    // 어느 면에서 때렸든 철판이 공격자에게서 멀어지는 방향으로 눌리게 한다.
    const FTransform& DoorTransform = GetActorTransform();
    PushSide = DoorTransform.InverseTransformPosition(SourcePoint).X > 0.0 ? -1.0f : 1.0f;
    const FVector LocalHit = DoorTransform.InverseTransformPosition(HitPoint);

    // 3타에는 온전한 문을 왼쪽 판으로 바꾸고 오른쪽 판도 보이게 한다.
    if (bJustCracked && CrackedLeftMesh && CrackedRightMesh)
    {
        BodyMesh->SetStaticMesh(CrackedLeftMesh.Get());
        RightPiece->SetVisibility(true);
    }

    // 두 조각에 같은 좌표를 기록해야 모델을 바꿔도 앞선 자국이 이어진다.
    const int32 Slot = (HitCount - 1) * 4;
    const float PressDepth = FMath::Max(0.0f, HitCount < CrackHit ? HitDepth : CrackedHitDepth);
    const FVector Dent(LocalHit.Y, LocalHit.Z, PushSide * PressDepth);
    BodyMesh->SetCustomPrimitiveDataVector3(Slot, Dent);
    RightPiece->SetCustomPrimitiveDataVector3(Slot, Dent);

    // 6타에는 철판이 떨어지는 것을 기다리지 않고 곧바로 길을 연다.
    if (bJustBroken)
    {
        bLeftFirst = LocalHit.Y <= 0.0;
        WallBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 모든 상태와 충돌을 먼저 확정한다. OnWallHit에서도 IsBroken으로 마지막 타격음을 생략할 수 있다.
    SetActorTickEnabled(true);
    OnWallHit(HitPoint);

    // 타격 연출이 액터를 제거한 경우에는 뒤의 연출 이벤트를 보내지 않는다.
    if (!IsValid(this) || IsActorBeingDestroyed()) return;

    // 상태가 처음 바뀐 타격에만 알린다. 모델 지정 여부와 연출 유무는 타수 규칙을 바꾸지 않는다.
    if (bJustCracked)
        OnWallCracked(HitPoint);
    else if (bJustBroken)
        OnWallBroken(HitPoint);
}

// 1~5타에는 판을 짧게 떨고 6타에는 양쪽 판을 차례로 떨어뜨린다.
void ALMBreakableWall::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const bool bBreaking = HitCount == BreakHit;
    const float PieceDuration = FMath::Max(0.01f, PieceMoveTime);
    const float Delay = FMath::Max(0.0f, PieceDelay);
    const float Duration = bBreaking ? PieceDuration + Delay : FMath::Max(0.01f, ShakeTime);
    EffectTime = FMath::Min(EffectTime + DeltaSeconds, Duration);

    if (bBreaking)
    {
        // 지연은 출발 시점에만 적용한다. 두 판의 이동 속도는 같은 판당 시간으로 계산한다.
        const bool bFinished = EffectTime >= Duration;
        MoveBrokenPiece(BodyMesh.Get(), -1.0f, bFinished ? 1.0f : (EffectTime - (bLeftFirst ? 0.0f : Delay)) / PieceDuration);
        MoveBrokenPiece(RightPiece.Get(), 1.0f, bFinished ? 1.0f : (EffectTime - (bLeftFirst ? Delay : 0.0f)) / PieceDuration);
    }
    else
    {
        // 판 전체의 진동만 원점으로 돌린다. 재질에 기록한 자국은 지우지 않는다.
        const float Alpha = EffectTime / Duration;
        const float Push = FMath::Max(0.0f, ShakeSize) * PushSide * FMath::Square(1.0f - Alpha) * FMath::Sin(2.7f * UE_PI * Alpha);
        BodyMesh->SetRelativeLocation(FVector(Push, 0.0, 0.0));
        RightPiece->SetRelativeLocation(FVector(Push, 0.0, 0.0));
    }

    if (EffectTime >= Duration)
        SetActorTickEnabled(false);
}

// 철판을 바깥 고정점에서 젖힌 다음 회전시키며 문턱 높이까지 떨어뜨린다.
void ALMBreakableWall::MoveBrokenPiece(UStaticMeshComponent* Piece, float Side, float PieceAlpha)
{
    const UStaticMesh* PieceMesh = Piece->GetStaticMesh();
    if (!PieceMesh) return;

    /*
     * 각 판의 진행률을 공통으로 사용해 판당 시간을 바꾸면 젖혀짐·낙하가 함께 늘어난다.
     * 0.99는 기존 1.08초에서 출발 지연 0.09초를 뺀 기준 시간으로, 커브를 비우면 기존 움직임을 보존한다.
     */
    const float Alpha = FMath::Clamp(PieceAlpha, 0.0f, 1.0f);
    const float Time = Alpha * 0.99f;
    const float Bend = ReadMoveCurve(BendCurve.Get(), Alpha, FMath::SmoothStep(0.0f, 0.27f, Time));
    const float FallAlpha = ReadMoveCurve(DropCurve.Get(), Alpha, FMath::Clamp((Time - 0.22f) / 0.75f, 0.0f, 1.0f));
    const float DropTime = FallAlpha * 0.75f;
    const float Drop = FMath::SmoothStep(0.0f, 1.0f, FallAlpha);

    // 바깥쪽 고정점에 걸려 잠깐 젖혀졌다가 거의 눕는 자세로 돌아간다.
    const FVector BoxExtent = WallBlocker->GetUnscaledBoxExtent();
    const FVector Pin(0.0, Side * BoxExtent.Y * 0.87, BoxExtent.Z * 0.62);
    const FQuat BendTurn = FRotator(7.0f * PushSide * Bend, Side * PushSide * 22.0f * Bend, Side * 3.0f * Bend).Quaternion();
    const FQuat RestTurn = FRotator(PushSide * 87.5f, -Side * PushSide * 9.0f, Side * 1.5f).Quaternion();
    const FQuat PieceTurn = FQuat::Slerp(BendTurn, RestTurn, Drop);

    // 두 메시가 공유하는 문 중앙 피벗을 조각별 회전 중심에 맞게 보정한다.
    const FBox PieceBox = PieceMesh->GetBoundingBox();
    const FVector PieceCenter = PieceBox.GetCenter();
    FVector PieceOffset = Pin + BendTurn.RotateVector(PieceCenter - Pin) - PieceTurn.RotateVector(PieceCenter);
    PieceOffset += FVector(PushSide * 62.0f * Drop, Side * BoxExtent.Y * 0.66f * Drop, -490.0f * DropTime * DropTime);

    // 상자 아랫면을 바닥으로 삼아 마지막에 철판이 바닥 아래로 가라앉지 않게 한다.
    const double PieceBottom = PieceBox.TransformBy(FTransform(PieceTurn)).Min.Z;
    PieceOffset.Z = FMath::Max(PieceOffset.Z, -BoxExtent.Z - PieceBottom);
    Piece->SetRelativeLocationAndRotation(PieceOffset, PieceTurn);
}

// 디자이너 커브는 움직임만 바꾸며, 시작 자세와 마지막 자세는 C++이 보장한다.
float ALMBreakableWall::ReadMoveCurve(const UCurveFloat* MoveCurve, float Alpha, float DefaultValue)
{
    if (Alpha <= 0.0f) return 0.0f;
    if (Alpha >= 1.0f) return 1.0f;
    if (!MoveCurve || MoveCurve->FloatCurve.IsEmpty())
        return DefaultValue;

    // 잘못된 키 값이나 커브 오버슈트가 철판의 위치·회전을 깨뜨리지 않게 제한한다.
    const float CurveValue = MoveCurve->GetFloatValue(Alpha);
    return FMath::IsFinite(CurveValue) ? FMath::Clamp(CurveValue, 0.0f, 1.0f) : DefaultValue;
}
