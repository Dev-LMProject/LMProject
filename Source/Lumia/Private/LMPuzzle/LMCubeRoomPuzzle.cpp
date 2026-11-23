#include "LMPuzzle/LMCubeRoomPuzzle.h"
#include "LMCharacter/LMPlayerCharacter.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMPlayer/LMPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "TimerManager.h"

namespace
{
    // 보간의 시작과 끝에서 속도를 줄인다.
    float BlendAt(float Seconds, float Delay, float Length)
    {
        const float Rate = FMath::Clamp((Seconds - Delay) / FMath::Max(Length, .001f), 0.f, 1.f);
        return Rate * Rate * (3.f - 2.f * Rate);
    }

    // 고정 Y/Z축으로 90도 돌린 뒤 축을 정수 방향에 맞춰 오차를 지운다.
    FQuat AfterTurn(const FQuat& Before, ELMCubeTurn Direction)
    {
        const bool bTilt = Direction == ELMCubeTurn::Forward || Direction == ELMCubeTurn::Back;
        const bool bReverse = Direction == ELMCubeTurn::Back || Direction == ELMCubeTurn::Left;
        const FVector Axis = bTilt ? FVector::RightVector : FVector::UpVector;

        const FQuat After = FQuat(Axis, FMath::DegreesToRadians(bReverse ? -90.0 : 90.0)) * Before;

        return FRotationMatrix::MakeFromXY(After.GetAxisX().GridSnap(1.0), After.GetAxisY().GridSnap(1.0)).ToQuat();
    }

    // 한 축 벡터가 가리키는 고정 기준면을 구한다.
    ELMCubeFace FindFace(const FVector& Axis)
    {
        if (Axis.X > .5) return ELMCubeFace::Front;
        if (Axis.X < -.5) return ELMCubeFace::Back;
        if (Axis.Y > .5) return ELMCubeFace::Right;
        if (Axis.Y < -.5) return ELMCubeFace::Left;

        return Axis.Z > 0 ? ELMCubeFace::Top : ELMCubeFace::Floor;
    }
}

// 고정 루트에는 발판과 모형을, 회전축에는 기존 방을 연결한다.
ALMCubeRoomPuzzle::ALMCubeRoomPuzzle()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FixedRoot")));

    Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stand"));
    Stand->SetupAttachment(RootComponent);
    Stand->SetMobility(EComponentMobility::Movable);
    Stand->SetRelativeLocation(FVector(0, 0, -15));
    Stand->SetRelativeScale3D(FVector(2, 2, .3));
    Stand->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    DetectBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectBox"));
    DetectBox->SetupAttachment(RootComponent);
    DetectBox->SetRelativeLocation(FVector(0, 0, 85));
    DetectBox->InitBoxExtent(FVector(100, 100, 100));
    DetectBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Interaction = CreateDefaultSubobject<ULMInteractionComponent>(TEXT("Interaction"));
    Interaction->InteractionName = NSLOCTEXT("LMCubeRoom", "Use", "큐브 조작");

    MiniCube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MiniCube"));
    MiniCube->SetupAttachment(RootComponent);
    MiniCube->SetMobility(EComponentMobility::Movable);
    MiniCube->SetRelativeLocation(FVector(65, 0, 70));
    MiniCube->SetRelativeScale3D(FVector(.45));
    MiniCube->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CubeDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeDoor"));
    CubeDoor->SetupAttachment(MiniCube);
    CubeGuide = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeGuide"));
    CubeGuide->SetupAttachment(MiniCube);
    ViewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewRoot"));
    ViewRoot->SetupAttachment(RootComponent);
    DoorGoal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorGoal"));
    DoorGoal->SetupAttachment(ViewRoot);
    GuideGoal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuideGoal"));
    GuideGoal->SetupAttachment(ViewRoot);

    // 목표 표시는 확대 화면에만 보이며 모든 모형 부품은 충돌을 만들지 않는다.
    for (UStaticMeshComponent* ViewMesh : {MiniCube.Get(), CubeDoor.Get(), CubeGuide.Get(), DoorGoal.Get(), GuideGoal.Get()})
    {
        ViewMesh->SetMobility(EComponentMobility::Movable);
        ViewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ViewMesh->SetLightingChannels(false, true, false);
    }
    DoorGoal->SetVisibleInSceneCaptureOnly(true);
    GuideGoal->SetVisibleInSceneCaptureOnly(true);

    CubeCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CubeCapture"));
    CubeCapture->SetupAttachment(ViewRoot);
    CubeCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    CubeCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
    CubeCapture->FOVAngle = 30.f;
    CubeCapture->bCaptureEveryFrame = false;
    CubeCapture->bCaptureOnMovement = false;
    CubeCapture->PrimaryComponentTick.bStartWithTickEnabled = false;
    CubeCapture->ShowFlags.SetAtmosphere(false);
    CubeCapture->ShowFlags.SetFog(false);
    CubeCapture->ShowFlags.SetVolumetricFog(false);
    CubeCapture->ShowFlags.SetMotionBlur(false);
    CubeCapture->ShowFlags.SetTemporalAA(false);
    CubeCapture->ShowFlags.SetEyeAdaptation(false);
    CubeCapture->ShowFlags.SetLumenGlobalIllumination(false);
    CubeCapture->ShowFlags.SetLumenReflections(false);

    KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
    FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
    RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RimLight"));
    for (UPointLightComponent* CubeLight : {KeyLight.Get(), FillLight.Get(), RimLight.Get()})
    {
        CubeLight->SetupAttachment(ViewRoot);
        CubeLight->SetMobility(EComponentMobility::Movable);
        CubeLight->SetLightingChannels(false, true, false);
        CubeLight->SetIntensityUnits(ELightUnits::Lumens);
        CubeLight->SetAttenuationRadius(350.f);
        CubeLight->SetCastShadows(false);
    }
    KeyLight->SetRelativeLocation(FVector(90, -80, 100));
    KeyLight->SetIntensity(900.f);
    KeyLight->SetLightColor(FLinearColor(1.f, .86f, .68f));
    KeyLight->SetCastShadows(true);
    FillLight->SetRelativeLocation(FVector(70, 100, 25));
    FillLight->SetIntensity(350.f);
    FillLight->SetLightColor(FLinearColor(.62f, .76f, 1.f));
    RimLight->SetRelativeLocation(FVector(-80, 20, 80));
    RimLight->SetIntensity(800.f);

    RoomPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RoomPivot"));
    RoomPivot->SetupAttachment(RootComponent);
    RoomPivot->SetMobility(EComponentMobility::Movable);

    ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
    ExitPoint->SetupAttachment(RoomPivot);
    ExitPoint->SetRelativeLocation(FVector(1220, 0, -1120));
    ExitTarget = CreateDefaultSubobject<USceneComponent>(TEXT("ExitTarget"));
    ExitTarget->SetupAttachment(RootComponent);
    ExitTarget->SetRelativeLocation(FVector(1220, 0, 100));

    EntryBridge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntryBridge"));
    ExitBridge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitBridge"));
    for (UStaticMeshComponent* BridgePart : {EntryBridge.Get(), ExitBridge.Get()})
    {
        BridgePart->SetupAttachment(RootComponent);
        BridgePart->SetMobility(EComponentMobility::Movable);
        BridgePart->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    }
    EntryBridge->SetRelativeLocation(EntryOpen);
    ExitBridge->SetRelativeLocation(ExitClosed);
    ExitBridge->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    EntryBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryBlock"));
    ExitBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitBlock"));
    for (UBoxComponent* PathBlock : {EntryBlock.Get(), ExitBlock.Get()})
    {
        PathBlock->SetupAttachment(RootComponent);
        PathBlock->SetMobility(EComponentMobility::Movable);
        PathBlock->InitBoxExtent(FVector(20, 220, 220));
        PathBlock->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        PathBlock->SetGenerateOverlapEvents(false);
    }
    EntryBlock->SetRelativeLocation(FVector(-2540, 0, 260));
    ExitBlock->SetRelativeLocation(FVector(2540, 0, 260));
    EntryBlock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// 회전축은 방 중심에 두고 조작대는 실제 바닥 높이에 놓는다.
