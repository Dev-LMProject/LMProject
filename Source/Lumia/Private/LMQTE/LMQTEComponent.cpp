#include "LMQTE/LMQTEComponent.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMPlayer/LMPlayerController.h"
#include "LMUI/LMQTEWidget.h"
#include "UObject/StrongObjectPtr.h"

// 진행하지 않을 때는 갱신 비용이 발생하지 않도록 설정한다.
ULMQTEComponent::ULMQTEComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Windows의 앱 활성화 알림을 연결하며 개별 위젯 포커스는 검사하지 않는다.
void ULMQTEComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!FSlateApplication::IsInitialized()) return;
    bAppActive = FSlateApplication::Get().IsActive();
    AppFocusHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::HandleAppActive);
}

// 취소 콜백에서 즉시 다음 QTE를 열더라도 비활성 앱에는 새 입력 창을 만들지 않는다.
void ULMQTEComponent::HandleAppActive(bool _bAppActive)
{
    bAppActive = _bAppActive;
    if (!bAppActive)
        ResetInputState();
}

// QTE 설정을 검사하며 상위 태그나 알 수 없는 규칙으로 대체하지 않는다.
bool ULMQTEComponent::IsConfigValid(const FLMQTEConfig& _Config) const
{
    const bool bPressRule = _Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Press);
    const bool bMashRule = _Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Mash);
    const bool bDragRule = _Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Drag);
    if ((!bPressRule && !bMashRule && !bDragRule) || !_Config.InputTag.IsValid() || !FMath::IsFinite(_Config.TimeLimit) || _Config.TimeLimit <= 0.0f)
    {
        return false;
    }
    if (bMashRule && _Config.TargetPresses < 1) return false;
    if (!bDragRule) return true;

    // 시작과 목표 영역이 겹치면 움직임 없이 성공하므로 해당 설정을 거부한다.
    const bool bStartInside = FMath::IsFinite(_Config.DragStart.X) && FMath::IsFinite(_Config.DragStart.Y) && _Config.DragStart.X >= 0.0 && _Config.DragStart.X <= 1.0 && _Config.DragStart.Y >= 0.0 && _Config.DragStart.Y <= 1.0;
    const bool bGoalInside = FMath::IsFinite(_Config.DragGoal.X) && FMath::IsFinite(_Config.DragGoal.Y) && _Config.DragGoal.X >= 0.0 && _Config.DragGoal.X <= 1.0 && _Config.DragGoal.Y >= 0.0 && _Config.DragGoal.Y <= 1.0;
    return bStartInside && bGoalInside && FMath::IsFinite(_Config.DragRadius) && _Config.DragRadius > 0.0f && _Config.DragRadius <= 1.0f && FVector2D::Distance(_Config.DragStart, _Config.DragGoal) > 2.0 * _Config.DragRadius;
}

