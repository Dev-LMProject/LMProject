#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LMQTE/LMQTETypes.h"
#include "LMQTEComponent.generated.h"

class AActor;
class ALMPlayerController;
class ULMQTEWidget;

// 화면 정리 후 해당 요청의 결과를 한 번 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLMOnQTEFinished, int32, RequestId, ELMQTEResult, Result);

// 판정과 같은 시계의 남은 시간 비율 및 조작 진행도를 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLMOnQTEProgress, int32, RequestId, float, RemainingRatio, float, InputProgress);

// 단일 PlayerController가 소유하는 공통 QTE 판정과 화면 수명을 관리한다.
UCLASS(ClassGroup = (LM), meta = (BlueprintSpawnableComponent))
class LUMIA_API ULMQTEComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 진행 중에만 Tick을 켜고 비활성 입력도 기록할 준비를 한다.
    ULMQTEComponent();

    // 화면 준비가 완료된 요청만 시작하며 실패 시 완료 이벤트를 발행하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "LM|QTE")
    bool StartQTE(AActor* _RequestOwner, const FLMQTEConfig& _Config, int32& NewRequestId);

    // 시작 및 정리 중인 예약도 포함하여 다른 입력 화면의 진입을 막는다.
    UFUNCTION(BlueprintPure, Category = "LM|QTE")
    bool HasActiveQTE() const;

    // 현재 요청 번호를 반환하며 활성 요청이 없으면 INDEX_NONE을 반환한다.
    UFUNCTION(BlueprintPure, Category = "LM|QTE")
    int32 GetActiveRequestId() const;

    // 비활성 때도 실제 누름 상태를 기록하고 활성 창의 새 입력만 판정한다.
    void SubmitButtonPress(FGameplayTag InputTag);

    // 실제 해제를 기록하여 같은 버튼의 다음 새 누름을 허용한다.
    void SubmitButtonRelease(FGameplayTag InputTag);

    // 정규화된 시작 영역의 새 좌클릭만 드래그로 받아들인다.
    UFUNCTION(BlueprintCallable, Category = "LM|QTE")
    bool BeginDrag(int32 RequestId, FVector2D NormalizedPos);

    // 현재 요청의 드래그 위치와 목표 영역 도착을 기록한다.
    UFUNCTION(BlueprintCallable, Category = "LM|QTE")
    void MoveDrag(int32 RequestId, FVector2D NormalizedPos);

    // 목표 안에 놓으면 성공하고, 다른 위치에 놓으면 제한시간을 유지한 채 재시도한다.
    UFUNCTION(BlueprintCallable, Category = "LM|QTE")
    void EndDrag(int32 RequestId);

    // 일치하는 요청만 취소하며 시작 준비 중에는 취소 표시만 남긴다.
    UFUNCTION(BlueprintCallable, Category = "LM|QTE")
    void CancelQTE(int32 RequestId);

    // 포커스 상실이나 게임 일시정지에 입력 상태와 현재 요청을 정리한다.
    void ResetInputState();

    // UI가 판정과 같은 실제 경과시간의 남은 비율을 조회한다.
    UFUNCTION(BlueprintPure, Category = "LM|QTE")
    float GetRemainingRatio() const;

    // 현재 입력 규칙이 계산한 진행도를 조회한다.
    UFUNCTION(BlueprintPure, Category = "LM|QTE")
    float GetInputProgress() const;

    UPROPERTY(BlueprintAssignable, Category = "LM|QTE")
    FLMOnQTEFinished OnQTEFinished;

    UPROPERTY(BlueprintAssignable, Category = "LM|QTE")
    FLMOnQTEProgress OnQTEProgress;

protected:
    // 게임 화면과 UI 사이의 이동은 허용하고 실제 앱 전환만 관찰한다.
    virtual void BeginPlay() override;

    // 활성 기간에만 요청 수명과 제한시간을 확인하고 화면을 갱신한다.
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 종료 중 새 요청을 막고 자신이 소유한 화면과 입력만 정리한다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 앱이 비활성화되면 먼저 새 요청을 막은 뒤 현재 요청과 눌림을 정리한다.
    void HandleAppActive(bool _bAppActive);

    // 알려진 정확한 태그와 유효한 수치 및 드래그 영역을 검사한다.
    bool IsConfigValid(const FLMQTEConfig& _Config) const;

    // UI 준비가 완료된 순간부터 입력 창과 실제 시계의 마감 시각을 시작한다.
    void BeginInput(double Now);

    // 외부 중단이나 시간 만료가 발생했으면 결과를 확정하고 true를 반환한다.
    bool CheckRequest();

    // 같은 시계에서 계산한 상태를 현재 위젯과 구독자에게 전달한다.
    void PublishProgress();

    // 완료 표시를 먼저 고정하고 정리를 마친 뒤 결과를 한 번 알린다.
    void FinishQTE(ELMQTEResult Result);

    // 자신이 생성한 위젯과 변경한 입력 모드 및 요청자 연결만 해제한다.
    void ClearRequest();

    // 요청한 방이 종료되면 남은 QTE를 취소한다.
    UFUNCTION()
    void HandleRequestOwnerEndPlay(AActor* EndedOwner, EEndPlayReason::Type EndReason);

    UPROPERTY(Transient)
    FLMQTEConfig Config;

    UPROPERTY(Transient)
    TObjectPtr<ULMQTEWidget> ActiveWidget;

    TWeakObjectPtr<AActor> RequestOwner;
    TWeakObjectPtr<ALMPlayerController> InputOwner;
    TSet<FGameplayTag> HeldInputs;
    FDelegateHandle AppFocusHandle;
    int32 ActiveRequestId = INDEX_NONE;
    int32 NextRequestId = 1;
    int32 PressCount = 0;
    double Deadline = 0.0;
    float InputProgress = 0.0f;
    bool bStarting = false;
    bool bActive = false;
    bool bFinishing = false;
    bool bStartCancelled = false;
    bool bIsEndingPlay = false;
    bool bInputOwned = false;
    bool bDragging = false;
    bool bMustRelease = false;
    bool bAppActive = true;

#if WITH_DEV_AUTOMATION_TESTS
    friend struct FLMQTETestAccess;
#endif
};