void ALMCubeRoomPuzzle::PlaceAtRoomCenter()
{
    if (IsTemplate() || HasActorBegunPlay()) return;
    if (!IsValid(RoomToRotate)) return;

    FBox WallBox(ForceInit);
    int32 WallCount = 0;

    TArray<UStaticMeshComponent*> WallMeshes;
    RoomToRotate->GetComponents<UStaticMeshComponent>(WallMeshes);

    for (const UStaticMeshComponent* WallMesh : WallMeshes)
    {
        if (WallMesh->ComponentHasTag(TEXT("LMCubeWall")))
        {
            WallBox += WallMesh->GetBounds().GetBox();
            ++WallCount;
        }
    }

    if (WallCount != 6 || !WallBox.IsValid)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cube: tag exactly six wall panels with LMCubeWall."));
        return;
    }

    FHitResult FloorHit;
    FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(LMCubeSetup), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(FloorHit, WallBox.GetCenter(), WallBox.GetCenter() - FVector::UpVector * WallBox.GetSize().Z, ECC_Visibility, FloorQuery))
    {
        return;
    }

    // 방과 바닥을 움직이지 않고 조작대와 회전축만 각자의 위치에 맞춘다.
    Modify();
    RoomPivot->Modify();

    const double StandTop = Stand->Bounds.GetBox().Max.Z - GetActorLocation().Z;
    SetActorLocation(FVector(WallBox.GetCenter().X, WallBox.GetCenter().Y, FloorHit.ImpactPoint.Z + 2.5 - StandTop));
    RoomPivot->SetWorldLocation(WallBox.GetCenter());
}

// 방의 완성 배치를 보관한 다음 PIE 시작 자세로 섞는다.
void ALMCubeRoomPuzzle::BeginPlay()
{
    Super::BeginPlay();

    USceneComponent* RoomRoot = IsValid(RoomToRotate) ? RoomToRotate->GetRootComponent() : nullptr;

    if (!RoomRoot || RoomToRotate == this || RoomRoot->GetAttachParent())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cube: assign an unattached room actor."));
        return;
    }

    // 고정 부품이나 물리 부품이 방의 회전을 무시하지 않도록 검사한다.
    TArray<USceneComponent*> RoomParts;
    RoomToRotate->GetComponents<USceneComponent>(RoomParts);

    for (USceneComponent* RoomPart : RoomParts)
    {
        const UPrimitiveComponent* SolidPart = Cast<UPrimitiveComponent>(RoomPart);

        if (RoomPart->Mobility != EComponentMobility::Movable || RoomPart->IsUsingAbsoluteLocation() || RoomPart->IsUsingAbsoluteRotation() || (SolidPart && SolidPart->IsSimulatingPhysics()))
        {
            UE_LOG(LogTemp, Warning, TEXT("Cube: %s must be Movable, relative and non-physics."), *RoomPart->GetName());
            return;
        }
    }

    TArray<UStaticMeshComponent*> Meshes;
    RoomToRotate->GetComponents<UStaticMeshComponent>(Meshes);

    for (UStaticMeshComponent* Mesh : Meshes)
    {
        if (!Mesh->bHiddenInGame && Mesh->ComponentHasTag(TEXT("LMCubeWall")))
            Walls.Add(Mesh);
    }

    if (Walls.Num() != 6)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cube: six visible LMCubeWall panels are required."));
        return;
    }

    // KeepWorld가 방의 순간 이동을 막고 이후 부모 회전만 따라가게 한다.
    RoomHome = RoomToRotate->GetActorTransform();
    bRoomAttached = RoomToRotate->AttachToComponent(RoomPivot, FAttachmentTransformRules::KeepWorldTransform);

    if (!bRoomAttached) return;

    RoomBase = RoomPivot->GetRelativeRotation().Quaternion();
    MiniBase = MiniCube->GetRelativeRotation().Quaternion();

    // 완성 자세에서 실제 문턱과 통로가 맞아야 한 가지 정답을 사용할 수 있다.
    ExitHome = ExitPoint->GetComponentTransform().GetRelativeTransform(GetActorTransform());
    bPathsReady = EntryBridge->GetStaticMesh() && ExitBridge->GetStaticMesh() && !EntryOpen.ContainsNaN() && !EntryClosed.ContainsNaN() && !ExitOpen.ContainsNaN() && !ExitClosed.ContainsNaN() && FVector::DistSquared(EntryOpen, EntryClosed) > 2500.0 && FVector::DistSquared(ExitOpen, ExitClosed) > 2500.0 && IsExitAligned();
    if (!bPathsReady)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cube: assign both bridges, their separate endpoints and an aligned ExitPoint/ExitTarget in the solved pose."));
        return;
    }
    ResetPaths(false);

    if (MiniCube->GetStaticMesh())
        ViewScale = static_cast<float>(MiniCube->GetStaticMesh()->GetBounds().BoxExtent.GetMax() / FMath::Max(FMath::Abs(FVector::DotProduct(ExitHome.GetLocation() - RoomPivot->GetRelativeLocation(), ExitHome.GetRotation().GetForwardVector())), 1.0));

    for (const ELMCubeTurn StartTurn : StartTurns)
        StartPose = AfterTurn(StartPose, StartTurn);

    Pose = StartPose;
    ApplyPose(Pose, Pose);

    // 시작 자세의 바닥이 달라도 조작대가 공중에 남지 않게 맞춘다.
    FVector FloorPoint;
    if (!FindFloor(FloorPoint))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cube: no walkable floor below the control stand."));
        return;
    }
    const FVector FloorShift = FVector::UpVector * (FloorPoint.Z + 2.5 - Stand->Bounds.GetBox().Max.Z);
    Stand->AddWorldOffset(FloorShift);
    MiniCube->AddWorldOffset(FloorShift);
    DetectBox->AddWorldOffset(FloorShift);
    StandHome = Stand->GetRelativeLocation();
    MiniHome = MiniCube->GetRelativeLocation();
    SafeLift = FMath::Max(0.f, static_cast<float>(RoomPivot->GetComponentLocation().Z - 100.0 - Stand->Bounds.GetBox().Max.Z));
    bRoomSafe = HasSafeCenter();
    SetupCubeView();
    if (!bRoomSafe)
        UE_LOG(LogTemp, Warning, TEXT("Cube: keep rotating collision outside the 300cm center safety sphere."));

    if (Interaction->GetInteractionTags().IsEmpty())
    {
        FGameplayTagContainer TagsToUse;
        TagsToUse.AddTag(LMGameplayTags::Interaction_Actor_ActiveTarget);
        Interaction->SetInteractionTags(TagsToUse);
    }

    Interaction->SetDetectionCollision(DetectBox);
}