// 화면 생성 콜백의 취소까지 반영한 후에만 실제 입력 창을 연다.
bool ULMQTEComponent::StartQTE(AActor* _RequestOwner, const FLMQTEConfig& _Config, int32& NewRequestId)
{
    NewRequestId = INDEX_NONE;
    ALMPlayerController* PlayerOwner = Cast<ALMPlayerController>(GetOwner());
    if (HasActiveQTE() || bIsEndingPlay || !bAppActive || !IsValid(_RequestOwner) || _RequestOwner->IsActorBeingDestroyed() || !IsValid(PlayerOwner) || !PlayerOwner->CanStartQTE() || !GetWorld() || _RequestOwner->GetWorld() != GetWorld() || !IsConfigValid(_Config))
    {
        return false;
    }
    if (!_Config.WidgetClass || _Config.WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || NextRequestId == MAX_int32)
    {
        return false;
    }

    // 준비 단계도 예약하여 위젯 BP 콜백이 다른 화면이나 QTE를 열지 못하게 한다.
    bStarting = true;
    bStartCancelled = false;
    Config = _Config;
    RequestOwner = _RequestOwner;
    InputOwner = PlayerOwner;
    ActiveRequestId = NextRequestId++;
    _RequestOwner->OnEndPlay.AddDynamic(this, &ThisClass::HandleRequestOwnerEndPlay);

    const TStrongObjectPtr<ULMQTEWidget> NewWidget(CreateWidget<ULMQTEWidget>(PlayerOwner, Config.WidgetClass));
    ActiveWidget = NewWidget.Get();
    bool bOpened = IsValid(ActiveWidget) && !bStartCancelled && !bIsEndingPlay && RequestOwner.IsValid() && InputOwner.IsValid() && InputOwner->CanStartQTE();
    if (bOpened)
    {
        ActiveWidget->InitializeQTE(this, ActiveRequestId, Config);
        bOpened = !bStartCancelled && !bIsEndingPlay && IsValid(ActiveWidget) && RequestOwner.IsValid() && InputOwner.IsValid() && InputOwner->CanStartQTE();
    }
    if (bOpened)
    {
        const bool bOnScreen = ActiveWidget->AddToPlayerScreen(20);
        bOpened = bOnScreen && !bStartCancelled && !bIsEndingPlay && IsValid(ActiveWidget) && ActiveWidget->IsInViewport() && RequestOwner.IsValid() && InputOwner.IsValid() && InputOwner->CanStartQTE();
    }
    if (bOpened)
    {
        // 입력 전환 중 콜백이 취소해도 같은 Controller의 상태를 되돌릴 수 있게 먼저 기록한다.
        bInputOwned = true;
        const bool bInputReady = PlayerOwner->BeginQTEInput(Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Drag), ActiveWidget);
        bOpened = bInputReady && !bStartCancelled && !bIsEndingPlay && IsValid(ActiveWidget) && ActiveWidget->IsInViewport() && RequestOwner.IsValid() && InputOwner.IsValid() && InputOwner->CanStartQTE();
    }
    if (bOpened)
    {
        ActiveWidget->UpdateQTE(1.0f, 0.0f);
        bOpened = !bStartCancelled && !bIsEndingPlay && IsValid(ActiveWidget) && ActiveWidget->IsInViewport() && RequestOwner.IsValid() && InputOwner.IsValid() && InputOwner->CanStartQTE();
    }
    if (!bOpened)
    {
        // 아직 시작하지 않은 요청의 실패에는 완료 이벤트를 발행하지 않는다.
        ClearRequest();
        bStarting = false;
        return false;
    }

    BeginInput(FPlatformTime::Seconds());
    NewRequestId = ActiveRequestId;
    bStarting = false;
    return true;
}

// 비활성 때 관찰한 누름 상태를 유지하여 창이 열린 뒤의 새 입력만 인정한다.
void ULMQTEComponent::BeginInput(double Now)
{
    PressCount = 0;
    InputProgress = 0.0f;
    bDragging = false;
    bMustRelease = HeldInputs.Contains(Config.InputTag);
    Deadline = Now + static_cast<double>(Config.TimeLimit);
    bActive = true;
    SetComponentTickEnabled(true);
}

// 입력 화면 준비와 정리 중에도 다른 요청이 끼어들지 못하게 한다.
bool ULMQTEComponent::HasActiveQTE() const
{
    return bStarting || bActive || bFinishing;
}

// 외부에서 자신이 시작한 요청만 취소할 수 있도록 번호를 제공한다.
int32 ULMQTEComponent::GetActiveRequestId() const
{
    return HasActiveQTE() ? ActiveRequestId : INDEX_NONE;
}

// 실제 시계의 남은 비율을 UI와 판정 양쪽에서 동일하게 사용한다.
float ULMQTEComponent::GetRemainingRatio() const
{
    if (!bActive) return 0.0f;
    return static_cast<float>(FMath::Clamp((Deadline - FPlatformTime::Seconds()) / static_cast<double>(Config.TimeLimit), 0.0, 1.0));
}

// 위젯이 별도 횟수나 위치 판정을 보관하지 않도록 진행도만 제공한다.
float ULMQTEComponent::GetInputProgress() const
{
    return bActive ? InputProgress : 0.0f;
}

