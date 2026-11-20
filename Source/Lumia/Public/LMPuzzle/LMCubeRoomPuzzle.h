#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LMInterface/LMInteractable.h"
#include "LMCubeRoomPuzzle.generated.h"

class ACameraActor;
class ALMPlayerController;
class UBoxComponent;
class UCameraShakeBase;
class ULMInteractionComponent;
class USceneComponent;
class UStaticMeshComponent;

// 한 번 요청할 90도 회전 방향이다.
UENUM(BlueprintType)
enum class ELMCubeTurn : uint8 { Forward, Back, Left, Right };

// 퍼즐 액터의 고정 축을 기준으로 한 여섯 면이다.
UENUM(BlueprintType)
enum class ELMCubeFace : uint8 { Front, Right, Back, Left, Top, Floor };

// 화면에 필요한 사실만 전달하며 문구와 색상은 위젯 BP가 정한다.
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

    // 여섯 구조 패널의 중심에 발판과 회전축을 맞춘다.
    UFUNCTION(CallInEditor, Category="Cube|Setup")
    void PlaceAtRoomCenter();

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

    // 정답을 확정했으므로 BP에서 점프 경로를 연다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPuzzleSolved();

    // 입력과 카메라 복구 후 BP의 반복 연출을 정리한다.
    UFUNCTION(BlueprintImplementableEvent, Category="Cube|Flow")
    void OnPuzzleClosed(bool bWasSolved);

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

    // 기존 큰 방이 연결되는 회전 중심이다.
    UPROPERTY(VisibleAnywhere, Category="Cube")
    TObjectPtr<USceneComponent> RoomPivot;

    // 레벨에 이미 배치한 방이다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    TObjectPtr<AActor> RoomToRotate;

    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    TObjectPtr<ACameraActor> PuzzleCamera;

    // 목표가 아직 없으면 끄고 회전만 시험할 수 있다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    bool bUseGoal = true;

    // 완성 자세에서 출구가 있는 면이다. UI 표시 기준으로 사용한다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    ELMCubeFace GoalExit = ELMCubeFace::Right;

    // 에디터의 완성 자세를 PIE 시작 때 섞는 순서다.
    UPROPERTY(EditInstanceOnly, Category="Cube|Setup")
    TArray<ELMCubeTurn> StartTurns;
    // 퍼즐을 열면 작은 큐브가 원래 위치에서 이 높이만큼 떠오른다. 단위는 cm다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.0", Units="cm"))
    float LiftHeight = 45.f;

    // 작은 큐브가 LiftHeight까지 올라가는 데 걸리는 시간이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.01", Units="s"))
    float LiftTime = .55f;

    // 방향키 입력을 받아들인 순간부터 큰 방이 90도 돌아 멈출 때까지의 전체 시간이다. RoomDelay도 포함한다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.05", Units="s"))
    float TurnTime = 1.f;

    // 작은 큐브가 90도 도는 데 걸리는 시간이다. 작은 큐브는 입력을 받자마자 돌기 시작한다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.01", Units="s"))
    float MiniTurnTime = .52f;

    // 작은 큐브가 돌기 시작한 뒤, 큰 방이 따라 돌기까지 기다리는 시간이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.0", Units="s"))
    float RoomDelay = .13f;

    // 정답을 맞힌 뒤 퍼즐 화면을 자동으로 닫기까지 기다리는 시간이다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion", meta=(ClampMin="0.01", Units="s"))
    float ResultTime = 1.2f;

    // 큰 방이 회전을 마치고 멈추는 순간에 재생할 카메라 흔들림을 지정한다.
    UPROPERTY(EditDefaultsOnly, Category="Cube|Motion")
    TSubclassOf<UCameraShakeBase> StopShake;

private:
    // C++은 현재 단계에 따라 플레이어 입력을 받을지, 어떤 연출을 진행할지 결정한다.
    enum class ELMPhase : uint8
    {
        Idle,    // 플레이어가 퍼즐 조작을 시작하기를 기다린다.
        Opening, // 퍼즐 화면을 여는 중이다. 이때는 다시 열기 요청을 받지 않는다.
        Lift,    // 작은 큐브가 플레이어 앞에서 떠오른다.
        Ready,   // 방향키를 눌러 방을 돌릴 수 있다.
        Turning, // 작은 큐브와 큰 방이 90도 도는 중이다. 다음 회전 입력은 잠시 막는다.
        Result,  // 정답을 맞혔다. ResultTime 동안 결과를 보여 준다.
        Closing, // 퍼즐 화면을 닫고 카메라와 조작을 플레이어에게 돌려준다.
        Solved   // 성공 연출까지 끝났다. 퍼즐을 다시 조작할 수 없다.
    };

    ELMPhase Phase = ELMPhase::Idle;

    bool bRoomAttached = false; // 방 연결 성공 여부
    bool bEnding = false;       // 종료 중 이벤트 재진입 방지
    float Elapsed = 0.f;        // 현재 연출 시간

    FTransform RoomHome;        // 연결 전 방 배치
    FVector MiniHome;           // 상승 전 작은 큐브 위치
    FQuat RoomBase;             // 회전축의 완성 방향
    FQuat MiniBase;             // 작은 큐브의 완성 방향

    FQuat StartPose = FQuat::Identity; // 섞어 둔 시작 자세
    FQuat Pose = FQuat::Identity;      // 정착한 현재 자세
    FQuat NextPose = FQuat::Identity;  // 다음 회전 목표

    FTimerHandle ResultTimer;

    TWeakObjectPtr<ALMPlayerController> Driver;
    TWeakObjectPtr<UCameraShakeBase> StopShakeInstance;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> Walls;

    // 같은 회전 규칙을 방과 작은 큐브에 각각 적용한다.
    void ApplyPose(const FQuat& RoomPose, const FQuat& MiniPose);

    // 연출 완료 시 상태를 확정하고 BP 흐름 이벤트를 알린다.
    void FinishMove(bool bFromTurn);

    // 카메라를 가리는 외벽의 표시만 바꾸고 충돌은 유지한다.
    void ShowInside(bool bShow);
};