# LM 프로젝트 명명 규칙

기본 명명 규칙은 언리얼 엔진의 공식 규칙을 따른다. 프로젝트에서 직접 만드는 타입, 애셋, 폴더에는 프로젝트 식별자 `LM`을 추가한다.
이 문서는 Codex가 새 작업에서 참고할 프로젝트 지침이다. 기존 구현의 이름을 새 규칙의 기준으로 삼지 않는다.

## 지침 기준과 관리

- 이 프로젝트의 지침 원본은 `Lumia.uproject`가 있는 개발 프로젝트 폴더(`Dev`)에서 Git으로 관리하는 `AGENTS.md`다. 프로젝트 지침의 확인·적용·추가·수정은 이 파일을 기준으로 한다.
- 작업 시작·재개 및 지침 변경 전에 이 원본의 현재 내용을 읽는다. 대화의 작업 폴더가 개발 프로젝트와 다르면 실제 개발 프로젝트의 원본을 확인하고 수정하며, 변경을 대화 전용 작업 폴더에만 남기지 않는다.
- 다른 체크아웃·개인 설정·메모리의 사본을 프로젝트 지침의 별도 원본으로 관리하지 않는다. 세부 작업 절차는 이 파일에서 연결하는 저장소의 공유 스킬과 문서를 따른다.

## 적용 범위

- 새로 만드는 프로젝트 코드, 애셋, 사용자 정의 폴더에 적용한다.
- 규칙 문서화만을 이유로 기존 코드, 애셋, 폴더를 일괄 변경하지 않는다.
- 아래 이름은 형식을 설명하는 예시이며, 해당 클래스나 기능을 구현하라는 요구사항이 아니다.

## 작업 방식

- 규칙을 문서화해 달라는 요청에는 지침 파일만 수정하며, 이를 이유로 실제 코드나 애셋을 변경하지 않는다.
- 사용자의 명시적 요청이나 승인 없이 브랜치, 워크트리 또는 워크스페이스를 만들지 않는다.
- 파일 수정 요청만으로 커밋·푸시·PR 생성을 수행하지 않는다. 각각 사용자가 명시적으로 요청하거나 승인한 경우에만 수행한다.
- 상위 지침이나 스킬이 자동 브랜치·워크트리 생성 또는 커밋을 요구하더라도 위 제한을 우선한다.
## 프로젝트 플레이 전제

- 이 프로젝트는 단일 플레이어의 싱글 로컬플레이 게임이다.
- 플레이 중 PlayerController 교체는 없다.
- 사용자가 전제 변경을 명시하기 전에는 멀티플레이·다중 로컬 플레이어·Controller 교체를 대비한 구조를 선행 구현하지 않는다.

## 오토포커싱 설계 기준

- 오토포커싱의 실행과 대상 선정은 GA를 중심으로 구현한다.
- 후보 목록은 현재 플레이어의 감지 범위와 카메라 시야 조건을 만족해 감지된 대상만 보관한다. 월드 전체의 대상이 미리 등록되는 공용 명단으로 취급하지 않는다.
- 대상이 감지 범위 또는 시야에서 벗어나면 즉시 후보 목록에서 제거한다. 유지 유예를 이유로 범위·시야 밖 대상을 후보 목록에 남기지 않는다.
- 현재 포커스 대상도 후보 목록 안의 유효한 대상이어야 한다. 후보에서 제외되면 같은 갱신에서 포커스를 해제하거나 다른 유효 후보로 변경한다.
- 후보 목록·현재 포커스·변경 알림은 기존 PlayerController에 직접 둔다. 이 상태 보관만을 위해 별도의 FocusComponent를 만들지 않는다.
- 현재 오토포커싱을 위해 TargetRegistrySubsystem이나 월드 공용 등록 Subsystem을 만들지 않는다.
- 실제 요구가 생기기 전에는 Controller에 직접 둘 수 있는 소규모 상태 관리를 별도 컴포넌트로 분리하지 않는다. 대상의 표적점 등 개별 오브젝트 정보와 플레이어의 후보·선택 상태는 구분한다.

## 코드 작성 규칙