// 바닥의 조작대 안에서 요청을 받고 안전한 상승 준비 후 화면을 연다.
bool ALMCubeRoomPuzzle::Interact_Implementation(AActor* Interactor)
{
    ALMPlayerCharacter* Body = Cast<ALMPlayerCharacter>(Interactor);

    if (Phase != ELMPhase::Idle || !CanStayOpen() || !IsValid(Body) || !Body->IsAliveForCombat() || Body->GetCharacterMovement()->MovementMode != MOVE_Walking)
    {
        return false;
    }

    ALMPlayerController* PC = Cast<ALMPlayerController>(Body->GetController());
    if (!IsValid(PC)) return false;

    const FBox StandBox = Stand->Bounds.GetBox();
    const FVector BodyPoint = Body->GetActorLocation();
    const float BodyRadius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float BodyHalfHeight = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector RaisedBody = BodyPoint + FVector::UpVector * SafeLift;
    if (FMath::Abs(BodyPoint.X - StandBox.GetCenter().X) + BodyRadius > StandBox.GetExtent().X || FMath::Abs(BodyPoint.Y - StandBox.GetCenter().Y) + BodyRadius > StandBox.GetExtent().Y || FMath::Abs(BodyPoint.Z - BodyHalfHeight - StandBox.Max.Z) > 12.f)
    {
        return false;
    }
    if (!ensureMsgf(FVector::Distance(RaisedBody, RoomPivot->GetComponentLocation()) + BodyHalfHeight < 300.f, TEXT("Cube: player capsule must fit inside the empty center sphere.")))
    {
        return false;
    }

    // 승강 경로가 막혔으면 시점이나 입력을 건드리기 전에 거절한다.
    HeldBody = Body;
    BodyHome = BodyPoint;
    BodyOffset = BodyPoint - Stand->GetComponentLocation();
    if (!CanMoveBody(BodyPoint + FVector::UpVector * SafeLift))
    {
        HeldBody.Reset();
        return false;
    }
    DrawCubeView();

    // 위젯 생성 중 콜백이 다시 진입해도 두 번 열리지 않게 먼저 예약한다.
    Phase = ELMPhase::Opening;
    Driver = PC;

    if (!PC->OpenCubePuzzle(this, PuzzleCamera))
    {
        ClosePuzzle();
        return false;
    }

    if (Phase != ELMPhase::Opening) return false;

    bBodyHeld = true;
    Body->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));

    ShowInside(true);
    OnPuzzleOpened();
    if (Phase == ELMPhase::Opening)
        StartPathMove(ELMCubePathMove::CloseEntry);

    return Phase != ELMPhase::Idle && Phase != ELMPhase::Closing;
}

// 사용 중인 퍼즐과 이미 해결된 퍼즐은 상호작용 목록에서 잠근다.
bool ALMCubeRoomPuzzle::IsInteractionBusy_Implementation() const
{
    return !CanStayOpen() || Phase != ELMPhase::Idle;
}

// 방이나 카메라가 제거됐을 때 화면이 혼자 남지 않게 한다.
bool ALMCubeRoomPuzzle::CanStayOpen() const
{
    const USceneComponent* RoomRoot = IsValid(RoomToRotate) ? RoomToRotate->GetRootComponent() : nullptr;

    const bool bViewReady = IsValid(GetCubeViewTarget()) && MiniCube->GetStaticMesh() && (!bUseGoal || (CubeDoor->GetStaticMesh() && CubeGuide->GetStaticMesh() && DoorGoal->GetStaticMesh() && GuideGoal->GetStaticMesh()));
    return !bEnding && bRoomAttached && bRoomSafe && bPathsReady && bViewReady && RoomRoot && RoomRoot->GetAttachParent() == RoomPivot && !RoomToRotate->IsActorBeingDestroyed() && IsValid(PuzzleCamera) && !PuzzleCamera->IsActorBeingDestroyed();
}

// 정착한 상태에서만 다음 90도 목표를 예약한다.
void ALMCubeRoomPuzzle::Turn(ELMCubeTurn Direction)
{
    if (Phase != ELMPhase::Ready) return;

    NextPose = AfterTurn(Pose, Direction);
    Phase = ELMPhase::Turning;
    Elapsed = 0.f;

    // 한 회전에는 한 인스턴스만 사용하며 완료 또는 취소 때 끝낸다.
    StopTurnShake(true);
    if (Driver.IsValid() && IsValid(Driver->PlayerCameraManager) && TurnShake)
        TurnShakeInstance = Driver->PlayerCameraManager->StartCameraShake(TurnShake);

    SetActorTickEnabled(true);

    if (Driver.IsValid())
        Driver->RefreshCubePanel(this);

    if (Phase == ELMPhase::Turning)
        OnTurnStarted(Direction);
}