// 외부 취소를 시간 만료보다 먼저 처리하며 마감 시각의 입력은 성공시키지 않는다.
bool ULMQTEComponent::CheckRequest()
{
    if (!bActive) return true;
    if (!bAppActive || !RequestOwner.IsValid() || RequestOwner->IsActorBeingDestroyed() || !InputOwner.IsValid() || !InputOwner->CanStartQTE() || !GetWorld() || GetWorld()->IsPaused())
    {
        FinishQTE(ELMQTEResult::Cancelled);
        return true;
    }
    if (FPlatformTime::Seconds() >= Deadline)
    {
        FinishQTE(ELMQTEResult::Failed);
        return true;
    }
    return false;
}

// 새 누름만 받아 Press와 Mash를 같은 입력 경로에서 판정한다.
void ULMQTEComponent::SubmitButtonPress(FGameplayTag InputTag)
{
    if (!InputTag.IsValid()) return;
    const bool bWasHeld = HeldInputs.Contains(InputTag);
    HeldInputs.Add(InputTag);
    if (!bActive || InputTag != Config.InputTag || bWasHeld || bMustRelease)
    {
        return;
    }
    if (CheckRequest()) return;

    if (Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Press))
    {
        InputProgress = 1.0f;
        FinishQTE(ELMQTEResult::Success);
        return;
    }
    if (Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Mash))
    {
        ++PressCount;
        InputProgress = FMath::Clamp(static_cast<float>(PressCount) / static_cast<float>(Config.TargetPresses), 0.0f, 1.0f);
        if (PressCount >= Config.TargetPresses)
            FinishQTE(ELMQTEResult::Success);
        else
            PublishProgress();
    }
}

// 입력 창이 닫혀 있어도 실제 해제는 다음 요청의 누름 상태에 반영한다.
void ULMQTEComponent::SubmitButtonRelease(FGameplayTag InputTag)
{
    HeldInputs.Remove(InputTag);
    if (bActive && InputTag == Config.InputTag)
        bMustRelease = false;
}

// 현재 위젯의 새 좌클릭이 시작 영역에 들어온 경우만 드래그를 시작한다.
bool ULMQTEComponent::BeginDrag(int32 RequestId, FVector2D NormalizedPos)
{
    if (!bActive || RequestId != ActiveRequestId || !Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Drag))
    {
        return false;
    }
    if (CheckRequest()) return false;
    const bool bWasHeld = HeldInputs.Contains(Config.InputTag);
    HeldInputs.Add(Config.InputTag);
    if (bWasHeld || bMustRelease || bDragging || !FMath::IsFinite(NormalizedPos.X) || !FMath::IsFinite(NormalizedPos.Y) || FVector2D::Distance(NormalizedPos, Config.DragStart) > Config.DragRadius)
    {
        return false;
    }
    bDragging = true;
    InputProgress = 0.0f;
    PublishProgress();
    return bActive && RequestId == ActiveRequestId && bDragging;
}

// 목표와의 실제 거리를 사용하여 방향 밖의 이동을 성공으로 오인하지 않는다.
void ULMQTEComponent::MoveDrag(int32 RequestId, FVector2D NormalizedPos)
{
    if (!bActive || RequestId != ActiveRequestId || !bDragging || !FMath::IsFinite(NormalizedPos.X) || !FMath::IsFinite(NormalizedPos.Y))
    {
        return;
    }
    if (CheckRequest()) return;
    const double GoalDistance = FVector2D::Distance(NormalizedPos, Config.DragGoal);
    const double FullDistance = FVector2D::Distance(Config.DragStart, Config.DragGoal);
    InputProgress = static_cast<float>(FMath::Clamp(1.0 - GoalDistance / FullDistance, 0.0, 1.0));
    // 이동 중 화면을 제거하지 않고 실제 버튼 해제까지 같은 위젯에서 받는다.
    if (GoalDistance <= Config.DragRadius)
        InputProgress = 1.0f;
    PublishProgress();
}

// 실제 놓은 위치가 목표 안이면 성공하고 이외의 해제는 새 시도를 허용한다.
void ULMQTEComponent::EndDrag(int32 RequestId)
{
    if (!bActive || RequestId != ActiveRequestId || !Config.InteractionTag.MatchesTagExact(LMGameplayTags::QTE_Interaction_Drag))
    {
        return;
    }
    HeldInputs.Remove(Config.InputTag);
    bMustRelease = false;
    if (CheckRequest()) return;
    if (bDragging && InputProgress >= 1.0f)
    {
        FinishQTE(ELMQTEResult::Success);
        return;
    }
    bDragging = false;
    InputProgress = 0.0f;
    PublishProgress();
}

