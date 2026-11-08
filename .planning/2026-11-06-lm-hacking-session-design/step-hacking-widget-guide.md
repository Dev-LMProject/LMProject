# 해킹 화면 준비 · 수동 안내

2026-11-08 / 기준 d37d27e / 제품에 적용하지 않은 예시.

승인된 v3+v4-r1의 세션 단위를 2-1 화면 준비, 2-2 실제 시작·취소·복원으로 나눈다. 책임이나 플레이 계약은 바꾸지 않는다. 이번 완료 기준은 C++ 위젯 부모와 최소 WBP를 만들고, 닫기 버튼과 ESC의 요청 경로 및 PC의 클래스 설정을 준비하는 것이다. E로 화면을 열고 실제로 닫는 동작은 2-2에서 구현한다.

**현재 상태**

- TryStartHacking은 false, IsCurrentSessionValid도 false, CancelHacking/ConfirmHacking/ReleaseSession은 비어 있다.
- ULMHackingWidget 및 해킹 화면 WBP가 없다. PC에는 UserHUDClass만 있다.
- 화면은 자기 세션의 해킹 컴포넌트만 약참조하고 사용자 요청을 전달한다. 세션과 화면 수명·pause·태그는 해킹 컴포넌트가 소유한다. InteractionComponent를 참조하지 않는다.
- 아래 코드는 현재 미구현 세션을 완성한 것처럼 호출하지 않는다. 특히 발신 위젯 인자만 추가했다고 오래된 위젯 차단이 완성되는 것은 아니다. 2-2에서 현재 위젯의 동일성·세션 유효성 검사와 외부 제거/시작 중 제거 처리를 함께 구현한 후 화면을 실제로 연다.

**1. 모듈 의존성**

Source/Lumia/Lumia.Build.cs의 PublicDependencyModuleNames 기존 목록에 "SlateCore"를 추가한다. 공개 위젯 헤더에서 사용하는 FReply/FKeyEvent 등 Slate 타입의 의존성을 명시한다. UMG와 InputCore는 이미 있으므로 유지한다.

**2. 새 위젯 부모**

Source/Lumia/Public/LMUI/LMHackingWidget.h:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LMHackingWidget.generated.h"

class ULMHackingComponent;

UCLASS(Abstract, Blueprintable)
class LUMIA_API ULMHackingWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    ULMHackingWidget(const FObjectInitializer& ObjectInitializer);

    // 이번 화면의 요청을 받을 해킹 컴포넌트를 연결한다.
    void InitializeForSession(ULMHackingComponent* InHackingComponent);

    // 닫힌 화면이 이전 세션에 요청하지 못하도록 연결을 끊는다.
    void ClearSession();

    // 닫기 버튼과 ESC의 사용자 요청을 한 경로로 전달한다.
    UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
    void RequestClose();

protected:
    // 자식 버튼에 포커스가 있어도 ESC를 받아 처리한다.
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    TWeakObjectPtr<ULMHackingComponent> HackingComponent;
};
```

Source/Lumia/Private/LMUI/LMHackingWidget.cpp:

```cpp
#include "LMUI/LMHackingWidget.h"

#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "LMComponent/LMHackingComponent.h"

ULMHackingWidget::ULMHackingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void ULMHackingWidget::InitializeForSession(ULMHackingComponent* InHackingComponent)
{
    HackingComponent = InHackingComponent;
}

void ULMHackingWidget::ClearSession()
{
    HackingComponent.Reset();
}

void ULMHackingWidget::RequestClose()
{
    ULMHackingComponent* Component = HackingComponent.Get();
    if (!IsValid(Component))
    {
        return;
    }

    switch (Component->GetPhase())
    {
    case ELMHackingPhase::Starting:
    case ELMHackingPhase::Playing:
    case ELMHackingPhase::Failed:
        Component->CancelHacking(this);
        break;

    case ELMHackingPhase::AwaitingConfirmation:
        Component->ConfirmHacking(this);
        break;

    default:
        break;
    }
}

FReply ULMHackingWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        // 누르고 있는 ESC가 닫기 요청을 반복하지 않도록 한다.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
```

RequestClose는 사용자의 닫기 의도만 전달한다. RemoveFromParent/SetPause/SetInputMode는 여기서 호출하지 않는다. AwaitingConfirmation의 외부 제거는 사용자 확인으로 간주하지 않으며 2-2에서 별도의 강제 취소 경로로 구현한다.

**3. 컴포넌트의 두 선언·정의 일치**

LMHackingComponent.h의 전방 선언에 `class ULMHackingWidget;`를 추가한다. 두 UFUNCTION은 그대로 두고 함수 파라미터를 변경한다.

```cpp
UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
void CancelHacking(ULMHackingWidget* SourceWidget);

UFUNCTION(BlueprintCallable, Category = "LM|Hacking")
void ConfirmHacking(ULMHackingWidget* SourceWidget);
```

LMHackingComponent.cpp의 기존 빈 정의도 바꾼다.

```cpp
void ULMHackingComponent::CancelHacking(ULMHackingWidget* SourceWidget)
{
    // shortcut: 세션 정리 구현 전에는 요청을 처리하지 않으며, 다음 세션 연결 단계에서 구현한다.
}