// 같은 논리 자세를 서로 다른 크기의 방과 모형에 적용한다.
void ALMCubeRoomPuzzle::ApplyPose(const FQuat& RoomPose, const FQuat& MiniPose)
{
    RoomPivot->SetRelativeRotation(RoomPose * RoomBase);
    MiniCube->SetRelativeRotation(MiniPose * MiniBase);
}

// UI에는 면과 상태만 보내고 문구와 색상은 보내지 않는다.
FLMCubeView ALMCubeRoomPuzzle::GetView() const
{
    FLMCubeView View;
    const FQuat GoalRotation = GetActorQuat().Inverse() * ExitTarget->GetComponentQuat();

    // UI의 목표도 실제 통로의 전방과 위쪽에서 가져온다.
    View.GoalExit = FindFace(GoalRotation.GetForwardVector());
    View.GoalGuide = FindFace(GoalRotation.GetUpVector());
    View.Exit = FindFace(Pose.RotateVector(ExitHome.GetRotation().GetForwardVector()));
    View.Guide = FindFace(Pose.RotateVector(ExitHome.GetRotation().GetUpVector()));

    View.bUseGoal = bUseGoal;
    View.bCanTurn = Phase == ELMPhase::Ready;
    View.bSolved = bSolved;

    return View;
}

// C++에서 결과를 먼저 확정한 뒤 BP에 성공과 정지 연출을 알린다.
void ALMCubeRoomPuzzle::FinishMove(bool bFromTurn)
{
    SetActorTickEnabled(false);
    Elapsed = 0.f;

    const bool bMatched = bUseGoal && Pose.Equals(FQuat::Identity, .0001) && IsExitAligned();
    bSolved = bMatched;
    Phase = bMatched ? ELMPhase::Result : ELMPhase::Ready;

    if (bMatched)
    {
        // 정답 연출 시간에만 기존 Tick을 유지해 문에 빛이 차오르는 화면을 갱신한다.
        SetActorTickEnabled(true);

        // BP가 화면을 닫더라도 취소할 수 있게 타이머를 먼저 등록한다.
        GetWorldTimerManager().SetTimer(ResultTimer, this, &ThisClass::StartPathOpen, FMath::Max(ResultTime, .01f), false);

        // 표시 이벤트에서 화면이 닫혀도 디자이너의 후속 연출이 빠지지 않게 먼저 알린다.
        OnPuzzleSolved();
    }

    if (Phase != ELMPhase::Ready && Phase != ELMPhase::Result)
    {
        return;
    }

    if (Driver.IsValid())
        Driver->RefreshCubePanel(this);

    if (Phase != ELMPhase::Ready && Phase != ELMPhase::Result)
    {
        return;
    }

    if (bFromTurn)
    {
        StopTurnShake(false);
        if (Driver.IsValid() && IsValid(Driver->PlayerCameraManager) && StopShake)
            StopShakeInstance = Driver->PlayerCameraManager->StartCameraShake(StopShake);

        OnTurnStopped();
    }
}

// 승강·회전·짧은 성공 연출 중에만 갱신하고 대기 중에는 Tick을 끈다.
void ALMCubeRoomPuzzle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CanStayOpen() || !Driver.IsValid() || !HeldBody.IsValid() || !HeldBody->IsAliveForCombat())
    {
        ClosePuzzle();
        return;
    }

    Elapsed += DeltaSeconds;

    if (Phase == ELMPhase::Lift)
    {
        const float LiftRate = BlendAt(Elapsed, 0.f, LiftTime);
        if (!MoveStand(SafeLift * LiftRate, LiftHeight * LiftRate))
        {
            ClosePuzzle();
            return;
        }
        if (Elapsed >= LiftTime)
            FinishMove(false);
    }
    else if (Phase == ELMPhase::Turning)
    {
        const float FullTime = FMath::Max(TurnTime, .05f);
        const float Delay = FMath::Clamp(RoomDelay, 0.f, FullTime - .01f);

        const FQuat MiniNow = FQuat::Slerp(Pose, NextPose, BlendAt(Elapsed, 0.f, FMath::Clamp(MiniTurnTime, .01f, FullTime)));
        const FQuat RoomNow = FQuat::Slerp(Pose, NextPose, BlendAt(Elapsed, Delay, FullTime - Delay));

        ApplyPose(RoomNow, MiniNow);
        ShowInside(true);
        if (Elapsed >= FullTime)
        {
            Pose = NextPose;
            ApplyPose(Pose, Pose);
            ShowInside(true);
            FinishMove(true);
        }
    }
    else if (Phase == ELMPhase::Returning)
    {
        const float ReturnRate = BlendAt(Elapsed, 0.f, TurnTime);
        ApplyPose(FQuat::Slerp(ReturnRoom, StartPose, ReturnRate), FQuat::Slerp(ReturnMini, StartPose, ReturnRate));
        ShowInside(true);
        if (Elapsed >= TurnTime)
        {
            Pose = StartPose;
            ApplyPose(Pose, Pose);
            StartPathMove(ELMCubePathMove::RestoreEntry);
        }
    }
    else if (Phase == ELMPhase::Lowering)
    {
        const float LowerRate = BlendAt(Elapsed, 0.f, LiftTime);
        if (!MoveStand(FMath::Lerp(LowerFrom, LowerTo, LowerRate), LowerCubeFrom * (1.f - LowerRate)))
        {
            ClosePuzzle();
            return;
        }
        if (Elapsed >= LiftTime)
        {
            // 정리 함수가 이미 끝낸 하강을 다시 되돌리지 않게 한다.
            bBodyHeld = false;
            ClosePuzzle();
        }
    }

    // 완료 콜백이 화면을 닫았으면 촬영하지 않고, 열린 화면만 프레임당 한 번 갱신한다.
    if (bBodyHeld)
        DrawCubeView();
}

// 카메라 쪽 외벽만 숨기고 충돌은 유지해 방 안을 보여 준다.
void ALMCubeRoomPuzzle::ShowInside(bool bShow)
{
    for (const TWeakObjectPtr<UStaticMeshComponent>& WallRef : Walls)
    {
        UStaticMeshComponent* Wall = WallRef.Get();
        if (!IsValid(Wall)) continue;

        bool bHide = false;

        if (bShow && IsValid(PuzzleCamera))
        {
            const FVector Center = Wall->GetBounds().Origin;
            bHide = FVector::DotProduct(Center - RoomPivot->GetComponentLocation(), PuzzleCamera->GetActorLocation() - Center) > 0.0;
        }

        Wall->SetHiddenInGame(bHide);
    }
}