- 실제 코드를 작성하거나 예시 코드를 제시할 때, 파라미터·인자 목록(함수 선언·정의·호출 등)은 길이나 개수와 관계없이 반드시 한 줄로 작성하며 절대 줄바꿈하지 않는다.
- 실제 코드와 예시 코드에서 멤버변수와 같은 의미의 값을 받는 파라미터는 해당 멤버변수의 이름을 그대로 사용하되 앞에 밑줄(`_`)을 붙인다. 예: `MoveSpeed` → `_MoveSpeed`, `bIsDead` → `_bIsDead`.
- 실제 C++ 코드와 예시 코드 모두 함수 선언·정의 바로 위에 한국어로 함수의 역할을 설명하는 주석을 작성한다. 생성자와 오버라이드 함수에도 적용한다.
- 함수 내부의 핵심 처리(초기화, 소유권·수명 연결, 중요한 검증·분기, 상태 변경·정리)에는 목적과 이유를 설명하는 한국어 주석을 작성한다.
- 실제 코드와 예시 코드에서 한 줄 주석은 `//`로 작성하고, 두 줄 이상의 주석은 `/* ... */`로 묶어 작성한다.
- 실제 코드와 예시 코드에서 `if` 하위에 실행문이 한 줄만 들어가는 경우 중괄호를 생략한다.
- `if` 조건문이 50자 이하이고 진입 시 실행하는 코드가 단순한 `return`이면 조건문과 `return`을 같은 줄에 작성한다.
- 단, 해당 `if`문에 이후 코드가 추가될 가능성이 있거나 한 줄에 넣기에 너무 길면 중괄호를 사용한다. 이 예외를 위 두 규칙보다 우선 적용한다.

## C++ 타입과 파일

타입 이름은 `언리얼 타입 접두어 + LM + 내용 및 역할`로 작성한다. `LM`은 대문자로 고정하고, 내용 및 역할은 PascalCase로 작성한다.

| 대상 | 형식 | 예시 |
| --- | --- | --- |
| Actor 계열 클래스 | `ALM역할` | `ALMPlayerCharacter`, `ALMPlayerController` |
| UObject 계열 클래스 | `ULM역할` | `ULMCombatComponent`, `ULMHealthWidget` |
| 구조체 | `FLM역할` | `FLMDamageInfo` |
| 열거형 | `ELM역할` | `ELMCharacterState` |
| 언리얼 인터페이스 | `ULM역할` / `ILM역할` | `ULMInteractable` / `ILMInteractable` |

- 언리얼 타입 접두어는 `LM` 앞에 유지한다. 예: `ALMPlayerCharacter`.
- 다른 종류의 C++ 타입도 언리얼의 해당 타입 접두어 규칙을 유지한다.
- C++ 파일명은 타입 접두어를 제외한 이름을 사용한다. 예: `LMPlayerCharacter.h`, `LMPlayerCharacter.cpp`.
- 함수와 변수에는 프로젝트 식별자 `LM`을 일괄 추가하지 않고 기본 언리얼 명명 규칙을 적용한다. 예: `ApplyDamage()`, `MoveSpeed`, `bIsDead`.
- 실제 코드와 예시 코드의 변수명·함수명은 가능한 한 초등 수준의 쉬운 영어 단어로 작성하며, 이름만 보고 변수의 의미와 함수의 동작을 직관적으로 알 수 있게 한다.
- 엔진에서 상속한 함수나 외부 API의 이름은 해당 계약을 유지한다.

## 애셋

애셋 이름은 `애셋타입축약어_LM_내용및역할`로 작성한다. 타입, 프로젝트 식별자, 내용 및 역할은 밑줄로 구분하고, 각 의미 단위는 PascalCase로 작성한다.
세부 용도를 구분해야 하는 경우 내용 뒤에 설명을 추가할 수 있다. 예: `T_LM_CharacterBody_Normal`.

애셋 타입 축약어는 Epic의 공식 권장사항을 기준으로 한다.

| 애셋 타입 | 축약어 | 예시 |
| --- | --- | --- |
| Blueprint | `BP` | `BP_LM_PlayerCharacter` |
| Animation Blueprint | `ABP` | `ABP_LM_PlayerCharacter` |
| Widget Blueprint | `WBP` | `WBP_LM_HealthBar` |
| Blueprint Interface | `BI` | `BI_LM_Interactable` |
| Static Mesh | `SM` | `SM_LM_LongSword` |
| Skeletal Mesh | `SK` | `SK_LM_PlayerCharacter` |
| Material | `M` | `M_LM_CharacterBody` |
| Material Instance | `MI` | `MI_LM_CharacterBody` |
| Texture | `T` | `T_LM_CharacterBody_Normal` |
| Animation Montage | `AM` | `AM_LM_PlayerAttack` |
| Animation Sequence | `AS` | `AS_LM_PlayerIdle` |
| Blend Space | `BS` | `BS_LM_PlayerLocomotion` |
| Data Table | `DT` | `DT_LM_CharacterStats` |

공식 권장사항이나 이 문서에 없는 축약어를 제안할 때는 이미 확정된 프로젝트 규칙과 구분한다.

## 폴더

직접 만드는 폴더는 `LM + 폴더 내용 및 역할`로 작성한다. `LM` 뒤에 밑줄을 넣지 않으며, 하위 사용자 정의 폴더에도 같은 규칙을 적용한다.

