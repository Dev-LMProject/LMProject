# 변경안 적용 1 · E 입력을 GAS로 통합

기준은 shd/dev_HackingSystem @ b06177d와 v4-r1 검토안이다. 이 문서는 사용자가 직접 적용할 예시다. 아래 코드는 Source나 BP에 적용하지 않았으며, 빌드·UHT·BP 컴파일·게임 검증은 사용자가 수행한다.

2026-11-07 재정리: UI 입력을 공통 매니저/CommonUI로 통합하는 제안은 사용자가 취소했다. 이 문서는 기존 휠·상하 키 바인딩과 HUD의 선택 소유 구조를 유지하면서 E만 기존 GAS 입력에 연결하는 단위다. 최신 확인에서 Git 작업 트리는 깨끗했고 Interact 입력 태그, PC 선택 getter, 입력 GA는 없으며 기존 raw E 바인딩이 남아 있다. 아래 순서대로 처음부터 적용하는 기준이다. Hacking/Interaction의 직접 결합 분리는 다음 단위로 남긴다.

이번 완료 기준은 일반 대상에서 E 한 번이 기존 Interactor.TryInteract로 한 번 전달되는 것이다. 원래 선택 대상과 요청 Pawn을 유지하고, 요청 도중 GA가 취소되더라도 새 활성화를 잘못 종료하지 않는다.

다음 개발 단위는 Interaction/Hacking의 직접 참조를 일반 인터페이스와 액터 BP로 분리하는 작업이다. 해킹 세션 구현은 그 뒤에 이어간다.

## 1. 입력 태그 추가

아래 두 파일의 기존 LMGameplayTags 네임스페이스에 각각 추가한다.

[태그 헤더](C:/Project/LMProject/Dev/Source/Lumia/Public/LMGameplayTags/LMGameplayTags.h)

~~~cpp
LUMIA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Interact);
~~~

[태그 구현](C:/Project/LMProject/Dev/Source/Lumia/Private/LMGameplayTags/LMGameplayTags.cpp)

~~~cpp
UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Interact, "LM.InputTag.Interact", "선택한 대상의 상호작용 요청");
~~~

태그 문자열은 IMC의 물리 키와 별개다. InputConfig와 AbilitySet에 똑같은 LM.InputTag.Interact를 지정해야 입력과 능력이 연결된다.

## 2. 컨트롤러에서 선택 대상 조회 제공

[LMPlayerController.h](C:/Project/LMProject/Dev/Source/Lumia/Public/LMPlayer/LMPlayerController.h)의 public에 추가한다. C++ GA만 호출하므로 UFUNCTION은 필요하지 않다.

~~~cpp
/** 상호작용 HUD의 현재 선택 액터를 반환한다. HUD가 없거나 선택이 무효하면 nullptr. */
AActor* GetSelectedInteractionActor() const;
~~~

[LMPlayerController.cpp](C:/Project/LMProject/Dev/Source/Lumia/Private/LMPlayer/LMPlayerController.cpp)에 추가한다.

~~~cpp
AActor* ALMPlayerController::GetSelectedInteractionActor() const
{
    return IsValid(InteractionHUD) ? InteractionHUD->GetSelectedActor() : nullptr;
}
~~~

같은 파일의 SetupInputComponent에서 E를 연결하는 다음 호출을 삭제한다.

~~~cpp
InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::InteractWithSelectedTarget).bConsumeInput = true;
~~~

현재 파일에서는 이 호출이 두 줄로 나뉘어 있다. 호출 전체를 삭제한다. 위쪽 주석은 휠·방향키의 목록 선택을 설명하는 내용으로 고친다.

이제 호출자가 사라지는 아래 두 함수는 선언과 정의를 함께 삭제한다.