// 성공 연출 중에는 취소 키로 정답 확인과 안전한 하강을 끊지 않는다.
void ALMCubeRoomPuzzle::RequestLeave()
{
    if (bSolved || Phase == ELMPhase::Returning || Phase == ELMPhase::Lowering || Phase == ELMPhase::RestoringPath)
    {
        return;
    }
    if (Phase != ELMPhase::Ready && Phase != ELMPhase::Turning && Phase != ELMPhase::Lift && Phase != ELMPhase::ClosingPath)
    {
        return;
    }

    StopTurnShake(false);
    if (Phase == ELMPhase::Lift || Phase == ELMPhase::ClosingPath)
    {
        StartPathMove(ELMCubePathMove::RestoreEntry);
        return;
    }
    ReturnRoom = RoomPivot->GetRelativeRotation().Quaternion() * RoomBase.Inverse();
    ReturnMini = MiniCube->GetRelativeRotation().Quaternion() * MiniBase.Inverse();
    Phase = ELMPhase::Returning;
    Elapsed = 0.f;
    SetActorTickEnabled(true);
    if (Driver.IsValid())
        Driver->RefreshCubePanel(this);
}

// 취소·성공·삭제 모두 같은 경로로 카메라와 입력을 돌려준다.
void ALMCubeRoomPuzzle::ClosePuzzle()
{
    if (Phase == ELMPhase::Idle || Phase == ELMPhase::Closing || Phase == ELMPhase::Solved)
    {
        return;
    }

    const bool bWasSolved = bSolved;
    Phase = ELMPhase::Closing;

    SetActorTickEnabled(false);
    GetWorldTimerManager().ClearTimer(ResultTimer);
    GetWorldTimerManager().ClearTimer(PathTimer);
    StopTurnShake(true);

    if (!bWasSolved)
    {
        Pose = StartPose;
        ApplyPose(Pose, Pose);
    }
    ResetPaths(bWasSolved);

    // 강제 종료는 방과 캡슐의 복귀를 먼저 처리하고 마지막에 입력을 푼다.
    ALMPlayerCharacter* ReturnBody = HeldBody.Get();
    if (bBodyHeld && !bEnding && !GetWorld()->bIsTearingDown && IsValid(ReturnBody) && ReturnBody->IsAliveForCombat() && Driver.IsValid() && !Driver->IsActorBeingDestroyed())
    {
        FVector ReturnPoint = BodyHome;
        FVector FloorPoint;
        const bool bHasFloor = FindFloor(FloorPoint);
        if (bWasSolved && bHasFloor)
            ReturnPoint.Z = FloorPoint.Z + ReturnBody->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f;

        if (bHasFloor && ReturnBody->TeleportTo(ReturnPoint, ReturnBody->GetActorRotation()))
        {
            bBodyHeld = false;
            const float FloorLift = StandLift + static_cast<float>(FloorPoint.Z + 2.5 - Stand->Bounds.GetBox().Max.Z);
            MoveStand(FloorLift, 0.f);
        }
        // 복귀 위치가 막히면 아직 살아 있는 중앙 발판 위에서 이동을 복구한다.
    }
    // 사망한 캐릭터는 이동시키지 않되 미해결 조작대는 다음 진입을 위해 바닥으로 복구한다.
    if (bBodyHeld && !bWasSolved && !bEnding && !GetWorld()->bIsTearingDown && (!IsValid(ReturnBody) || !ReturnBody->IsAliveForCombat()))
    {
        bBodyHeld = false;
        MoveStand(0.f, 0.f);
    }
    bBodyHeld = false;
    MiniCube->SetRelativeLocation(Stand->GetRelativeLocation() + MiniHome - StandHome);
    StandHome = Stand->GetRelativeLocation();
    MiniHome = MiniCube->GetRelativeLocation();
    StandLift = 0.f;
    SafeLift = FMath::Max(0.f, static_cast<float>(RoomPivot->GetComponentLocation().Z - 100.0 - Stand->Bounds.GetBox().Max.Z));
    DetectBox->SetWorldLocation(Stand->GetComponentLocation() + FVector::UpVector * (Stand->Bounds.BoxExtent.Z + 85.f));

    ShowInside(false);

    ALMPlayerController* PC = Driver.Get();

    if (IsValid(PC))
    {
        if (StopShakeInstance.IsValid() && IsValid(PC->PlayerCameraManager))
            PC->PlayerCameraManager->StopCameraShake(StopShakeInstance.Get(), true);

        PC->CloseCubePuzzle(this);
    }

    StopShakeInstance.Reset();
    Driver.Reset();
    HeldBody.Reset();

    Phase = bWasSolved ? ELMPhase::Solved : ELMPhase::Idle;
    Interaction->SetIsBusy(bWasSolved);

    if (!bEnding)
        OnPuzzleClosed(bWasSolved);
}

// 원점에서 먼 부품은 회전해도 중앙 안전 구역에 들어오지 않는다.
bool ALMCubeRoomPuzzle::HasSafeCenter() const
{
    if (!IsValid(RoomToRotate) || !GetActorScale3D().Equals(FVector::OneVector) || !GetActorUpVector().Equals(FVector::UpVector))
    {
        return false;
    }

    TArray<UPrimitiveComponent*> SolidParts;
    RoomToRotate->GetComponents<UPrimitiveComponent>(SolidParts);
    for (const UPrimitiveComponent* SolidPart : SolidParts)
    {
        // 방의 기준점으로 쓰는 빈 메시 컴포넌트는 실제 충돌 형상이 없다.
        const UStaticMeshComponent* SolidMesh = Cast<UStaticMeshComponent>(SolidPart);
        if (SolidMesh && !SolidMesh->GetStaticMesh()) continue;
        if (SolidPart->GetCollisionEnabled() == ECollisionEnabled::NoCollision || SolidPart->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
        if (SolidPart->Bounds.GetBox().ComputeSquaredDistanceToPoint(RoomPivot->GetComponentLocation()) < FMath::Square(300.0))
        {
            UE_LOG(LogTemp, Warning, TEXT("Cube: %s enters the center safety sphere."), *SolidPart->GetName());
            return false;
        }
    }
    return true;
}

// 발판이나 플레이어를 제외하고 회전한 방의 실제 바닥을 찾는다.
bool ALMCubeRoomPuzzle::FindFloor(FVector& FloorPoint) const
{
    FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(LMCubeFloor), false, this);
    if (HeldBody.IsValid())
        FloorQuery.AddIgnoredActor(HeldBody.Get());
    if (const APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController())
        FloorQuery.AddIgnoredActor(LocalPlayer->GetPawn());

    FHitResult FloorHit;
    const FVector FloorStart = RoomPivot->GetComponentLocation();
    if (!GetWorld()->LineTraceSingleByChannel(FloorHit, FloorStart, FloorStart - FVector::UpVector * 10000.f, ECC_Pawn, FloorQuery) || FloorHit.ImpactNormal.Z < .7f)
    {
        return false;
    }

    FloorPoint = FloorHit.ImpactPoint;
    return true;
}

