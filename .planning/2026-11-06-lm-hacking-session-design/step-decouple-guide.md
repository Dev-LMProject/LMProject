# 다음 개발 단위 · 해킹과 상호작용의 직접 의존성 분리

2026-11-08 기준 수동 적용 안내. 제품 소스와 BP에는 아직 적용하지 않았다. 기존 v4-r1의 인터페이스/BP 조합 계약을 현재 파일에 맞춘다. E 입력 GA는 이전 단계에서 적용됐으며 이번에는 변경하지 않는다.

목표: InteractionComponent는 일반 상호작용 규칙만 알고, HackingComponent는 해킹 상태만 알게 한다. 소유 액터 BP가 두 컴포넌트를 연결한다.

```text
GA → Interactor → InteractionComponent → 소유 액터의 LMInteractable 구현
                                         ├─ IsInteractionBusy → Hacking.IsInteractionReserved
                                         └─ HandleInteractionRequest → 해킹 상태에 따른 처리
                                               PassThrough일 때만 기존 Interact 실행
```

현재 확인: RequestInteraction 안에 해킹 컴포넌트 검색/태그 분기가 있다. IsInteractionReserved는 선언만 있고 TryStartHacking은 false를 반환한다. 에디터의 BP_NeedHacking/BP_HackingCompleted는 각각 Actor 직접 자식이다. 한쪽을 수정해도 다른 쪽에 상속되지 않으므로 두 BP에 모두 연결한다. BP_InteractTest는 LMInteraction을 가진 일반 액터다. 에셋 이름의 Test와 폐기된 Test1/Test2 GameplayTag를 혼동하지 않는다.

## 1. 기존 LMInteractable에 결과 enum과 함수 두 개 추가

`Source/Lumia/Public/LMInterface/LMInteractable.h`의 generated.h include 아래, UINTERFACE 선언 위에 추가한다. 기존 Interact는 유지한다.

```cpp
// 요청을 별도 기능이 처리했는지, 기존 Interact로 진행할지를 구분한다.
UENUM(BlueprintType)
enum class ELMInteractionRequestHandling : uint8
{
    PassThrough = 0, // 기존 Interact로 진행한다.
    Accepted,        // 별도 기능이 요청을 수락했으므로 여기서 끝낸다.
    Rejected         // 요청을 거절했으므로 여기서 끝낸다.
};
```

같은 파일의 `ILMInteractable::public`에 추가한다.

```cpp
// 소유 액터의 별도 기능이 새 상호작용을 막고 있는지 조회한다.
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
bool IsInteractionBusy() const;
virtual bool IsInteractionBusy_Implementation() const;

// 기존 Interact 실행 전에 요청을 별도 기능에서 처리할 기회를 제공한다.
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
ELMInteractionRequestHandling HandleInteractionRequest(AActor* RequestActor);
virtual ELMInteractionRequestHandling HandleInteractionRequest_Implementation(AActor* RequestActor);
```

인터페이스 함수에는 BlueprintPure를 명시하지 않는다. UE 5.8 UHT는 이 명시를 거절한다. 위처럼 BlueprintCallable과 const 반환 함수를 사용하면 UHT가 자동으로 pure 플래그를 부여하며, 이 자동 지정은 인터페이스 검사에서 허용된다. 컴포넌트 클래스의 IsInteractionReserved에는 기존 BlueprintPure를 유지한다.

`Source/Lumia/Private/LMInterface/LMInteractable.cpp`에 기본 구현을 추가한다.

```cpp
bool ILMInteractable::IsInteractionBusy_Implementation() const
{
    return false;
}

ELMInteractionRequestHandling ILMInteractable::HandleInteractionRequest_Implementation(AActor* RequestActor)
{
    return ELMInteractionRequestHandling::PassThrough;
}
```

일반 문은 두 함수를 구현하지 않아도 기존 Interact로 진행한다. 반환값이 bool 하나면 '별도 처리 없음'과 '해킹 시작 실패'가 섞이므로 세 가지 결과를 사용한다. PassThrough의 값 0은 BP에서 새 함수를 구현하지 않은 경우의 기본 반환과도 일치한다. 이 enum은 헤더 순환 참조를 만들지 않도록 인터페이스 헤더에 둔다.

