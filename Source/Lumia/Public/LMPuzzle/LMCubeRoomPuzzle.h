#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LMInterface/LMInteractable.h"
#include "LMCubeRoomPuzzle.generated.h"

class ACameraActor;
class ALMPlayerController;
class ALMPlayerCharacter;
class UBoxComponent;
class UCameraShakeBase;
class ULMInteractionComponent;
class USceneComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;

// 한 번 요청할 90도 회전 방향이다.
UENUM(BlueprintType)
enum class ELMCubeTurn : uint8 { Forward, Back, Left, Right };

// 퍼즐 액터의 고정 축을 기준으로 한 여섯 면이다.
UENUM(BlueprintType)
enum class ELMCubeFace : uint8 { Front, Right, Back, Left, Top, Floor };

// 정렬 표시와 위젯에 필요한 현재 면과 조작 상태를 전달한다.
USTRUCT(BlueprintType)
struct LUMIA_API FLMCubeView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ELMCubeFace Exit = ELMCubeFace::Right;

    UPROPERTY(BlueprintReadOnly)
    ELMCubeFace Guide = ELMCubeFace::Top;

    UPROPERTY(BlueprintReadOnly)
    ELMCubeFace GoalExit = ELMCubeFace::Right;

    UPROPERTY(BlueprintReadOnly)
    ELMCubeFace GoalGuide = ELMCubeFace::Top;

    UPROPERTY(BlueprintReadOnly)
    bool bUseGoal = false;

    UPROPERTY(BlueprintReadOnly)
    bool bCanTurn = false;

    UPROPERTY(BlueprintReadOnly)
    bool bSolved = false;
};

UCLASS()
class LUMIA_API ALMCubeRoomPuzzle : public AActor, public ILMInteractable
{
    GENERATED_BODY()

public:
    // 고정 발판과 회전 부품을 만든다. 외형은 BP에서 지정한다.
    ALMCubeRoomPuzzle();

    // 기존 상호작용 경로로 들어온 요청을 검사하고 조작을 시작한다.
    virtual bool Interact_Implementation(AActor* Interactor) override;

    // 사용 중이거나 해결한 퍼즐의 중복 진입을 막는다.
    virtual bool IsInteractionBusy_Implementation() const override;

    // 입력 한 번을 받아 다음 90도 회전을 시작한다.
    void Turn(ELMCubeTurn Direction);

    // 플레이어의 취소 요청을 처리하며 성공 연출 중에는 기다린다.
    void RequestLeave();

    // 정상 종료와 강제 종료가 같은 정리 경로를 사용한다.
    void ClosePuzzle();

    // 조작에 필요한 방과 카메라가 아직 남아 있는지 확인한다.
    bool CanStayOpen() const;

    // UI가 표시할 현재 면과 목표 면을 계산한다.
    FLMCubeView GetView() const;

    // UI가 이 퍼즐의 실제 큐브 촬영 결과를 연결한다.
    UTextureRenderTarget2D* GetCubeViewTarget() const;

    // 에디터에서 재질과 모형의 촬영 구도를 한 번 갱신한다.
    UFUNCTION(CallInEditor, Category="Cube|View")
    void RefreshCubePreview();

    // 여섯 구조 패널의 중심에 발판과 회전축을 맞춘다.
    UFUNCTION(CallInEditor, Category="Cube|Setup")
    void PlaceAtRoomCenter();

    // BP Timeline의 진행값으로 수납 통로를 이동하며 충돌 권한은 C++에 둔다.
    UFUNCTION(BlueprintCallable, Category="Cube|Path")
    void SetPathMoveAlpha(float Alpha);

    // 통로가 끝점에 도달한 뒤에만 상승 또는 하강을 허용한다.
    UFUNCTION(BlueprintCallable, Category="Cube|Path")
    void FinishPathMove();

protected:
    // 기존 방을 연결하고 에디터의 완성 자세를 시작 자세로 섞는다.
    virtual void BeginPlay() override;

    // 작은 큐브 상승과 두 물체의 회전 중에만 실행한다.
    virtual void Tick(float DeltaSeconds) override;

    // 화면과 방 연결을 정리한다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 화면 진입과 입력 고정이 끝났음을 BP에 알린다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPuzzleOpened();