// 이동 경로 전체를 캡슐로 검사해 얇은 천장이나 장애물도 놓치지 않는다.
bool ALMCubeRoomPuzzle::CanMoveBody(const FVector& EndPoint) const
{
    const ALMPlayerCharacter* CheckBody = HeldBody.Get();
    if (!IsValid(CheckBody)) return false;

    FCollisionQueryParams BodyQuery(SCENE_QUERY_STAT(LMCubeLift), false, this);
    BodyQuery.AddIgnoredActor(CheckBody);
    const UCapsuleComponent* BodyCapsule = CheckBody->GetCapsuleComponent();
    const FCollisionShape BodyShape = FCollisionShape::MakeCapsule(BodyCapsule->GetScaledCapsuleRadius(), BodyCapsule->GetScaledCapsuleHalfHeight());
    FHitResult BodyHit;
    return !GetWorld()->SweepSingleByChannel(BodyHit, CheckBody->GetActorLocation(), EndPoint, FQuat::Identity, ECC_Pawn, BodyShape, BodyQuery);
}

// 승강 중 새로 들어온 장애물도 검사한 뒤 캐릭터와 발판을 함께 옮긴다.
bool ALMCubeRoomPuzzle::MoveStand(float Height, float CubeRise)
{
    ALMPlayerCharacter* MovingBody = HeldBody.Get();
    const FVector NextStand = GetActorTransform().TransformPosition(StandHome + FVector::UpVector * Height);
    const FVector NextBody = NextStand + BodyOffset;
    const bool bMoveBody = bBodyHeld && IsValid(MovingBody) && MovingBody->IsAliveForCombat();
    if (bMoveBody && !CanMoveBody(NextBody)) return false;

    StandLift = Height;
    Stand->SetRelativeLocation(StandHome + FVector::UpVector * Height);
    MiniCube->SetRelativeLocation(MiniHome + FVector::UpVector * (Height + CubeRise));
    if (bMoveBody)
        MovingBody->SetActorLocation(NextBody, false, nullptr, ETeleportType::TeleportPhysics);
    return true;
}

// 실제 문턱의 위치와 두 방향이 모두 맞아야 보행 통로를 연결한다.
bool ALMCubeRoomPuzzle::IsExitAligned() const
{
    if (!IsValid(ExitPoint) || !IsValid(ExitTarget)) return false;
    return FVector::DistSquared(ExitPoint->GetComponentLocation(), ExitTarget->GetComponentLocation()) <= 25.0 && FVector::DotProduct(ExitPoint->GetForwardVector(), ExitTarget->GetForwardVector()) > .9999 && FVector::DotProduct(ExitPoint->GetUpVector(), ExitTarget->GetUpVector()) > .9999;
}

// 정답을 수락한 방을 유지한 채 출구 통로를 펼친다.
void ALMCubeRoomPuzzle::StartPathOpen()
{
    if (!bSolved || Phase != ELMPhase::Result) return;

    // 타이머가 마지막 Tick보다 먼저 실행돼도 완성 색을 한 번 확정한다.
    Elapsed = FMath::Max(ResultTime, .01f);
    DrawCubeView();
    StartPathMove(ELMCubePathMove::OpenExit);
}

// BP Timeline은 한 번만 재생하고 이동 대상과 충돌은 네이티브 상태가 정한다.
void ALMCubeRoomPuzzle::StartPathMove(ELMCubePathMove Move)
{
    if (bEnding || !bPathsReady || Phase == ELMPhase::Closing || Phase == ELMPhase::Solved || Phase == ELMPhase::Idle)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(PathTimer);
    OnPathMoveReset();
    if (bEnding || Phase == ELMPhase::Closing || Phase == ELMPhase::Solved || Phase == ELMPhase::Idle)
    {
        return;
    }

    PathMove = Move;
    Phase = Move == ELMCubePathMove::CloseEntry ? ELMPhase::ClosingPath : (Move == ELMCubePathMove::OpenExit ? ELMPhase::OpeningPath : ELMPhase::RestoringPath);
    UStaticMeshComponent* MovingBridge = Move == ELMCubePathMove::OpenExit ? ExitBridge.Get() : EntryBridge.Get();
    PathFrom = MovingBridge->GetRelativeLocation();
    PathTo = Move == ELMCubePathMove::OpenExit ? ExitOpen : (Move == ELMCubePathMove::CloseEntry ? EntryClosed : EntryOpen);
    PathAlpha = 0.f;
    MovingBridge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    (Move == ELMCubePathMove::OpenExit ? ExitBlock : EntryBlock)->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SetActorTickEnabled(false);

    // 누락된 Timeline 연결 때문에 이동 입력이 영구히 잠기는 것을 막는다.
    GetWorldTimerManager().SetTimer(PathTimer, this, &ThisClass::OnPathMoveTimeout, 3.f, false);
    if (Driver.IsValid())
        Driver->RefreshCubePanel(this);
    if (Phase == ELMPhase::ClosingPath || Phase == ELMPhase::OpeningPath || Phase == ELMPhase::RestoringPath)
        OnPathMoveStarted();
}

// Timeline 입력을 현재 통로의 두 끝점 사이로 제한해 적용한다.
void ALMCubeRoomPuzzle::SetPathMoveAlpha(float Alpha)
{
    if (bEnding || !FMath::IsFinite(Alpha) || (Phase != ELMPhase::ClosingPath && Phase != ELMPhase::OpeningPath && Phase != ELMPhase::RestoringPath))
    {
        return;
    }
    if (!CanStayOpen() || !Driver.IsValid() || !HeldBody.IsValid() || !HeldBody->IsAliveForCombat())
    {
        ClosePuzzle();
        return;
    }

    PathAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
    UStaticMeshComponent* MovingBridge = PathMove == ELMCubePathMove::OpenExit ? ExitBridge.Get() : EntryBridge.Get();
    MovingBridge->SetRelativeLocation(FMath::Lerp(PathFrom, PathTo, PathAlpha));
}