## 2. 해킹 컴포넌트의 조회/시작 함수를 BP에 노출

`Source/Lumia/Public/LMComponent/LMHackingComponent.h`의 기존 두 선언에 매크로와 주석을 붙인다. 동일 선언을 추가로 중복 작성하지 않는다.

```cpp
// 액터의 상호작용 처리 함수가 요청한 해킹 진입을 시도한다.
UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
bool TryStartHacking(AActor* InInteractor);

// 해킹 진행 또는 정리 중이라 새 상호작용을 받아서는 안 되는지 반환한다.
UFUNCTION(BlueprintPure, Category = "LM|Hacking")
bool IsInteractionReserved() const;
```

`Source/Lumia/Private/LMComponent/LMHackingComponent.cpp`에 추가한다.

```cpp
bool ULMHackingComponent::IsInteractionReserved() const
{
    return bIsCleaningUp
        || Phase == ELMHackingPhase::Starting
        || Phase == ELMHackingPhase::Playing
        || Phase == ELMHackingPhase::Failed
        || Phase == ELMHackingPhase::AwaitingConfirmation;
}
```

Idle과 정상적인 Completed는 예약하지 않는다. 다음 세션 구현에서는 시작 취소의 되돌리기가 끝날 때까지 Starting을 유지하고, 완료 처리에서는 정리와 완료 콜백 전달이 모두 끝날 때까지 보호 상태를 유지해야 한다. Phase를 Completed로 바꿨다는 이유만으로 콜백 중 예약을 풀면 안 된다. 현재 TryStartHacking/ConfirmHacking/ReleaseSession은 미구현이므로 위 조회만 추가해 실제 세션 수명까지 완성됐다고 판단하지 않는다.

HackingComponent.h에서 다음 미사용 선언을 제거한다. 현 cpp에는 이 필드의 사용처가 없다.

```cpp
class ULMInteractionComponent;
TWeakObjectPtr<ULMInteractionComponent> InteractionComponent;
bool bOwnsBusy = false;
```

위 세 줄에 붙은 주석도 함께 제거한다. ReleaseSession의 설명은 '이번 세션이 확보한 UI·입력 제한·월드 상태를 정리한다'로 바꾼다. 해킹 종료에서 일반 상호작용의 SetIsBusy(false)를 호출하지 않는다.

## 3. InteractionComponent의 요청 처리를 일반 인터페이스로 교체

`Source/Lumia/Public/LMComponent/LMInteractionComponent.h`에서 다음 선언을 교체한다. EvaluateInteraction의 기존 포인터 인자는 읽기 전용 참조로 바꾸며, 실제 호출부는 같은 cpp의 RequestInteraction뿐이다.

```cpp
// 일반 Busy, 요청 전달 중 상태, 액터가 제공한 별도 예약을 합산한다.
bool GetIsBusy() const;

// 요청 문맥과 현재 조건을 검사한다. 내부 재검사는 자기 전달 가드만 제외한다.
FLMInteractionResult EvaluateInteraction(AActor* RequestActor, const FGameplayTagContainer& ActionTags, bool bIgnoreDispatchGuard = false);
```

RequestInteraction 선언도 인자 목록을 한 줄로 유지한다. UFUNCTION은 기존 것을 유지한다.

```cpp
FLMInteractionResult RequestInteraction(AActor* RequestActor, UPARAM(ref) FGameplayTagContainer& ActionTags);
```

private에 추가한다.

```cpp
// BP 콜백 안에서 같은 컴포넌트로 다시 들어오는 요청을 막는다.
bool bIsDispatchingRequest = false;
```

`Source/Lumia/Private/LMComponent/LMInteractionComponent.cpp`에서 해킹 전용 include 두 개를 제거한다. 이 파일의 다른 코드에는 해당 타입/태그의 사용이 없다.