    // 회전이 시작됐으므로 BP에서 모터음 등을 재생한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnTurnStarted(ELMCubeTurn Direction);

    // 두 물체가 멈췄으므로 BP에서 정지 연출을 재생한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnTurnStopped();

    // 정답을 확정했으므로 BP에서 디자이너가 정한 후속 연출을 시작한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPuzzleSolved();

    // 입력과 카메라 복구 후 BP의 반복 연출을 정리한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPuzzleClosed(bool bWasSolved);

    // BP에서 통로용 Timeline을 처음부터 재생한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPathMoveStarted();

    // 취소나 강제 종료 때 BP Timeline의 늦은 갱신을 중단한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPathMoveReset();

    // 고정 루트 아래에 남는 발판이다.
    UPROPERTY(VisibleAnywhere, Category="Cube")
    TObjectPtr<UStaticMeshComponent> Stand;

    // 기존 상호작용 컴포넌트에 연결할 감지 영역이다.
    UPROPERTY(VisibleAnywhere, Category="Cube")
    TObjectPtr<UBoxComponent> DetectBox;

    UPROPERTY(VisibleAnywhere, Category="Cube")
    TObjectPtr<ULMInteractionComponent> Interaction;

    // 큰 방과 같은 논리 자세를 사용하는 작은 큐브다.
    UPROPERTY(VisibleAnywhere, Category="Cube")
    TObjectPtr<UStaticMeshComponent> MiniCube;

    // 모형에 붙어 함께 도는 실제 출구와 기준 홈이다.
    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UStaticMeshComponent> CubeDoor;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UStaticMeshComponent> CubeGuide;

    // 큐브의 위치만 따라가며 촬영 카메라와 목표 표시는 회전하지 않는다.
    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<USceneComponent> ViewRoot;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<USceneCaptureComponent2D> CubeCapture;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UStaticMeshComponent> DoorGoal;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UStaticMeshComponent> GuideGoal;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UPointLightComponent> KeyLight;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UPointLightComponent> FillLight;

    UPROPERTY(VisibleAnywhere, Category="Cube|View")
    TObjectPtr<UPointLightComponent> RimLight;

    // 기존 큰 방이 연결되는 회전 중심이다.
    UPROPERTY(VisibleAnywhere, Category="Cube")
    TObjectPtr<USceneComponent> RoomPivot;

    // 회전하는 실제 출구의 문턱이다. X는 통과 방향, Z는 위쪽이다.
    UPROPERTY(VisibleAnywhere, Category="Cube|Path")
    TObjectPtr<USceneComponent> ExitPoint;

    // 펼쳐진 외부 통로의 방 쪽 끝이며 방과 함께 회전하지 않는다.
    UPROPERTY(VisibleAnywhere, Category="Cube|Path")
    TObjectPtr<USceneComponent> ExitTarget;

    UPROPERTY(VisibleAnywhere, Category="Cube|Path")
    TObjectPtr<UStaticMeshComponent> EntryBridge;

    UPROPERTY(VisibleAnywhere, Category="Cube|Path")
    TObjectPtr<UStaticMeshComponent> ExitBridge;

    UPROPERTY(VisibleAnywhere, Category="Cube|Path")
    TObjectPtr<UBoxComponent> EntryBlock;

    UPROPERTY(VisibleAnywhere, Category="Cube|Path")
    TObjectPtr<UBoxComponent> ExitBlock;

    // 통로의 끝점은 고정 루트 기준이며 에디터에서 복도 치수에 맞춘다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Path")
    FVector EntryOpen = FVector(-1870, 0, 80);

    UPROPERTY(EditInstanceOnly, Category="Cube|Path")
    FVector EntryClosed = FVector(-3270, 0, 80);

    UPROPERTY(EditInstanceOnly, Category="Cube|Path")
    FVector ExitOpen = FVector(1870, 0, 80);

    UPROPERTY(EditInstanceOnly, Category="Cube|Path")
    FVector ExitClosed = FVector(3270, 0, 80);

    // 레벨에 이미 배치한 방이다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    TObjectPtr<AActor> RoomToRotate;

    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    TObjectPtr<ACameraActor> PuzzleCamera;

    // 목표가 아직 없으면 끄고 회전만 시험할 수 있다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    bool bUseGoal = true;