void ULMHackingComponent::ConfirmHacking(ULMHackingWidget* SourceWidget)
{
    // shortcut: 성공 확정 구현 전에는 해금하지 않으며, 퍼즐 완료 연결 단계에서 구현한다.
}
```

SourceWidget은 닫기를 요청한 화면의 신원이다. 다음 세션 단계에서 현재 화면과 비교해야 하며, 이번 빈 함수는 비교/복원을 수행하지 않는다. Cancel의 실제 구현은 세션 연결, Confirm의 완료 확정은 퍼즐 연결 단위에서 한다. SubmitMove/RetryPuzzle의 발신 인자는 해당 UI 호출부를 연결할 때 추가한다.

현재 C++ 호출부 검색에서는 기존 선언/정의 외 사용처가 없다. 기존 BP에서 이 두 함수를 직접 썼다면 노드 Refresh 후 Source Widget 핀을 실제 발신 위젯으로 연결한다. None을 기본 허용하는 계약을 추가하지 않는다.

같은 헤더의 과거 책임 주석 두 곳도 실제 결정에 맞게 바꾼다.

```cpp
// 이 세션이 확보한 화면·입력·일시정지·태그를 정리한다.
void ReleaseSession();

// 세션을 시작한 원래 컨트롤러. 입력 복원 대상을 확인할 때 사용한다.
TWeakObjectPtr<ALMPlayerController> SessionController;
```

**4. 컨트롤러에는 위젯 클래스 기본값**

Source/Lumia/Public/LMPlayer/LMPlayerController.h:

```cpp
// 기존 전방 선언 옆에 추가.
class ULMHackingWidget;
```

public:

```cpp
// 해킹 컴포넌트가 생성할 화면의 BP 클래스를 반환한다.
TSubclassOf<ULMHackingWidget> GetHackingWidgetClass() const { return HackingWidgetClass; }
```

private:

```cpp
// 로컬 플레이어가 사용할 해킹 화면의 기본 클래스.
UPROPERTY(EditDefaultsOnly, Category = "LM|Hacking")
TSubclassOf<ULMHackingWidget> HackingWidgetClass;
```

활성 세션/위젯 인스턴스를 PC에 보관하지 않는다. 기존 E GA 및 휠·상하키 경로는 이번 변경 대상이 아니다.

**5. 빌드 후 최소 WBP 구성**

1. 현재 에디터 작업을 저장하고 에디터를 닫는다. LumiaEditor / Win64 / Development를 빌드하고 다시 연다. 새 UCLASS와 UFUNCTION 파라미터 변경을 UHT에 반영하는 과정이다.
2. /Game/LMUI/LMHacking에 Widget Blueprint `WBP_LM_HackingPuzzle`을 만들고 Parent Class를 LMHackingWidget으로 지정한다. 기존 UI 폴더/에셋을 옮기지는 않는다.
3. Class Defaults에서 Is Focusable=true 확인. Designer에 CanvasPanel, 중앙 Border(예: 640×360), VerticalBox, 제목/설명 Text, Spacer, Button_Close와 그 자식의 닫기 Text를 만든다. 퍼즐 셀/숫자 선택/남은 횟수/성공 문구는 퍼즐 단위에서 추가한다.
4. Button_Close.OnClicked → Request Close(Target=self) 한 줄을 연결한다. ESC는 native preview에서 같은 함수로 전달된다. PrintString/RemoveFromParent는 추가하지 않는다.
5. 그래프 Comment: “사용자 닫기 요청: 진행·실패는 취소, 성공 확인 화면은 완료를 요청한다. 화면 제거와 입력 복원은 해킹 컴포넌트가 담당한다.”
6. /Game/LMCharacter/LMPlayer/BP_LM_PlayerController의 Class Defaults → LM/Hacking → Hacking Widget Class = WBP_LM_HackingPuzzle로 설정한다.
7. WBP 및 Controller BP를 컴파일·저장한다. 아직 Construct/BeginPlay에서 임시로 AddToViewport 하지 않는다. 실제 생성/부착은 예약과 정리가 준비된 2-2에서 연결한다.

**확인 범위**

| 확인 | 기대 결과 |
|---|---|
| C++/UHT | 새 위젯 클래스, 두 함수 서명, SlateCore 의존성이 빌드됨 |
| WBP | 부모가 LMHackingWidget이고 Request Close 노드가 나타남 |
| Designer | 최소 화면과 닫기 버튼 배치 확인 |
| Controller BP | 위젯 클래스 목록에서 해당 WBP 선택/컴파일/저장 가능 |
| E 입력 | 현재 TryStartHacking=false이므로 화면이 열리지 않는 상태 유지 |

이 단위의 컴파일/에셋 구성 검증을 실제 닫기·ESC·복원 테스트와 구분한다. 다음 단위는 “E → 세션 시작 → 취소/자원 복원”이다. 그때 현재 위젯 identity 검사, Starting 중 취소 보류, 외부 제거·Pawn/PC 종료 감지, 원래 ASC 태그 및 입력/pause 복구를 연결한다. 승인된 생명주기 계약을 누락한 채 이 빈 화면을 실제 게임 진입점에 먼저 연결하지 않는다.

검증 근거: 현 소스와 설치 UE 5.8 UserWidget.h/Input/Events.h, Epic [OnPreviewKeyDown](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UUserWidget/OnPreviewKeyDown?lang=en-US). 현재 단위의 코드는 안내용이며 적용·빌드·UHT·BP 컴파일·PIE는 수행하지 않았다.
