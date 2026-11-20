#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LMInterface/LMBreakable.h"
#include "LMBreakableWall.generated.h"

class UBoxComponent;
class UCurveFloat;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

// 공통 피격 인터페이스로 타격을 받아 3타에 갈라지고 6타에 통로를 여는 철문 액터다.
UCLASS(Blueprintable, meta = (DisplayName = "LM Breakable Wall"))
class LUMIA_API ALMBreakableWall : public AActor, public ILMBreakable
{
    GENERATED_BODY()

public:
    // 생성자: 통행·검 판정용 상자와 표시용 철판 두 개를 만들고, 맞기 전에는 Tick을 꺼 둔다.
    ALMBreakableWall();

    /*
     * 공통 공격 코드에 이 문의 피격 상자를 알려 준다.
     * 6타 전에는 WallBlocker를, 이미 부서졌다면 nullptr을 반환해 추가 타격 대상에서 빠진다.
     */
    virtual UPrimitiveComponent* GetBreakHitBody_Implementation() const override;

    /*
     * 공통 피격을 한 타로 반영해 눌림을 기록하고 3타의 메시 교체와 6타의 충돌 해제를 처리한다.
     * SourcePoint는 검 공격자의 위치 또는 착지 충격의 출발점으로, 철판을 누를 방향을 정한다.
     * HitPoint는 자국을 남길 접점이다. 두 좌표는 모두 월드 기준이며 단위는 cm다.
     */
    virtual void ReceiveBreakHit_Implementation(const FVector& SourcePoint, const FVector& HitPoint) override;

    // 마지막 타격음과 파괴음이 겹치지 않도록 BP에 현재 파괴 여부를 알려 준다.
    UFUNCTION(BlueprintPure, Category = "LM|Wall")
    bool IsBroken() const;

protected:
    /*
     * 에디터에서 배치하거나 속성을 바꿀 때 BP에서 지정한 모델을 컴포넌트에 넣는다.
     * BodyMesh에는 온전한 문을 넣고, RightPiece에는 오른쪽 조각을 넣은 뒤 숨긴다.
     * PlacedTransform은 언리얼이 전달한 배치 변환이며 부모의 OnConstruction에 넘긴다.
     */
    virtual void OnConstruction(const FTransform& PlacedTransform) override;

    // 플레이 시작 시 메시 세 개가 모두 지정됐는지 확인하고, 빠졌다면 해당 액터 이름을 로그에 남긴다.
    virtual void BeginPlay() override;

    /*
     * 맞은 직후 활성화되어 1~5타의 짧은 떨림 또는 6타의 조각 낙하를 갱신한다.
     * DeltaSeconds는 이번 프레임의 경과 시간(초)이며, 연출이 끝나면 Tick을 다시 끈다.
     */
    virtual void Tick(float DeltaSeconds) override;