    // 에디터의 완성 자세를 PIE 시작 때 섞는 순서다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    TArray<ELMCubeTurn> StartTurns;
    
    // 퍼즐을 열면 작은 큐브가 원래 위치에서 이 높이만큼 떠오른다. 단위는 cm다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.0", Units="cm"))
    float LiftHeight = 45.f;

    // 작은 큐브가 LiftHeight까지 올라가는 데 걸리는 시간이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.01", Units="s"))
    float LiftTime = 2.4f;

    // 방향키 입력을 받아들인 순간부터 큰 방이 90도 돌아 멈출 때까지의 전체 시간이다. RoomDelay도 포함한다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.05", Units="s"))
    float TurnTime = 2.6f;

    // 작은 큐브가 90도 도는 데 걸리는 시간이다. 작은 큐브는 입력을 받자마자 돌기 시작한다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.01", Units="s"))
    float MiniTurnTime = 2.6f;

    // 작은 큐브가 돌기 시작한 뒤, 큰 방이 따라 돌기까지 기다리는 시간이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.0", Units="s"))
    float RoomDelay = 0.f;

    // 정답을 맞힌 뒤 퍼즐 화면을 자동으로 닫기까지 기다리는 시간이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.01", Units="s"))
    float ResultTime = 1.2f;

    // 큰 방이 회전을 마치고 멈추는 순간에 재생할 카메라 흔들림을 지정한다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion")
    TSubclassOf<UCameraShakeBase> StopShake;

    // 회전 시작부터 정지까지 재생할 낮은 진동이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion")
    TSubclassOf<UCameraShakeBase> TurnShake;

private:
    // C++은 현재 단계에 따라 플레이어 입력을 받을지, 어떤 연출을 진행할지 결정한다.
    enum class ELMPhase : uint8
    {
        Idle,    // 플레이어가 퍼즐 조작을 시작하기를 기다린다.
        Opening, // 퍼즐 화면을 여는 중이다. 이때는 다시 열기 요청을 받지 않는다.
        ClosingPath, // 입구 통로 수납이 끝나기 전에는 방을 돌리지 않는다.
        OpeningPath, // 출구 통로가 펼쳐지는 동안 플레이어를 중앙에 유지한다.
        RestoringPath, // 취소 후 시작 자세의 입구 통로를 복구한다.
        Lift,    // 작은 큐브가 플레이어 앞에서 떠오른다.
        Ready,   // 방향키를 눌러 방을 돌릴 수 있다.
        Turning, // 작은 큐브와 큰 방이 90도 도는 중이다. 다음 회전 입력은 잠시 막는다.
        Returning, // 플레이어를 중앙에 둔 채 방의 시작 자세를 복구한다.
        Lowering, // 방이 멈춘 뒤 플레이어와 조작대를 바닥으로 내린다.
        Result,  // 정답을 맞혔다. ResultTime 동안 결과를 보여 준다.
        Closing, // 퍼즐 화면을 닫고 카메라와 조작을 플레이어에게 돌려준다.
        Solved   // 성공 연출까지 끝났다. 퍼즐을 다시 조작할 수 없다.
    };

    ELMPhase Phase = ELMPhase::Idle;

    // 통로 연출의 종류이며 별도의 퍼즐 정답이나 진행 단계가 아니다.
    enum class ELMCubePathMove : uint8 { CloseEntry, OpenExit, RestoreEntry };
    ELMCubePathMove PathMove = ELMCubePathMove::CloseEntry;

    bool bRoomAttached = false; // 방 연결 성공 여부
    bool bEnding = false;       // 종료 중 이벤트 재진입 방지
    bool bRoomSafe = false;     // 회전 중심의 안전 구역 검사 결과
    bool bPathsReady = false;   // 통로와 실제 출구 기준이 연결됐는지 여부
    bool bSolved = false;       // 성공 후 하강 중에도 정답을 유지한다.
    bool bBodyHeld = false;     // 이 퍼즐이 실제로 이동을 고정한 캐릭터인지 구분한다.
    float Elapsed = 0.f;        // 현재 연출 시간

