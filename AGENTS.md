# LM 프로젝트 명명 규칙

기본 명명 규칙은 언리얼 엔진의 공식 규칙을 따른다. 프로젝트에서 직접 만드는 타입, 애셋, 폴더에는 프로젝트 식별자 `LM`을 추가한다.
이 문서는 Codex가 새 작업에서 참고할 프로젝트 지침이다. 기존 구현의 이름을 새 규칙의 기준으로 삼지 않는다.

## 적용 범위

- 새로 만드는 프로젝트 코드, 애셋, 사용자 정의 폴더에 적용한다.
- 규칙 문서화만을 이유로 기존 코드, 애셋, 폴더를 일괄 변경하지 않는다.
- 아래 이름은 형식을 설명하는 예시이며, 해당 클래스나 기능을 구현하라는 요구사항이 아니다.

## 코드 작성 규칙

- 실제 코드를 작성하거나 예시 코드를 제시할 때, 파라미터·인자 목록(함수 선언·정의·호출 등)은 길이나 개수와 관계없이 반드시 한 줄로 작성하며 절대 줄바꿈하지 않는다.

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

## 기준 문서

- [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)
- [Recommended Asset Naming Conventions](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)
- [Codex AGENTS.md 지침 탐색](https://learn.chatgpt.com/docs/agent-configuration/agents-md)

<!-- LMContext:start -->
## Codex 작업 상태 보존

- 개발 프로젝트는 `Lumia.uproject`가 있는 폴더다. `EngineAssociation`은 5.8이며, 설치/배포 기록상 UE **5.8.4**다. `Build/EditorBuild.json`과 설치 엔진의 `Build.version`으로 정확한 패치를 확인한다.
- C++ 빌드 대상은 `LumiaEditor / Win64 / Development`다. 빌드·UHT·BP 컴파일·PIE·게임 검증의 기존 사용자 수행 원칙은 `README_CODE.ko.txt`를 따른다. 실행하지 않은 검증을 통과로 적지 않는다.
- 복잡한 작업은 `.agents/skills/planning-with-files/SKILL.md`와 `Docs/LMContext/README.ko.md`를 읽고 개인 로컬 작업별 `.planning/<ID>/`의 `task_plan.md`, `findings.md`, `progress.md`를 유지한다. 이 폴더는 Git/P4에 추가하지 않는다. 재개·압축 후에는 세 파일과 실제 변경 상태를 먼저 확인한다.
- Windows 훅이 알려준 `Saved/LMContext/Sessions/<hash>.plan`에 현재 요청의 작업 ID 한 줄만 연결한다. 바인딩이 없으면 요청에 맞는 기존 과제를 재사용하거나 새 과제를 만들고, 다른 사람의 기록을 추측해 선택하지 않는다. 한 작업의 기록 소유자는 한 명이며 로컬 대화 바인딩은 공유하지 않는다.
- 결정 이유·변경 파일·실패한 시도·실행한 검증·미완료와 다음 행동을 수시로 기록한다. 기록은 대화 원본이나 비밀 정보를 포함하지 않는다. PreCompact 안내만 믿고 저장을 미루지 않는다.
- 모든 Markdown은 P4 add/reconcile에서 제외한다. AGENTS.md와 검토된 팀 지식은 기존 Dev Git에서 관리한다. 개인 TODO·임시 조사·대화·머신 경로는 `.planning/` 또는 `Saved/LMContext/`에만 두고 Git/P4에서 제외한다. 팀 규칙·공통 결정·인수인계만 `Docs/LMContext/Team/`에 필요한 부분을 새로 요약하여 검토 후 명시적으로 stage한다. 개인 기록 전체를 복사하거나 강제 add하지 않는다. 분류·승격 절차는 `Docs/LMContext/SHARING.ko.md`를 따른다.
<!-- LMContext:end -->