// 지난 위젯이나 지난 방의 취소가 현재 요청을 종료하지 못하게 한다.
void ULMQTEComponent::CancelQTE(int32 RequestId)
{
    if (RequestId != ActiveRequestId) return;
    if (bStarting)
    {
        bStartCancelled = true;
        return;
    }
    FinishQTE(ELMQTEResult::Cancelled);
}

// 포커스나 게임 일시정지로 입력을 잃으면 성공 해제로 해석하지 않고 취소한다.
void ULMQTEComponent::ResetInputState()
{
    HeldInputs.Reset();
    if (bStarting)
    {
        bStartCancelled = true;
        return;
    }
    FinishQTE(ELMQTEResult::Cancelled);
}

// 위젯 BP 콜백이 요청을 취소하면 뒤의 진행 알림을 보내지 않는다.
void ULMQTEComponent::PublishProgress()
{
    if (!bActive) return;
    const int32 ProgressRequestId = ActiveRequestId;
    const float RemainingRatio = GetRemainingRatio();
    const float ProgressValue = InputProgress;
    if (IsValid(ActiveWidget))
        ActiveWidget->UpdateQTE(RemainingRatio, ProgressValue);
    if (bActive && ActiveRequestId == ProgressRequestId)
        OnQTEProgress.Broadcast(ProgressRequestId, RemainingRatio, ProgressValue);
}

// 먼저 입력 창을 닫아 재진입으로 중복 결과가 발생하지 않게 한다.
void ULMQTEComponent::FinishQTE(ELMQTEResult Result)
{
    if (!bActive) return;
    const int32 FinishedRequestId = ActiveRequestId;
    bActive = false;
    bFinishing = true;
    ClearRequest();
    bFinishing = false;
    OnQTEFinished.Broadcast(FinishedRequestId, Result);
}

// 자기 화면과 입력만 되돌리고 held 기록은 다음 창의 새 누름 검사에 남긴다.
void ULMQTEComponent::ClearRequest()
{
    SetComponentTickEnabled(false);
    if (AActor* CurrentOwner = RequestOwner.Get())
        CurrentOwner->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleRequestOwnerEndPlay);

    // 위젯 제거 콜백이 현재 위젯으로 인식하지 않도록 참조를 먼저 분리한다.
    const TStrongObjectPtr<ULMQTEWidget> ClosingWidget(ActiveWidget.Get());
    ActiveWidget = nullptr;
    if (ClosingWidget.IsValid())
    {
        ClosingWidget->ClearQTE();
        ClosingWidget->RemoveFromParent();
    }
    if (bInputOwned)
    {
        bInputOwned = false;
        if (ALMPlayerController* PlayerOwner = InputOwner.Get())
            PlayerOwner->EndQTEInput();
    }
    RequestOwner.Reset();
    InputOwner.Reset();
    Config = FLMQTEConfig();
    ActiveRequestId = INDEX_NONE;
    PressCount = 0;
    InputProgress = 0.0f;
    Deadline = 0.0;
    bDragging = false;
    bMustRelease = false;
}

// 요청자가 사라졌을 때 준비 중이면 시작을 취소하고 활성 중이면 결과를 정리한다.
void ULMQTEComponent::HandleRequestOwnerEndPlay(AActor* EndedOwner, EEndPlayReason::Type EndReason)
{
    CancelQTE(ActiveRequestId);
}

// 실제 시간으로 마감을 계산하며 연출 속도나 UI 애니메이션에 의존하지 않는다.
void ULMQTEComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (CheckRequest()) return;
    if (!IsValid(ActiveWidget) || !ActiveWidget->IsInViewport())
    {
        FinishQTE(ELMQTEResult::Cancelled);
        return;
    }
    PublishProgress();
}

// 월드 종료 중의 새 시작을 막고 외부 구독에 취소를 한 번 알린다.
void ULMQTEComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bIsEndingPlay = true;
    if (AppFocusHandle.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(AppFocusHandle);
    AppFocusHandle.Reset();
    ResetInputState();
    Super::EndPlay(EndPlayReason);
}
