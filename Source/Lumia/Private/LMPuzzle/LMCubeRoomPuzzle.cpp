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

    // 한 축 벡터가 가리키는 전개도 면을 구한다.
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

    RoomPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RoomPivot"));
    RoomPivot->SetupAttachment(RootComponent);
    RoomPivot->SetMobility(EComponentMobility::Movable);
}

// 외벽 여섯 장의 중심에 회전축을 놓고 발판은 그보다 100cm 아래에 둔다.
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

    // 에디터의 기존 방 배치는 바꾸지 않는다.
    Modify();
    RoomPivot->Modify();

    SetActorLocation(WallBox.GetCenter() - GetActorUpVector() * 100.0);
    RoomPivot->SetRelativeLocation(FVector(0, 0, 100));
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
    MiniHome = MiniCube->GetRelativeLocation();

    for (const ELMCubeTurn StartTurn : StartTurns)
        StartPose = AfterTurn(StartPose, StartTurn);

    Pose = StartPose;
    ApplyPose(Pose, Pose);

    if (Interaction->GetInteractionTags().IsEmpty())
    {
        FGameplayTagContainer TagsToUse;
        TagsToUse.AddTag(LMGameplayTags::Interaction_Actor_ActiveTarget);
        Interaction->SetInteractionTags(TagsToUse);
    }

    Interaction->SetDetectionCollision(DetectBox);
}

// 발판 위에서 시작한 요청만 받고 화면 진입은 기존 컨트롤러에 맡긴다.
bool ALMCubeRoomPuzzle::Interact_Implementation(AActor* Interactor)
{
    ALMPlayerCharacter* Body = Cast<ALMPlayerCharacter>(Interactor);

    if (Phase != ELMPhase::Idle || !CanStayOpen() || !IsValid(Body) || !Body->IsAliveForCombat() || Body->GetMovementBase() != Stand.Get() || Body->GetCharacterMovement()->MovementMode != MOVE_Walking)
    {
        return false;
    }

    ALMPlayerController* PC = Cast<ALMPlayerController>(Body->GetController());
    if (!IsValid(PC)) return false;

    // 위젯 생성 중 콜백이 다시 진입해도 두 번 열리지 않게 먼저 예약한다.
    Phase = ELMPhase::Opening;
    Driver = PC;

    if (!PC->OpenCubePuzzle(this, PuzzleCamera))
    {
        ClosePuzzle();
        return false;
    }

    if (Phase != ELMPhase::Opening) return false;

    Phase = ELMPhase::Lift;
    Elapsed = 0.f;

    ShowInside(true);
    SetActorTickEnabled(true);
    PC->RefreshCubePanel(this);

    if (Phase == ELMPhase::Lift)
        OnPuzzleOpened();

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

    return !bEnding && bRoomAttached && RoomRoot && RoomRoot->GetAttachParent() == RoomPivot && !RoomToRotate->IsActorBeingDestroyed() && IsValid(PuzzleCamera) && !PuzzleCamera->IsActorBeingDestroyed();
}

// 정착한 상태에서만 다음 90도 목표를 예약한다.
void ALMCubeRoomPuzzle::Turn(ELMCubeTurn Direction)
{
    if (Phase != ELMPhase::Ready) return;

    NextPose = AfterTurn(Pose, Direction);
    Phase = ELMPhase::Turning;
    Elapsed = 0.f;

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
    static const FVector Axes[] = {FVector::ForwardVector, FVector::RightVector, -FVector::ForwardVector, -FVector::RightVector, FVector::UpVector, -FVector::UpVector};

    FLMCubeView View;
    View.GoalExit = GoalExit;

    // 출구와 기준면이 서로 다른 두 축을 가리키게 한다.
    View.GoalGuide = GoalExit == ELMCubeFace::Top || GoalExit == ELMCubeFace::Floor ? ELMCubeFace::Front : ELMCubeFace::Top;

    View.Exit = FindFace(Pose.RotateVector(Axes[static_cast<int32>(View.GoalExit)]));
    View.Guide = FindFace(Pose.RotateVector(Axes[static_cast<int32>(View.GoalGuide)]));

    View.bUseGoal = bUseGoal;
    View.bCanTurn = Phase == ELMPhase::Ready;
    View.bSolved = Phase == ELMPhase::Result || Phase == ELMPhase::Solved;

    return View;
}