```cpp
#include "LMComponent/LMHackingComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
```

scope 종료 시 가드를 자동 복원하도록 아래 include를 추가한다.

```cpp
#include "Templates/UnrealTemplate.h"
```

GetIsBusy 정의를 추가하고, EvaluateInteraction과 RequestInteraction의 기존 본문을 아래로 교체한다. 겹침 감지, 태그 보관, SetIsBusy, 기존 성공/실패 결과 enum 이름은 유지한다.

```cpp
bool ULMInteractionComponent::GetIsBusy() const
{
    if (bIsBusy || bIsDispatchingRequest || bIsEndingPlay || IsBeingDestroyed())
    {
        return true;
    }

    AActor* TargetActor = GetOwner();
    return IsValid(TargetActor)
        && TargetActor->GetClass()->ImplementsInterface(ULMInteractable::StaticClass())
        && ILMInteractable::Execute_IsInteractionBusy(TargetActor);
}

FLMInteractionResult ULMInteractionComponent::EvaluateInteraction(AActor* RequestActor, const FGameplayTagContainer& ActionTags, bool bIgnoreDispatchGuard)
{
    FLMInteractionResult R;
    R.InteractionId = InteractionId;
    AActor* TargetActor = GetOwner();

    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }

    if (!ActionTags.IsValid())
    {
        R.Failure = EInteractionFailure::InvalidActionTags;
        return R;
    }

    if (!TargetActor->GetClass()->ImplementsInterface(ULMInteractable::StaticClass()))
    {
        R.Failure = EInteractionFailure::NotHasInterface;
        return R;
    }

    if (bIsBusy || (!bIgnoreDispatchGuard && bIsDispatchingRequest))
    {
        R.Failure = EInteractionFailure::IsBusy;
        return R;
    }

    // 액터 BP는 여기서 상태만 읽으며, 새로운 요청이나 상태 변경을 실행하지 않는다.
    const bool bReserved = ILMInteractable::Execute_IsInteractionBusy(TargetActor);
    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || GetOwner() != TargetActor || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }

    if (bIsBusy || (!bIgnoreDispatchGuard && bIsDispatchingRequest) || bReserved)
    {
        R.Failure = EInteractionFailure::IsBusy;
        return R;
    }

    if (!MatchesInteractionTags(ActionTags))
    {
        R.Failure = EInteractionFailure::NotHasAllRequireTags;
        return R;
    }

    R.bCanInteraction = true;
    return R;
}

FLMInteractionResult ULMInteractionComponent::RequestInteraction(AActor* RequestActor, FGameplayTagContainer& ActionTags)
{
    FLMInteractionResult R = EvaluateInteraction(RequestActor, ActionTags);
    if (!R.bCanInteraction)
    {
        return R;
    }

    AActor* TargetActor = GetOwner();
    // 최초 평가 뒤부터 모든 외부 콜백과 결과 처리까지 재진입을 막는다.
    TGuardValue<bool> DispatchGuard(bIsDispatchingRequest, true);
    const ELMInteractionRequestHandling Handling = ILMInteractable::Execute_HandleInteractionRequest(TargetActor, RequestActor);

    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || GetOwner() != TargetActor || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.bCanInteraction = false;
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }

    switch (Handling)
    {
    case ELMInteractionRequestHandling::Accepted:
        return R;
    case ELMInteractionRequestHandling::PassThrough:
        break;
    case ELMInteractionRequestHandling::Rejected:
    default:
        R.Failure = EInteractionFailure::ExecutionFailed;
        return R;
    }

    // Handler가 태그·일반 Busy·예약을 바꿨을 수 있으므로 다시 판정한다.
    // 현재 요청이 세운 DispatchGuard만 무시하고 나머지 조건은 모두 검사한다.
    R = EvaluateInteraction(RequestActor, ActionTags, true);
    if (!R.bCanInteraction)
    {
        return R;
    }

    bIsBusy = true;
    const bool bStarted = ILMInteractable::Execute_Interact(TargetActor, RequestActor);
    if (!IsValid(this) || bIsEndingPlay || IsBeingDestroyed() || !IsValid(TargetActor) || !IsValid(RequestActor) || TargetActor->IsActorBeingDestroyed() || RequestActor->IsActorBeingDestroyed())
    {
        R.bCanInteraction = false;
        R.Failure = EInteractionFailure::CantAccessActor;
        return R;
    }

    if (!bStarted)
    {
        bIsBusy = false;
        R.Failure = EInteractionFailure::ExecutionFailed;
    }

    return R;
}
```

