#include "LMCharacter/LMPlayerAnimInstance.h"

#include "LMCharacter/LMPlayerCharacter.h"
#include "AnimCharacterMovementLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

/** 현재 Pawn을 기준으로 애니메이션 상태를 초기화한다. */
void ULMPlayerAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    ResetLocomotionState(
        Cast<ALMPlayerCharacter>(TryGetPawnOwner()));
}

/** Pawn 교체·재초기화 시 이전 사건과 포즈 상태를 정리한다. */
void ULMPlayerAnimInstance::ResetLocomotionState(ALMPlayerCharacter* Character)
{
    CachedCharacter = Character;

    Locomotion = FLMPlayerLocomotionFrame();

    SelectedStop = nullptr;
    SelectedJump = nullptr;
    SelectedStart = nullptr;

    // 이미 발생한 사건을 재초기화 후 새 사건으로 처리하지 않는다.
    SeenJumpSerial =
        Character ? Character->GetJumpSerial() : 0;

    SeenLandingSerial =
        Character ? Character->GetLandingSerial() : 0;

    bLeftSupport = true;
    bWasPreparingJump = false;
    bStartActive = false;
    bWasRunRequested = false;
    bLandingActive = false;
    bLandingWasMoving = false;

    StartAge = 0.0f;
    StartBlendOutSeconds = 0.0f;
}