// 끝점에 도달한 통로만 충돌을 켜고 다음 승강 단계로 넘긴다.
void ALMCubeRoomPuzzle::FinishPathMove()
{
    if (bEnding || PathAlpha < .9999f || (Phase != ELMPhase::ClosingPath && Phase != ELMPhase::OpeningPath && Phase != ELMPhase::RestoringPath))
    {
        return;
    }
    if (!CanStayOpen() || !Driver.IsValid() || !HeldBody.IsValid() || !HeldBody->IsAliveForCombat())
    {
        ClosePuzzle();
        return;
    }

    GetWorldTimerManager().ClearTimer(PathTimer);
    UStaticMeshComponent* MovingBridge = PathMove == ELMCubePathMove::OpenExit ? ExitBridge.Get() : EntryBridge.Get();
    MovingBridge->SetRelativeLocation(PathTo);
    if (!ensureMsgf(MovingBridge->GetRelativeLocation().Equals(PathTo, .1), TEXT("Cube: passage must reach its endpoint before releasing the player.")))
    {
        ClosePuzzle();
        return;
    }

    if (PathMove == ELMCubePathMove::CloseEntry)
    {
        // 방의 회전 범위에서 입구 통로가 빠진 뒤에만 중앙으로 상승한다.
        Phase = ELMPhase::Lift;
        Elapsed = 0.f;
        SetActorTickEnabled(true);
        if (Driver.IsValid())
            Driver->RefreshCubePanel(this);
    }
    else
    {
        MovingBridge->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        (PathMove == ELMCubePathMove::OpenExit ? ExitBlock : EntryBlock)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StartLowering();
    }
}