주의: 최초 평가 전에 DispatchGuard를 세우면 자기 자신이 Busy로 거절된다. PassThrough 재검사에서 일반 GetIsBusy를 그대로 부르면 역시 자기 가드에 걸린다. 그래서 내부 재검사만 자기 가드를 제외한다. 일반 Busy와 액터 예약까지 무시하면 안 된다.

`bCanInteraction`은 기존 코드처럼 접근 조건 통과 여부다. 별도 처리의 성공 여부는 Failure를 읽는다. 이번에 공개 결과 구조나 기존 Succsess 철자를 변경하지 않는다.

## 4. 빌드 후 해킹 액터 BP 두 곳에 구현

새 UFUNCTION과 enum은 에디터를 종료하고 LumiaEditor / Win64 / Development 빌드 후 에디터를 다시 열어 반영한다. 지난 단계의 '이번만' 빌드 위임을 새 단계의 실행 승인으로 확대하지 않는다. 이번 응답은 적용 안내다.

대상은 `/Game/_BP/BP_NeedHacking`과 `/Game/_BP/BP_HackingCompleted`. 기존 LMInteractable 인터페이스의 함수 목록에서 아래 두 함수를 구현한다. 반환값이 있으므로 별도 함수 그래프에서 Return Node로 결과를 연결한다. 기존 Interact 그래프의 문 열기/본래 행동은 유지한다. C++에서 BP 인터페이스 구현을 호출할 때 Execute_ 래퍼를 사용한다. 근거: https://dev.epicgames.com/documentation/en-us/unreal-engine/interfaces-in-unreal-engine

**IsInteractionBusy 그래프**

1. 함수 진입 → LMHacking 컴포넌트 참조의 Is Valid 검사.
2. 유효하면 LMHacking의 IsInteractionReserved 결과 → Return Value.
3. 무효하면 Return Value=true로 새 요청을 막는다. 누락된 필수 컴포넌트를 일반 상호작용으로 우회시키지 않는다.
4. LMInteraction.GetIsBusy를 다시 호출하지 않는다. IsBusy 값을 다른 BP 변수에 복제하거나 SetIsBusy로 동기화하지 않는다.

Comment: `해킹 예약 상태를 상호작용에 제공한다. 조회만 수행하며 Interaction의 Busy를 다시 조회하거나 변경하지 않는다.`

**HandleInteractionRequest 그래프**

GetHackingStateTag는 값만 읽는 노드이므로 이것만 배치해서는 태그를 검사하지 않는다. 아래처럼 `Equal (GameplayTag)` 비교 노드 두 개와 Branch를 연결한다. 비교 노드는 그래프에서 `==`로 표시된다.

**데이터 핀 연결**

1. LMHacking 참조 → Get Hacking State Tag의 Target.
2. Get Hacking State Tag의 반환값 → 첫 번째 Equal (GameplayTag)의 A.
3. Make Literal Gameplay Tag의 Value를 LM.State.Hacking.NeedHacking으로 선택 → 반환값을 첫 번째 비교 노드의 B에 연결.
4. 첫 번째 비교의 bool 반환값 → 첫 번째 Branch의 Condition.
5. 같은 Getter 반환값 → 두 번째 Equal (GameplayTag)의 A.
6. 두 번째 Make Literal Gameplay Tag의 Value를 LM.State.Hacking.HackedComplete로 선택 → 반환값을 두 번째 비교 노드의 B에 연결.
7. 두 번째 비교의 bool 반환값 → 두 번째 Branch의 Condition.