- LMPlayerController.h/.cpp의 InteractWithSelectedTarget()
- [LMInteractionHUDWidget.h](C:/Project/LMProject/Dev/Source/Lumia/Public/LMUI/LMInteractionHUDWidget.h) / [LMInteractionHUDWidget.cpp](C:/Project/LMProject/Dev/Source/Lumia/Private/LMUI/LMInteractionHUDWidget.cpp)의 TryInteractSelected()

현재 C++ 호출부는 컨트롤러 → HUD의 한 경로뿐이며 두 함수 모두 UFUNCTION이 아니다. HUD의 GetSelectedActor(), 목록 갱신, 선택 기능은 다음 GA가 계속 사용한다.

이전 라이브 확인에서 BP_LM_PlayerCharacter에는 별도 InputKey E 노드가 없었다. 이번 재정리에서 BP 그래프를 다시 읽지는 않았다. 다른 캐릭터 BP에 옛 E → 상호작용 호출이 남아 있다면 그 실행 연결도 제거한다. 입력을 소비하던 컨트롤러의 BindKey를 삭제한 뒤 옛 BP 경로와 새 GA가 함께 실행되지 않도록 확인하는 항목이다.

## 3. 일회성 입력 GA의 C++ 부모 추가

이 예시는 요청 문맥·동기 취소를 C++ 부모에서 처리하고 GA BP에는 기본 설정만 둔다. 기존 GameplayAbilities 모듈 의존성을 사용한다.

새 파일 위치

- C:/Project/LMProject/Dev/Source/Lumia/Public/LMGAS/LMInteractInputAbility.h
- C:/Project/LMProject/Dev/Source/Lumia/Private/LMGAS/LMInteractInputAbility.cpp

LMInteractInputAbility.h 전체

~~~cpp
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LMInteractInputAbility.generated.h"

UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMInteractInputAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    ULMInteractInputAbility();

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
    uint64 ActivationSerial = 0;
};
~~~

LMInteractInputAbility.cpp 전체

~~~cpp
#include "LMGAS/LMInteractInputAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "LMComponent/LMInteractorComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "LMPlayer/LMPlayerController.h"