/** 실제 이동 결과를 읽어 모션 선택과 AnimBP 전환 조건을 갱신한다. */
void ULMPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    ALMPlayerCharacter* Character =
        Cast<ALMPlayerCharacter>(TryGetPawnOwner());

    if (Character != CachedCharacter.Get())
    {
        ResetLocomotionState(Character);
    }

    if (!Character || !GetWorld())
    {
        Locomotion = FLMPlayerLocomotionFrame();
        return;
    }

    const float Dt = FMath::Max(DeltaSeconds, 0.0f);

    const FLMPlayerLocomotionFrame Previous =
        Locomotion;

    const UCharacterMovementComponent* M =
        Character->GetCharacterMovement();

    const FVector Velocity =
        Character->GetVelocity();

    const bool bFree =
        Character->UsesFreeMovement() &&
        (M->IsMovingOnGround() || M->IsFalling());

    const float RunThreshold =
        0.5f *
        (Character->GetWalkSpeed() + Character->GetRunSpeed());

    Locomotion.GroundSpeed = Velocity.Size2D();
    Locomotion.VerticalSpeed = Velocity.Z;
    Locomotion.HasMoveInput = Character->HasMoveInput();
    Locomotion.InAir = M->IsFalling();

    Locomotion.PreparingJump =
        bFree && Character->IsPreparingJump();

    Locomotion.FlightPoseActive =
        Locomotion.InAir || Locomotion.PreparingJump;

    Locomotion.Moving =
        Locomotion.GroundSpeed > 5.0f;

    // 실제 속도에 맞춰 Walk/Run 혼합과 발 재생률을 계산한다.
    const float GaitTarget = FMath::Clamp(
        (Locomotion.GroundSpeed - Character->GetWalkSpeed()) /
        FMath::Max(
            Character->GetRunSpeed() - Character->GetWalkSpeed(),
            1.0f),
        0.0f,
        1.0f);

    Locomotion.WalkRunAlpha = FMath::FInterpTo(
        Previous.WalkRunAlpha,
        GaitTarget,
        Dt,
        10.0f);

    const float ReferenceSpeed = FMath::Lerp(
        WalkAnimationReferenceSpeed,
        RunAnimationReferenceSpeed,
        Locomotion.WalkRunAlpha);

    Locomotion.LoopPlayRate = FMath::Clamp(
        Locomotion.GroundSpeed /
            FMath::Max(ReferenceSpeed, 1.0f),
        0.45f,
        1.6f);

    UpdateSupportFoot();

    // 점프 준비 또는 준비 없는 실제 도약에서 클립을 한 번 선택한다.
    if (Locomotion.PreparingJump && !bWasPreparingJump)
    {
        SelectJump(
            Locomotion.GroundSpeed,
            RunThreshold);
    }

    if (Character->GetJumpSerial() != SeenJumpSerial)
    {
        if (bFree && (!bWasPreparingJump || !SelectedJump))
        {
            SelectJump(
                Character->GetTakeoffSpeed(),
                RunThreshold);
        }

        SeenJumpSerial = Character->GetJumpSerial();
    }

    const float JumpAge = Character->GetJumpAnimationElapsed();

    Locomotion.JumpPoseTime = SelectedJump
        ? FMath::Clamp(
            JumpAge,
            0.0f,
            SelectedJump->GetPlayLength())
        : 0.0f;

    Locomotion.JumpStarting =
        bFree &&
        SelectedJump &&
        (Locomotion.PreparingJump ||
        (Locomotion.InAir &&
         Character->IsCurrentFallFromJump() &&
         JumpAge < JumpStartWindow &&
         Velocity.Z > 0.0f));

    bWasPreparingJump = Locomotion.PreparingJump;

    // 외부 행동에서 발생한 착지는 일반 착지 모션으로 뒤늦게 재생하지 않는다.
    if (Character->GetLandingSerial() != SeenLandingSerial)
    {
        SeenLandingSerial =
            Character->GetLandingSerial();

        bLandingActive =
            bFree &&
            Character->WasFreeMovementAtLanding();

        bLandingWasMoving =
            Character->WasMovingAtLanding();

        Locomotion.HeavyLanding =
            Character->GetLandingImpactSpeed() >=
            HeavyLandingSpeed;
    }

    const double LandAge =
        GetWorld()->GetTimeSeconds() -
        Character->GetLastLandingTime();

    if (bLandingActive && Locomotion.HasMoveInput)
    {
        // 이후 키를 놓아도 긴 착지 시간으로 되돌아가지 않는다.
        bLandingWasMoving = true;
    }

    const float LandWindow = bLandingWasMoving
        ? MovingLandingWindow
        : (Locomotion.HeavyLanding
            ? HeavyLandingWindow
            : IdleLandingWindow);

    if (!bFree ||
        Locomotion.FlightPoseActive ||
        LandAge < 0.0 ||
        LandAge >= LandWindow)
    {
        bLandingActive = false;
    }

    Locomotion.Landing = bLandingActive;

    Locomotion.LandingPoseTime =
        FMath::Max(
            0.0f,
            static_cast<float>(LandAge));

    // 정지 종류를 감속 진입 때 고정해 RunStop이 도중에 WalkStop으로 바뀌지 않게 한다.
    const bool bBraking =
        bFree &&
        M->IsMovingOnGround() &&
        !Locomotion.HasMoveInput &&
        Locomotion.Moving &&
        !Locomotion.PreparingJump &&
        !Locomotion.Landing;

    if (bBraking && !Previous.Stopping)
    {
        SelectStop(
            FMath::Max(
                Previous.GroundSpeed,
                Locomotion.GroundSpeed),
            RunThreshold);
    }

    Locomotion.Stopping =
        bBraking && SelectedStop != nullptr;

    Locomotion.StopDistance = Locomotion.Stopping
        ? UAnimCharacterMovementLibrary::PredictGroundMovementStopLocation(
            Velocity,
            M->bUseSeparateBrakingFriction,
            M->BrakingFriction,
            M->GroundFriction,
            M->BrakingFrictionFactor,
            M->BrakingDecelerationWalking).Size2D()
        : 0.0f;

    // 정지 출발과 걷기 중 달리기 전환을 각각 감지한다.
    const bool bRunRequested =
        Character->IsRunRequestedAndAllowed();

    const bool bRunBegan =
        bRunRequested && !bWasRunRequested;

    const bool bMoveBegan =
        Locomotion.HasMoveInput && !Previous.HasMoveInput;

    const bool bCanPlayStart =
        bFree &&
        M->IsMovingOnGround() &&
        Locomotion.HasMoveInput &&
        bRunRequested &&
        !Locomotion.FlightPoseActive &&
        !Locomotion.Landing;

    UAnimSequence* StartCandidate = nullptr;

    if (!bStartActive && bCanPlayStart)
    {
        const bool bWasIdle = Previous.GroundSpeed <= 5.0f;

        if (bWasIdle && (bMoveBegan || bRunBegan))
        {
            StartCandidate = RunStart.Get();
        }
        else if (!bWasIdle &&
                 bRunBegan &&
                 Previous.GroundSpeed < RunThreshold)
        {
            StartCandidate = WalkToRunStart.Get();
        }
    }

    const float StartPlayRate =
        FMath::Clamp(RunStartPlayRate, 0.1f, 3.0f);

    if (StartCandidate)
    {
        SelectedStart = StartCandidate;
        StartAge = 0.0f;
        bStartActive = true;

        // 전환 시간을 AnimBP에서 읽어, 클립 종료 전에 같은 길이만큼 겹쳐 재생한다.
        StartBlendOutSeconds = 0.0f;
        if (const FBakedAnimationStateMachine* GroundMachine =
            GetStateMachineInstanceDesc(TEXT("GroundMovement")))
        {
            const int32 TransitionIndex = GroundMachine->FindTransitionIndex(
                TEXT("Start"), TEXT("Move"));

            if (GroundMachine->Transitions.IsValidIndex(TransitionIndex))
            {
                StartBlendOutSeconds = FMath::Max(
                    GroundMachine->Transitions[TransitionIndex].CrossfadeDuration,
                    0.0f);
            }
        }
    }
    else if (bStartActive)
    {
        // 달리기로 섞이는 동안에도 출발 자세를 끝까지 진행한다.
        StartAge += Dt * StartPlayRate;
    }

    // 입력 해제·점프·착지·클립 종료 시 출발 표현을 끝낸다.
    if (!bCanPlayStart ||
        !SelectedStart ||
        (SelectedStart &&
         StartAge >= SelectedStart->GetPlayLength()))
    {
        bStartActive = false;
    }

    const float StartLength = SelectedStart
        ? SelectedStart->GetPlayLength()
        : 0.0f;

    const float StartTimeRemaining =
        FMath::Max(StartLength - StartAge, 0.0f) / StartPlayRate;

    // Starting은 Start 상태의 유지 조건이다. false가 되어도 포즈 시계는 계속 흐른다.
    Locomotion.Starting = bStartActive &&
        (StartCandidate != nullptr || StartTimeRemaining > StartBlendOutSeconds);
    Locomotion.StartPoseTime = FMath::Clamp(StartAge, 0.0f, StartLength);

    bWasRunRequested = bRunRequested;
}