**흰색 실행 핀 연결**

```text
함수 진입
 └─ Is Valid (Input Object = LMHacking)
     ├─ Is Not Valid → Return Rejected
     └─ Is Valid → Branch ① (Condition = 현재 태그 == NeedHacking)
         ├─ True → Try Start Hacking
         │           └─ Branch ③ (Condition = Try Start Hacking의 bool 반환값)
         │               ├─ True  → Return Accepted
         │               └─ False → Return Rejected
         └─ False → Branch ② (Condition = 현재 태그 == HackedComplete)
                     ├─ True  → Return PassThrough
                     └─ False → Return Rejected
```

Try Start Hacking의 Target은 LMHacking이며, In Interactor 핀에는 HandleInteractionRequest 함수 입력 RequestActor를 연결한다. 이 노드의 실행 출력과 bool 출력은 각각 Branch ③의 실행 입력과 Condition에 연결한다. Getter와 Equal 노드에는 실행 핀이 없다. Is Valid의 실행 출력은 Branch ①로 직접 연결하며, Getter는 비교를 위한 데이터 핀으로만 연결한다.

태그 비교는 부모 태그 포함 비교가 아닌 정확한 태그 일치로 한다. 해킹 컴포넌트의 상태 태그를 읽으며 액터의 종류 태그 Hacking/ActiveTarget과 구분한다. 이 함수에서 기존 Interact나 RequestInteraction을 다시 호출하지 않는다. PassThrough를 받은 C++이 기존 Interact를 정확히 한 번 호출한다.

Comment: `잠긴 대상은 해킹 시작 수락/거절로 요청을 끝낸다. 해금 완료 상태에서만 PassThrough를 반환해 기존 행동으로 진행한다.`

일반 문은 기존 `LM.Interaction.Actor.ActiveTarget` 종류 태그와 Interact 구현을 유지한다. 새 Test1/Test2/Door 태그는 만들지 않는다.

## 5. 이번 단계의 확인 기준

| 대상/상황 | 기대 결과 |
|---|---|
| 일반 문: 새 함수 미구현 | 기존 Interact가 한 번 실행된다. |
| BP_NeedHacking: NeedHacking | 현재 TryStartHacking이 false이므로 Rejected, 기존 Interact는 실행되지 않는다. |
| BP_HackingCompleted: HackedComplete | PassThrough 후 기존 Interact가 한 번 실행된다. |
| 일반 bIsBusy=true | Handler와 Interact 모두 실행되지 않는다. |
| 해킹 태그 미지원/필수 컴포넌트 무효 | 기존 Interact가 실행되지 않는다. |
| Handler 안에서 같은 RequestInteraction 재호출 | 중첩 요청은 Busy로 거절된다. |
| Handler가 Busy/종류 태그를 바꾸고 PassThrough | 변경된 상태로 재검사하며 조건 불충족 시 Interact가 실행되지 않는다. |
| Handler가 소유 액터/컴포넌트를 파괴 | 후속 Interact를 호출하지 않는다. |

확인용 Print String이 필요하면 테스트 액터의 기존 Interact 진입에 임시로 하나만 둔다. 테스트 뒤 제거한다. 해킹 창 열기·예약 유지·완료 콜백 중 재진입 테스트는 실제 세션 구현 후 수행한다. 지금은 시작 성공을 강제로 true로 바꿔 완료를 가장하지 않는다.

소스 검색으로 InteractionComponent의 Hacking 타입/해킹 상태 태그 참조 0건, HackingComponent의 InteractionComponent/bOwnsBusy 참조 0건을 확인한다. 두 해킹 BP가 각각 두 함수와 모든 Return 분기를 구현했는지 확인한다. 구성 전체가 누락되면 일반 액터 기본값으로 동작하므로 이 에셋 검사를 생략할 수 없다.

이번 단계 완료 후 다음은 '해킹 세션 시작·UI 표시·취소/자원 복원'이다. 성공 확인은 잠금만 해제하고, 다음 E에서 본래 기능을 실행한다는 기존 결정은 유지한다.