ULMInteractInputAbility::ULMInteractInputAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
    bRetriggerInstancedAbility = false;
    bReplicateInputDirectly = false;

    ActivationBlockedTags.AddTag(LMGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(LMGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(LMGameplayTags::State_Input_Blocked);
}

void ULMInteractInputAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const uint64 ThisActivation = ++ActivationSerial;
    const TWeakObjectPtr<ULMInteractInputAbility> WeakThis(this);
    const TWeakObjectPtr<APawn> RequestPawn(Cast<APawn>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
    const TWeakObjectPtr<ALMPlayerController> RequestController(Cast<ALMPlayerController>(ActorInfo ? ActorInfo->PlayerController.Get() : nullptr));
    const TWeakObjectPtr<UAbilitySystemComponent> RequestASC(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
    const TWeakObjectPtr<ULMInteractorComponent> Interactor(RequestPawn.IsValid() ? RequestPawn->FindComponentByClass<ULMInteractorComponent>() : nullptr);
    const TWeakObjectPtr<AActor> Target(RequestController.IsValid() ? RequestController->GetSelectedInteractionActor() : nullptr);

    const auto IsCurrentActivation = [WeakThis, ThisActivation]()
    {
        return WeakThis.IsValid() && WeakThis->ActivationSerial == ThisActivation && WeakThis->IsActive();
    };

    const auto HasRequestContext = [WeakThis, RequestPawn, RequestController, RequestASC, Interactor, Target]()
    {
        return RequestPawn.IsValid()
            && RequestController.IsValid()
            && RequestASC.IsValid()
            && Interactor.IsValid()
            && Target.IsValid()
            && RequestController->IsLocalPlayerController()
            && RequestController->GetPawn() == RequestPawn.Get()
            && RequestPawn->GetController() == RequestController.Get()
            && RequestASC->GetAvatarActor() == RequestPawn.Get()
            && RequestASC->GetOwnerActor() == RequestController->GetPlayerState<APlayerState>()
            && RequestASC->AbilityActorInfo.IsValid()
            && RequestASC->AbilityActorInfo->PlayerController.Get() == RequestController.Get()
            && Interactor->GetOwner() == RequestPawn.Get()
            && !RequestASC->HasAnyMatchingGameplayTags(WeakThis->ActivationBlockedTags)
            && !UGameplayStatics::IsGamePaused(RequestPawn.Get());
    };

    const auto FinishActivation = [WeakThis, RequestASC, Handle, ActivationInfo, IsCurrentActivation](bool bWasCancelled)
    {
        if (!IsCurrentActivation() || !RequestASC.IsValid() || !RequestASC->AbilityActorInfo.IsValid())
        {
            return;
        }

        WeakThis->EndAbility(Handle, RequestASC->AbilityActorInfo.Get(), ActivationInfo, false, bWasCancelled);
    };

    if (!IsCurrentActivation() || !HasRequestContext())
    {
        FinishActivation(true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        FinishActivation(true);
        return;
    }

    // Commit의 콜백에서 조종 대상이 바뀌거나 이 GA가 종료될 수 있다.
    if (!IsCurrentActivation() || !HasRequestContext())
    {
        FinishActivation(true);
        return;
    }

    const FLMInteractionResult Result = Interactor->TryInteract(Target.Get());
    if (Result.Failure != EInteractionFailure::Succsess)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Interaction request failed: %d"), static_cast<int32>(Result.Failure));
    }

    // 요청 중 입력 정리가 이 GA를 취소했으면 종료를 반복하지 않는다.
    FinishActivation(Result.Failure != EInteractionFailure::Succsess);
}
~~~

이 코드에서 확인할 점

- Commit 전에 대상과 Pawn/PC/ASC를 보관하므로 콜백 후 새 대상을 다시 선택해 실행하지 않는다.
- 대상 등록·종류 태그·Busy·잠금 분기는 기존 Interactor.TryInteract/Interaction.RequestInteraction이 검사한다.
- ASC의 Owner는 PlayerState, Avatar는 현재 Pawn이다.
- ActivationSerial은 같은 GA 인스턴스의 새 실행을 구분한다. 이전 요청이 돌아왔을 때 새 실행을 종료하지 않는다.
- HasRequestContext는 항상 IsCurrentActivation을 먼저 확인한 뒤 호출한다.
- 이 GA는 해킹 세션을 보관하거나 정리하지 않는다. GA의 OnEndAbility에서 CancelHacking 또는 위젯 제거를 호출하지 않는다.
- Native ActivateAbility가 실제 실행을 담당하므로 GA BP에는 별도의 ActivateAbility 그래프를 만들지 않는다.

## 4. C++ 빌드와 에디터 재실행

새 UCLASS와 네이티브 태그를 추가했으므로 에디터를 닫고 LumiaEditor / Win64 / Development를 빌드한 다음 에디터를 다시 연다. 컴파일·UHT 오류가 없고 부모 클래스 LMInteractInputAbility 및 LM.InputTag.Interact를 에디터에서 찾을 수 있어야 한다.

## 5. 입력 액션과 GA BP 생성

콘텐츠 브라우저의 기존 /Game/LMCore/LMInput 폴더에 Input Action을 만들고 IA_LM_Interact로 이름을 지정한다.

- Value Type을 Boolean으로 설정한다.
- 일반적인 한 번 누르기 입력으로 두고 Trigger/Modifier를 추가하지 않는다. 기존 BindAbilityInputs가 Started를 ASC에 보낸다.

기존 /Game/GAS/GA 폴더에서 Blueprint Class → All Classes → LMInteractInputAbility를 찾아 GA_LM_InteractInput을 만든다. 기존 폴더를 재사용한다. Class Defaults에서 다음 값을 확인한다.

| 항목 | 값 |
|---|---|
| Instancing Policy | Instanced Per Actor |
| Net Execution Policy | Local Only |
| Retrigger Instanced Ability | false |
| Replicate Input Directly | false |
| Activation Blocked Tags | LM.State.Dead, LM.State.Stunned, LM.State.Input.Blocked |
| Activation Owned Tags | 비움 |
| Cost / Cooldown Gameplay Effect | 비움 |

새 BP를 컴파일하고 저장한다.

## 6. 기존 세 에셋에 연결

각 에셋의 기존 항목을 유지하고 아래 행을 하나씩 추가한다.

| 에셋 | 추가할 설정 |
|---|---|
| /Game/LMCore/LMInput/Action/IMC_LM_Player | IA_LM_Interact → E |
| /Game/LMCharacter/LMPlayer/Data/DA_LM_PlayerInputConfig | Ability Inputs에 Input Action = IA_LM_Interact, Input Tag = LM.InputTag.Interact |
| /Game/LMCharacter/LMPlayer/Data/DA_LM_PlayerStartupAbilities | Abilities에 Ability Class = GA_LM_InteractInput, Ability Level = 1, Input Tag = LM.InputTag.Interact |

이 입력 태그에 연결된 Interact GA는 한 항목만 둔다. 세 에셋을 저장한 뒤 새 플레이 세션을 시작한다. 현재 PlayerState는 StartupAbilitySet을 한 번 부여하므로, 이미 실행 중인 플레이 세션에 설정 변경을 반영하려면 중지 후 다시 시작한다.

## 7. 현재 코드로 확인하기

일반 상호작용 액터의 기존 Interact에 실행 횟수 표시를 두면 한 번 요청 여부를 확인하기 쉽다. 반복 시험용 동기 액터는 동작 끝에서 기존 일반 Busy를 SetIsBusy(false)로 해제하고 true를 반환한다. 비동기 문이라면 실제 문 동작이 끝날 때 해제한다.

| 입력/조건 | 기대 결과 |
|---|---|
| 일반 문을 선택하고 E 한 번 | 기존 Interact 1회 |
| E를 계속 누름 | 추가 Interact 없음 |
| E를 떼고 다시 누름 | 대상의 일반 Busy가 해제됐다면 1회 추가 |
| 선택 대상 없이 E | 실행 없이 GA 종료, 이후 정상 사용 가능 |
| ASC가 입력 차단·사망·기절 상태 | Interact 0회 |
| 잠긴 해킹 단말에서 E | 현재 TryStartHacking의 false로 거절, 본래 Interact 0회 |
| 대상 소멸/범위 이탈 | 기존 유효성·등록 검사에서 거절 |

아직 해킹 화면이 없어도 동기 취소를 검사할 수 있다. 시험 액터의 Interact에서 요청 Pawn의 Controller를 얻어 CancelAbilityInput을 먼저 호출하고, 이어 실행 횟수 기록 → 일반 Busy 해제 → true 반환으로 연결한다. E 한 번에 횟수가 1 증가하고 다음 E도 정상 동작해야 한다. 시험 후 이 임시 입력 취소 연결을 제거한다.

추가 검증에서는 CommitAbility가 발생시키는 시험 콜백에서 선택 대상을 바꾸거나 Pawn을 교체하는 상황을 만든다. 선택만 바뀌었다면 기존 요청 대상을 유지하고, Pawn이 바뀌었다면 요청을 중단해야 한다. 이 검증과 실제 해킹 진입 중 취소·재활성화는 실행 확인이 필요한 항목이다.

현재 완료한 것은 소스/API 대조와 적용 예시 작성이다. 코드 적용·빌드·플레이 통과로 기록하지 않는다.

예시 정적 검토: 기존 Ultra 검토자가 UE 5.8 헤더와 호출부를 추가 대조해 명백한 컴파일 API 오류, 동기 취소 및 활성화 일련번호 처리의 필수 수정 사항을 찾지 못했다. 이 결과는 실제 컴파일 통과를 의미하지 않는다.