// 강제 종료도 정답 여부에 맞는 안전한 연결을 남긴다.
void ALMCubeRoomPuzzle::ResetPaths(bool bKeepExit)
{
    GetWorldTimerManager().ClearTimer(PathTimer);
    if (!bEnding)
        OnPathMoveReset();
    if (!bPathsReady) return;

    EntryBridge->SetRelativeLocation(bKeepExit ? EntryClosed : EntryOpen);
    ExitBridge->SetRelativeLocation(bKeepExit ? ExitOpen : ExitClosed);
    EntryBridge->SetCollisionEnabled(bKeepExit ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    ExitBridge->SetCollisionEnabled(bKeepExit ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    EntryBlock->SetCollisionEnabled(bKeepExit ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    ExitBlock->SetCollisionEnabled(bKeepExit ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    PathAlpha = 0.f;
}

// 연출 완료 신호를 받지 못하면 기존 종료 경로로 플레이어를 복구한다.
void ALMCubeRoomPuzzle::OnPathMoveTimeout()
{
    if (Phase != ELMPhase::ClosingPath && Phase != ELMPhase::OpeningPath && Phase != ELMPhase::RestoringPath)
    {
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("Cube: passage Timeline did not finish; restoring a safe connection."));
    ClosePuzzle();
}

// 정지한 방에서 실제 바닥 높이를 다시 조사한 후 하강한다.
void ALMCubeRoomPuzzle::StartLowering()
{
    if (!HeldBody.IsValid() || Phase == ELMPhase::Closing || Phase == ELMPhase::Solved || Phase == ELMPhase::Idle)
    {
        return;
    }
    GetWorldTimerManager().ClearTimer(ResultTimer);
    StopTurnShake(false);

    FVector FloorPoint;
    if (!FindFloor(FloorPoint))
    {
        ClosePuzzle();
        return;
    }
    LowerFrom = StandLift;
    LowerTo = StandLift + static_cast<float>(FloorPoint.Z + 2.5 - Stand->Bounds.GetBox().Max.Z);
    LowerCubeFrom = static_cast<float>(MiniCube->GetRelativeLocation().Z - MiniHome.Z) - StandLift;
    const FVector BodyEnd = HeldBody->GetActorLocation() + FVector::UpVector * (LowerTo - LowerFrom);
    if (!CanMoveBody(BodyEnd))
    {
        ClosePuzzle();
        return;
    }

    Phase = ELMPhase::Lowering;
    Elapsed = 0.f;
    SetActorTickEnabled(true);
    if (Driver.IsValid())
        Driver->RefreshCubePanel(this);
}

// 이 모형을 촬영하는 렌더 타깃을 UI에 전달한다.
UTextureRenderTarget2D* ALMCubeRoomPuzzle::GetCubeViewTarget() const
{
    return IsValid(CubeCapture) ? CubeCapture->TextureTarget.Get() : nullptr;
}

// 모형의 출구와 홈, 고정된 목표 표시를 같은 기준 축에 맞춘다.
void ALMCubeRoomPuzzle::SetupCubeView()
{
    if (!MiniCube->GetStaticMesh() || !CubeDoor->GetStaticMesh() || !CubeGuide->GetStaticMesh() || !DoorGoal->GetStaticMesh() || !GuideGoal->GetStaticMesh())
    {
        return;
    }

    // 에디터 미리보기는 현재 완성 배치를 사용하고 플레이 중에는 저장한 원본을 사용한다.
    if (!bRoomAttached)
    {
        ExitHome = ExitPoint->GetComponentTransform().GetRelativeTransform(GetActorTransform());
        MiniBase = MiniCube->GetRelativeRotation().Quaternion();
        ViewScale = static_cast<float>(MiniCube->GetStaticMesh()->GetBounds().BoxExtent.GetMax() / FMath::Max(FMath::Abs(FVector::DotProduct(ExitHome.GetLocation() - RoomPivot->GetRelativeLocation(), ExitHome.GetRotation().GetForwardVector())), 1.0));
    }
    const FTransform GoalHome = ExitTarget->GetComponentTransform().GetRelativeTransform(GetActorTransform());
    const FQuat DoorTurn = MiniBase.Inverse() * ExitHome.GetRotation();
    const FQuat GoalTurn = GoalHome.GetRotation();
    const FVector DoorPoint = (ExitHome.GetLocation() - RoomPivot->GetRelativeLocation()) * ViewScale;
    const FVector GoalPoint = (GoalHome.GetLocation() - RoomPivot->GetRelativeLocation()) * ViewScale;
    const FBoxSphereBounds DoorBounds = CubeDoor->GetStaticMesh()->GetBounds();
    const FBoxSphereBounds GoalBounds = DoorGoal->GetStaticMesh()->GetBounds();
    const FVector DoorAnchor = DoorBounds.Origin - FVector::UpVector * DoorBounds.BoxExtent.Z;
    const FVector GoalAnchor = GoalBounds.Origin - FVector::UpVector * GoalBounds.BoxExtent.Z;
    const FVector MiniScale = MiniCube->GetRelativeScale3D();

    // 메시 원점 대신 문턱을 맞춰 실제 출구의 높이와 뒤집힌 상태를 그대로 보여 준다.
    CubeDoor->SetRelativeRotation(DoorTurn);
    CubeDoor->SetRelativeLocation(MiniBase.UnrotateVector(DoorPoint + ExitHome.GetRotation().GetForwardVector() * .8) - DoorTurn.RotateVector(DoorAnchor));
    CubeGuide->SetRelativeRotation(DoorTurn);
    CubeGuide->SetRelativeLocation(MiniBase.UnrotateVector(DoorPoint + ExitHome.GetRotation().GetForwardVector() * 3.0) - DoorTurn.RotateVector(CubeGuide->GetStaticMesh()->GetBounds().Origin));
    DoorGoal->SetRelativeRotation(GoalTurn);
    DoorGoal->SetRelativeLocation((GoalPoint + GoalTurn.GetForwardVector() * 1.3 - GoalTurn.RotateVector(GoalAnchor)) * MiniScale);
    GuideGoal->SetRelativeRotation(GoalTurn);
    GuideGoal->SetRelativeLocation((GoalPoint + GoalTurn.GetForwardVector() * 3.0 - GoalTurn.RotateVector(GuideGoal->GetStaticMesh()->GetBounds().Origin)) * MiniScale);
    DoorGoal->SetRelativeScale3D(MiniCube->GetRelativeScale3D());
    GuideGoal->SetRelativeScale3D(MiniCube->GetRelativeScale3D());
    DoorGoal->SetVisibility(bUseGoal);
    GuideGoal->SetVisibility(bUseGoal);

    // 월드 전체를 추가 렌더하지 않고 작은 모형과 목표 부품만 지정한다.
    CubeCapture->ClearShowOnlyComponents();
    for (UStaticMeshComponent* ViewMesh : {MiniCube.Get(), CubeDoor.Get(), CubeGuide.Get(), DoorGoal.Get(), GuideGoal.Get()})
        CubeCapture->ShowOnlyComponent(ViewMesh);
    CubeCapture->bCaptureEveryFrame = false;
    CubeCapture->bCaptureOnMovement = false;
    CubeCapture->SetComponentTickEnabled(false);
}

// 기존 퍼즐 Tick이 모형을 움직인 직후에만 확대 화면을 갱신한다.
void ALMCubeRoomPuzzle::DrawCubeView()
{
    if (!IsValid(GetCubeViewTarget()) || !IsValid(PuzzleCamera))
    {
        return;
    }

    ViewRoot->SetWorldLocation(MiniCube->GetComponentLocation());
    const FRotator ViewAngle = PuzzleCamera->GetActorRotation();
    const double ViewDistance = FMath::Max(140.0, MiniCube->Bounds.SphereRadius * 5.2);
    CubeCapture->SetWorldLocation(MiniCube->GetComponentLocation() - ViewAngle.Vector() * ViewDistance);
    CubeCapture->SetWorldRotation(ViewAngle);

    const bool bAtRest = Phase == ELMPhase::Ready || bSolved;
    const bool bDoorMatches = FVector::DotProduct(CubeDoor->GetForwardVector(), DoorGoal->GetForwardVector()) > .9999;
    const bool bGuideMatches = FVector::DotProduct(CubeGuide->GetUpVector(), GuideGoal->GetUpVector()) > .9999;
    const FVector WarmTint(4.f, 1.45f, .18f);
    const FVector MatchTint(3.f, 2.75f, 2.1f);
    // 문만 보고 풀 수 있도록 출구 방향과 위쪽 축이 모두 맞아야 정렬색을 준다.
    DoorGoal->SetVectorParameterValueOnMaterials(TEXT("Tint"), bAtRest && bDoorMatches && bGuideMatches ? MatchTint : WarmTint);

    // 실제 정답 상태만 채움과 광채를 재생하며 통로 전개 이후에는 완성 색을 유지한다.
    const float WinRate = bSolved ? (Phase == ELMPhase::Result ? FMath::Clamp(Elapsed / FMath::Max(ResultTime, .01f), 0.f, 1.f) : 1.f) : 0.f;
    CubeDoor->SetScalarParameterValueOnMaterials(TEXT("Win"), WinRate);
    DoorGoal->SetScalarParameterValueOnMaterials(TEXT("Win"), WinRate);
    CubeCapture->CaptureScene();
}

// 플레이하지 않고도 에디터에서 현재 외형과 목표 배치를 촬영한다.
void ALMCubeRoomPuzzle::RefreshCubePreview()
{
    if (IsTemplate() || HasActorBegunPlay()) return;
    MiniBase = MiniCube->GetRelativeRotation().Quaternion();
    SetupCubeView();
    DrawCubeView();
}

// 다른 카메라 연출에 영향을 주지 않고 자기 진동 인스턴스만 끝낸다.
void ALMCubeRoomPuzzle::StopTurnShake(bool bImmediately)
{
    if (TurnShakeInstance.IsValid() && Driver.IsValid() && IsValid(Driver->PlayerCameraManager))
        Driver->PlayerCameraManager->StopCameraShake(TurnShakeInstance.Get(), bImmediately);

    if (bImmediately)
        TurnShakeInstance.Reset();
}

// 기존 방이 아직 내 회전축에 연결돼 있을 때만 원래 배치를 돌려준다.
void ALMCubeRoomPuzzle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // shortcut: 이 퍼즐은 맵 상주 액터다. 조작 중 삭제 기능을 추가할 때는 외부 귀환 바닥부터 확보한다.
    bEnding = true;
    ClosePuzzle();

    const USceneComponent* RoomRoot = IsValid(RoomToRotate) ? RoomToRotate->GetRootComponent() : nullptr;

    if (bRoomAttached && RoomRoot && RoomRoot->GetAttachParent() == RoomPivot)
    {
        RoomToRotate->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        RoomToRotate->SetActorTransform(RoomHome, false, nullptr, ETeleportType::TeleportPhysics);
    }

    Super::EndPlay(EndPlayReason);
}