// C++에서 결과를 먼저 확정한 뒤 BP에 성공과 정지 연출을 알린다.
void ALMCubeRoomPuzzle::FinishMove(bool bFromTurn)
{
    SetActorTickEnabled(false);
    Elapsed = 0.f;

    const bool bMatched = bUseGoal && Pose.Equals(FQuat::Identity, .0001);
    Phase = bMatched ? ELMPhase::Result : ELMPhase::Ready;

    if (bMatched)
    {
        // BP가 화면을 닫더라도 취소할 수 있게 타이머를 먼저 등록한다.
        GetWorldTimerManager().SetTimer(ResultTimer, this, &ThisClass::ClosePuzzle, FMath::Max(ResultTime, .01f), false);

        // 표시 이벤트에서 화면이 닫혀도 발판 개방이 빠지지 않게 먼저 알린다.
        OnPuzzleSolved();
    }

    if (Phase != ELMPhase::Ready && Phase != ELMPhase::Result) return;

    if (Driver.IsValid())
        Driver->RefreshCubePanel(this);

    if (Phase != ELMPhase::Ready && Phase != ELMPhase::Result) return;

    if (bFromTurn)
    {
        if (Driver.IsValid() && IsValid(Driver->PlayerCameraManager) && StopShake)
            StopShakeInstance = Driver->PlayerCameraManager->StartCameraShake(StopShake);

        OnTurnStopped();
    }
}

// 작은 큐브 상승과 회전 중에만 보간하고 나머지 시간에는 Tick을 끈다.
void ALMCubeRoomPuzzle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CanStayOpen() || !Driver.IsValid())
    {
        ClosePuzzle();
        return;
    }

    Elapsed += DeltaSeconds;

    if (Phase == ELMPhase::Lift)
    {
        MiniCube->SetRelativeLocation(MiniHome + FVector::UpVector * LiftHeight * BlendAt(Elapsed, 0.f, LiftTime));

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

        if (Elapsed < FullTime) return;

        Pose = NextPose;
        ApplyPose(Pose, Pose);
        ShowInside(true);
        FinishMove(true);
    }
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

// 성공 연출 중에는 취소 키로 발판 전개를 끊지 않는다.
void ALMCubeRoomPuzzle::RequestLeave()
{
    if (Phase == ELMPhase::Result) return;
    ClosePuzzle();
}

// 취소·성공·삭제 모두 같은 경로로 카메라와 입력을 돌려준다.
void ALMCubeRoomPuzzle::ClosePuzzle()
{
    if (Phase == ELMPhase::Idle || Phase == ELMPhase::Closing || Phase == ELMPhase::Solved) return;

    const bool bWasSolved = Phase == ELMPhase::Result;
    Phase = ELMPhase::Closing;

    SetActorTickEnabled(false);
    GetWorldTimerManager().ClearTimer(ResultTimer);

    if (!bWasSolved)
    {
        Pose = StartPose;
        ApplyPose(Pose, Pose);
        MiniCube->SetRelativeLocation(MiniHome);
    }

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

    Phase = bWasSolved ? ELMPhase::Solved : ELMPhase::Idle;
    Interaction->SetIsBusy(bWasSolved);

    if (!bEnding)
        OnPuzzleClosed(bWasSolved);
}

// 기존 방이 아직 내 회전축에 연결돼 있을 때만 원래 배치를 돌려준다.
void ALMCubeRoomPuzzle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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