- 예: `LMCharacter`, `LMPlayer`, `LMCombat`, `LMInput`, `LMUI`.
- 경로 예: `Content/LMCharacter/LMPlayer/BP_LM_PlayerCharacter`.
- `Content`, `Source`, `Config`, `Plugins`, `Public`, `Private` 등 엔진과 빌드 구조에서 사용하는 표준 폴더명은 유지한다.
- 엔진 생성 폴더와 외부 플러그인·외부 패키지가 제공하는 폴더를 이 규칙에 맞추기 위해 임의로 바꾸지 않는다.

## 언리얼 구현 자료 조사

- 언리얼 구현 정보를 찾을 때는 [Epic Developer Community의 Unreal Engine 스니펫](https://dev.epicgames.com/community/unreal-engine/snippets)에서 관련 스니펫을 먼저 검색한다.
- 적합한 스니펫이 없거나 설명이 부족하면 [Unreal Engine 5.9 한국어 공식 문서](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-9-documentation?lang=ko)를 확인한다.
- 스니펫과 공식 문서에서 찾지 못한 내용은 다른 외부 자료를 조사하기 전에 [Epic Unreal Engine 어시스턴트](https://dev.epicgames.com/community/assistant/unreal-engine)에 먼저 질문한다.
- 스니펫·공식 문서·어시스턴트 답변을 적용하거나 예시 코드로 제시하기 전에 현재 프로젝트의 **UE 5.8**과 호환되는지 점검한다. 자료의 대상 버전과 사용한 API·헤더·모듈·폐기된 기능을 UE 5.8 공식 문서와 설치된 엔진 소스에 대조하고, 확인하지 못한 부분은 미확인으로 명시한다. UE 5.9 문서를 참고하더라도 프로젝트의 기준 버전은 UE 5.8로 유지한다.

## 기준 문서

- [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)
- [Recommended Asset Naming Conventions](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)
- [Codex AGENTS.md 지침 탐색](https://learn.chatgpt.com/docs/agent-configuration/agents-md)

## 공유 스킬

프로젝트 스킬의 원본은 이 저장소의 `.agents/skills/`에서 관리한다. 같은 이름의 개인 설치본이 보이더라도 프로젝트 작업에는 이 저장소의 스킬을 사용한다. 스킬 목록, 팀원 사용 방법, 필요한 플러그인과 기존 개인 설치본 전환은 [공유 스킬 안내](Docs/LMContext/TEAM_SKILLS.ko.md)를 따른다.

<!-- LMContext:start -->
## Codex 작업 상태 보존

- 개발 프로젝트는 `Lumia.uproject`가 있는 폴더다. `EngineAssociation`은 5.8이며, 설치/배포 기록상 UE **5.8.4**다. `Build/EditorBuild.json`과 설치 엔진의 `Build.version`으로 정확한 패치를 확인한다.
- C++ 빌드 대상은 `LumiaEditor / Win64 / Development`다. 빌드·UHT·BP 컴파일·PIE·게임 검증의 기존 사용자 수행 원칙은 `README_CODE.ko.txt`를 따른다. 실행하지 않은 검증을 통과로 적지 않는다.
- 복잡한 작업은 `.agents/skills/planning-with-files/SKILL.md`와 `Docs/LMContext/README.ko.md`를 읽고 개인 로컬 작업별 `.planning/<ID>/`의 `task_plan.md`, `findings.md`, `progress.md`를 유지한다. 이 폴더는 Git/P4에 추가하지 않는다. 재개·압축 후에는 세 파일과 실제 변경 상태를 먼저 확인한다.
- Windows 훅이 알려준 `Saved/LMContext/Sessions/<hash>.plan`에 현재 요청의 작업 ID 한 줄만 연결한다. 바인딩이 없으면 요청에 맞는 기존 과제를 재사용하거나 새 과제를 만들고, 다른 사람의 기록을 추측해 선택하지 않는다. 한 작업의 기록 소유자는 한 명이며 로컬 대화 바인딩은 공유하지 않는다.
- 결정 이유·변경 파일·실패한 시도·실행한 검증·미완료와 다음 행동을 수시로 기록한다. 기록은 대화 원본이나 비밀 정보를 포함하지 않는다. PreCompact 안내만 믿고 저장을 미루지 않는다.
- 모든 Markdown은 P4 add/reconcile에서 제외한다. AGENTS.md와 검토된 팀 지식은 기존 Dev Git에서 관리한다. 개인 TODO·임시 조사·대화·머신 경로는 `.planning/` 또는 `Saved/LMContext/`에만 두고 Git/P4에서 제외한다. 팀 규칙·공통 결정·인수인계만 `Docs/LMContext/Team/`에 필요한 부분을 새로 요약하여 검토 후 명시적으로 stage한다. 개인 기록 전체를 복사하거나 강제 add하지 않는다. 분류·승격 절차는 `Docs/LMContext/SHARING.ko.md`를 따른다.
<!-- LMContext:end -->