/** 동기화 그룹의 직전 마커로 좌우 보행 주기를 읽는다. */
void ULMPlayerAnimInstance::UpdateSupportFoot()
{
    const FMarkerSyncAnimPosition P =
        GetSyncGroupPosition(TEXT("LM_Locomotion"));

    if (!P.IsValid())
    {
        return;
    }

    if (P.PreviousMarkerName == TEXT("LM_LeftSupport"))
    {
        bLeftSupport = true;
    }
    else if (P.PreviousMarkerName == TEXT("LM_RightSupport"))
    {
        bLeftSupport = false;
    }
}

/** 정지 진입 순간에 속도 계열과 좌우 발에 맞는 클립을 선택한다. */
void ULMPlayerAnimInstance::SelectStop(float Speed, float RunThreshold)
{
    SelectedStop = Speed >= RunThreshold
        ? (bLeftSupport ? RunStopLeft : RunStopRight)
        : (bLeftSupport ? WalkStopLeft : WalkStopRight);
}

/** 점프 진입 순간에 속도 계열과 좌우 발에 맞는 도약 클립을 선택한다. */
void ULMPlayerAnimInstance::SelectJump(float Speed, float RunThreshold)
{
    SelectedJump = Speed >= RunThreshold
        ? (bLeftSupport ? RunJumpLeft : RunJumpRight)
        : (bLeftSupport ? WalkJumpLeft : WalkJumpRight);
}