    FTransform RoomHome;        // 연결 전 방 배치
    FVector MiniHome;           // 상승 전 작은 큐브 위치
    FVector StandHome;          // 바닥에 놓인 조작대의 상대 위치
    FVector BodyHome;           // 강제 종료 시 복귀할 원래 캡슐 중심
    FVector BodyOffset;         // 조작대 중심에서 캐릭터까지의 고정 거리
    float StandLift = 0.f;      // 현재 조작대 상승량
    float SafeLift = 0.f;       // 방 중앙 안전 높이까지의 상승량
    float LowerFrom = 0.f;      // 하강을 시작한 높이
    float LowerTo = 0.f;        // 실제 바닥 조사로 정한 하강 목표
    float LowerCubeFrom = 0.f;  // 상승 중 취소해도 모형이 순간 이동하지 않게 보관한다.
    FQuat RoomBase;             // 회전축의 완성 방향
    FQuat MiniBase = FQuat::Identity; // 작은 큐브의 완성 방향
    FQuat ReturnRoom;           // 취소 요청을 받은 순간의 방 자세
    FQuat ReturnMini;           // 취소 요청을 받은 순간의 모형 자세
    FTransform ExitHome;       // 완성 자세의 문턱을 고정 루트 좌표로 보관한다.
    FVector PathFrom;          // 중간 취소에서도 현재 위치부터 연출을 이어간다.
    FVector PathTo;
    float PathAlpha = 0.f;
    float ViewScale = 1.f;      // 큰 방의 좌표를 작은 모형 좌표로 줄이는 비율

    FQuat StartPose = FQuat::Identity; // 섞어 둔 시작 자세
    FQuat Pose = FQuat::Identity;      // 정착한 현재 자세
    FQuat NextPose = FQuat::Identity;  // 다음 회전 목표

    FTimerHandle ResultTimer;
    FTimerHandle PathTimer;

    TWeakObjectPtr<ALMPlayerController> Driver;
    TWeakObjectPtr<ALMPlayerCharacter> HeldBody;
    TWeakObjectPtr<UCameraShakeBase> StopShakeInstance;
    TWeakObjectPtr<UCameraShakeBase> TurnShakeInstance;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> Walls;

    // 같은 회전 규칙을 방과 작은 큐브에 각각 적용한다.
    void ApplyPose(const FQuat& RoomPose, const FQuat& MiniPose);

    // 연출 완료 시 상태를 확정하고 BP 흐름 이벤트를 알린다.
    void FinishMove(bool bFromTurn);

    // 카메라를 가리는 외벽의 표시만 바꾸고 충돌은 유지한다.
    void ShowInside(bool bShow);

    // 충돌이 있는 회전 부품이 중앙 안전 구역을 침범하지 않는지 확인한다.
    bool HasSafeCenter() const;

    // 조작대 아래에서 서 있을 수 있는 바닥을 찾는다.
    bool FindFloor(FVector& FloorPoint) const;

    // 캐릭터의 전체 캡슐이 승강 경로를 통과할 수 있는지 확인한다.
    bool CanMoveBody(const FVector& EndPoint) const;

    // 다음 위치가 비어 있을 때만 조작대와 원래 플레이어를 함께 움직인다.
    bool MoveStand(float Height, float CubeRise);

    // 정답 또는 취소 후 안전한 바닥 하강을 시작한다.
    void StartLowering();

    // 실제 출구와 외부 통로의 문턱 위치·전방·위쪽 방향을 비교한다.
    bool IsExitAligned() const;

    // 정답 정지 연출이 끝나면 출구 통로의 전개를 시작한다.
    void StartPathOpen();

    // 입력이 잠긴 상태에서 BP에 통로 연출 한 번을 요청한다.
    void StartPathMove(ELMCubePathMove Move);

    // 강제 종료에서도 안전한 입구 또는 출구 끝점과 충돌을 맞춘다.
    void ResetPaths(bool bKeepExit);

    // BP 완료 연결이 빠져도 입력과 중앙 발판을 영구히 잠그지 않는다.
    void OnPathMoveTimeout();

    // 촬영 대상과 방향 힌트를 한 번 준비한다.
    void SetupCubeView();

    // 실제 모형 변형 이후 작은 렌더 타깃 한 장만 갱신한다.
    void DrawCubeView();

    // 이 퍼즐이 시작한 회전 진동만 끝낸다.
    void StopTurnShake(bool bImmediately);
};