    /*
     * 타수와 문 상태를 반영한 뒤 BP의 Event On Wall Hit를 실행한다.
     * BP는 HitCount를 출력하거나 월드 접점 HitPoint에 소리·이펙트를 재생한다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "LM|Wall|연출 이벤트")
    void OnWallHit(FVector HitPoint);

    // 처음 3타가 되어 메시와 눌림을 반영한 뒤 한 번 호출한다. HitPoint는 월드 접점이다.
    UFUNCTION(BlueprintImplementableEvent, Category = "LM|Wall|연출 이벤트")
    void OnWallCracked(FVector HitPoint);

    // 처음 6타가 되어 통로 충돌을 끄고 낙하를 시작한 뒤 한 번 호출한다. HitPoint는 월드 접점이다.
    UFUNCTION(BlueprintImplementableEvent, Category = "LM|Wall|연출 이벤트")
    void OnWallBroken(FVector HitPoint);

    // 이 문 인스턴스가 받은 타수다. 0에서 시작해 최대 6까지 올라가며 BP에서는 읽기만 가능하다.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LM|Wall")
    int32 HitCount = 0;

    // 0~2타에 보여 줄 온전한 문 모델이다. BP의 Class Defaults에서 Whole Mesh에 지정한다.
    UPROPERTY(EditAnywhere, Category = "LM|Wall")
    TObjectPtr<UStaticMesh> WholeMesh;

    // 3타에 BodyMesh로 교체할 왼쪽 조각 모델이다. 온전한 문과 피벗·표면 좌표를 맞춰 사용한다.
    UPROPERTY(EditAnywhere, Category = "LM|Wall")
    TObjectPtr<UStaticMesh> CrackedLeftMesh;

    // RightPiece에 미리 넣어 두고 3타부터 보여 줄 오른쪽 조각 모델이다.
    UPROPERTY(EditAnywhere, Category = "LM|Wall")
    TObjectPtr<UStaticMesh> CrackedRightMesh;

    // 온전한 문과 두 조각의 첫 재질 슬롯에 같은 재질 인스턴스를 적용한다. 비우면 메시의 원래 재질을 쓴다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|외형", meta = (DisplayName = "철문 재질"))
    TObjectPtr<UMaterialInterface> DoorMaterial;

    // 1~5타에 문 전체가 떨리는 시간이다. 레벨 인스턴스가 아닌 BP 기본값에서 조절한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "피격 떨림 시간", ClampMin = "0.01", Units = "s"))
    float ShakeTime = 0.16f;

    // 문 로컬 X축으로 떨리는 거리의 배율이다. 0이면 떨림 없이 눌림만 남는다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "피격 떨림 강도", ClampMin = "0.0", Units = "cm"))
    float ShakeSize = 1.45f;

    // 1~2타의 접점에 남길 눌림 깊이다. 재질의 기존 타격 슬롯에 전달한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "첫 두 타 눌림 깊이", ClampMin = "0.0", Units = "cm"))
    float HitDepth = 6.0f;

    // 3~6타의 접점에 남길 눌림 깊이다. 앞선 타격 자국의 깊이는 그대로 유지한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "균열 이후 눌림 깊이", ClampMin = "0.0", Units = "cm"))
    float CrackedHitDepth = 8.0f;

    // 각 철판이 출발한 뒤 마지막 자세까지 움직이는 시간이다. 출발 지연과 독립적으로 낙하 속도를 조절한다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "철판 한 장의 연출 시간", ClampMin = "0.01", Units = "s"))
    float PieceMoveTime = 0.99f;

    // 마지막 타격의 반대쪽 판이 늦게 출발하는 시간이다. 전체 연출 시간은 철판 한 장의 연출 시간과 이 값의 합이다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "반대쪽 철판 출발 지연", ClampMin = "0.0", Units = "s"))
    float PieceDelay = 0.09f;

    // X는 각 판의 전체 진행률, Y는 젖혀짐 진행률이다. 양축 0~1, 시작 (0,0)·끝 (1,1)을 사용하며 비우면 기존 움직임을 쓴다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "젖혀짐 커브"))
    TObjectPtr<UCurveFloat> BendCurve;

    // X는 각 판의 전체 진행률, Y는 낙하 진행률이다. 0~1 값을 회전·이동·중력 하강에 함께 적용하며 비우면 기존 움직임을 쓴다.
    UPROPERTY(EditDefaultsOnly, Category = "LM|Wall|연출 설정", meta = (DisplayName = "낙하 커브"))
    TObjectPtr<UCurveFloat> DropCurve;

private:
    // 온전한 문을 좌우 조각으로 바꾸는 타수다. 이때는 통로 충돌을 유지한다.
    static constexpr int32 CrackHit = 3;

    // 통로 충돌을 끄고 두 조각의 낙하를 시작하는 최종 타수다.
    static constexpr int32 BreakHit = 6;

    /*
     * 플레이어 이동과 검 접촉을 막는 루트 충돌 상자다. 화면에는 철판처럼 표시되지 않는다.
     * 보이는 모델의 변형과 통행 판정을 분리해 3~5타에는 막고 6타에 즉시 길을 연다.
     */
    UPROPERTY(VisibleAnywhere, Category = "LM|Wall")
    TObjectPtr<UBoxComponent> WallBlocker;

    /*
     * 모델을 실제로 표시하고 움직이는 컴포넌트다. 처음에는 WholeMesh, 3타부터 CrackedLeftMesh를 쓴다.
     * 자체 충돌은 없으며, 컴포넌트에 저장한 타격 데이터로 재질이 각 자국을 표현한다.
     */
    UPROPERTY(VisibleAnywhere, Category = "LM|Wall")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    /*
     * 오른쪽 조각을 표시하고 움직이는 컴포넌트다. 3타 전에는 숨겨 두며 자체 충돌은 없다.
     * 숨겨진 동안에도 타격 데이터를 받아 나타나는 순간 앞선 눌림이 같은 위치에 이어진다.
     */
    UPROPERTY(VisibleAnywhere, Category = "LM|Wall")
    TObjectPtr<UStaticMeshComponent> RightPiece;

    // 마지막 타격 후 연출이 진행된 시간(초)이다. 새 타격마다 0으로 돌아가고 Tick에서 증가한다.
    float EffectTime = 0.0f;

    // 문 로컬 X축 기준 누르는 방향이다. +X 쪽에서 때리면 -1, 반대쪽에서 때리면 +1을 사용한다.
    float PushSide = 1.0f;

    // 6타의 접점이 문 로컬 Y <= 0이면 true다. 이때 왼쪽 판이 먼저, 오른쪽이 PieceDelay 뒤 움직인다.
    bool bLeftFirst = true;

    /*
     * 철판 한 조각이 바깥 고정점에서 젖혀졌다가 문턱 높이에 놓이는 위치와 회전을 계산한다.
     * Piece는 움직일 컴포넌트, Side는 왼쪽 -1 또는 오른쪽 +1이다.
     * PieceAlpha는 출발 지연을 제외한 해당 조각의 진행률이며, 0은 시작·1은 마지막 자세다.
     */
    void MoveBrokenPiece(UStaticMeshComponent* Piece, float Side, float PieceAlpha);

    // 커브가 없거나 비었으면 기존 진행률을 쓰고, 커브의 시작·끝과 출력 범위를 0~1로 보장한다.
    static float ReadMoveCurve(const UCurveFloat* MoveCurve, float Alpha, float DefaultValue);
};
