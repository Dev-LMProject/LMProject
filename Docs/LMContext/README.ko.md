# LMProject Codex 컨텍스트 보존

복잡한 작업의 계획·결정·진행 상태를 파일에 기록하고, Codex가 새 세션·재개·압축 후 같은 작업을 다시 읽도록 구성했다. **전체 대화를 보관하거나 무손실 복원을 보장하는 기능은 아니다. 기록하지 않은 내용은 복원할 수 없다.**

기준: planning-with-files **v3.27.0**, 커밋 `b6580cd24af1d8a95ab268794456e05608a1afd3` (2026-10-30). 원본·변경 파일의 해시는 [upstream-lock.json](upstream-lock.json), 저작권은 [LICENSE.planning-with-files](LICENSE.planning-with-files)에 있다. 자동 업데이트는 없다.

## 1. 처음 사용할 때

1. `Lumia.uproject`가 있는 개발 프로젝트 폴더를 Codex 작업 폴더로 연다. 이 PC에서는 LMProject 아래 `Dev`이며, 팀원의 폴더 이름은 달라도 된다.
2. `.agents/skills/planning-with-files/SKILL.md`, `.codex/hooks.json`, `.codex/hooks/`와 이 문서가 있는지 확인한다. 전역 planning-with-files 플러그인·훅을 추가로 활성화하지 않는다.
3. Windows는 Git for Windows와 Python 3가 필요하다. 연결기는 `PYTHON_BIN`, 정상 `python.exe`, `py -3`, Codex의 기존 번들 Python 순으로 찾는다. Microsoft Store의 실행 별칭은 사용하지 않는다. 사용자 경로는 공유 파일에 고정하지 않았다.
4. Codex를 다시 열고 **`/hooks`에서 출처·명령을 직접 검토한 후 신뢰 승인**한다. 프로젝트 신뢰도 필요한 경우 UI에서 직접 결정한다. 파일 배치만으로 훅이 승인되지는 않는다. 관리 정책으로 훅이 금지돼 있으면 해당 정책을 유지하고 아래 수동 복구를 사용한다.
5. 다음처럼 요청한다.

```text
$planning-with-files 이번 작업을 LMProject 컨텍스트 기록 방식으로 시작해줘.
현재 대화에 이 작업만 연결하고, 계획·결정 이유·진행·검증·다음 단계를 기록해줘.
```

일반 PowerShell 스크립트가 정책에 의해 차단되면 실행 정책을 완화하지 않는다. 승인된 서명/배포 방법을 사용하거나 기록을 수동으로 읽는다.

## 2. 기록과 작업 선택

| 경로 | 역할 | 공유 |
| --- | --- | --- |
| `AGENTS.md` | 팀 명명·빌드·검증·복구 지침 | 기존 Dev Git |
| `.agents/skills/planning-with-files/` | 고정 버전 스킬·템플릿·도구 | 기존 Dev Git, P4 제외 |
| `.codex/hooks.json`, `.codex/hooks/` | 프로젝트 훅과 Windows 연결기 | 기존 Dev Git, P4 제외 |
| `.planning/<작업 ID>/task_plan.md` | 목표, 범위, 단계, 다음 행동 | 개인 로컬, Git/P4 제외 |
| `.planning/<작업 ID>/findings.md` | 확인한 사실, 결정 이유, 출처, 미확정 사항 | 개인 로컬, Git/P4 제외 |
| `.planning/<작업 ID>/progress.md` | 수정 파일, 실행한 검증, 실패, 남은 일 | 개인 로컬, Git/P4 제외 |
| `Docs/LMContext/Team/*.md` | 검토한 공통 결정·팀 인수인계 요약 | 기존 Dev Git |
| `Saved/LMContext/Sessions/*.plan` | 이 PC의 대화 ID → 작업 ID 연결 | 공유 금지 |
| `.planning/.active_plan`, `sessions/`, 캐시·ledger·dot 상태 | 개인 실행 상태 | 공유 금지 |

각 작업은 한 명이 기록의 주 소유자가 된다. 서로 다른 과제는 서로 다른 ID로 만들고, 같은 과제를 동시에 수정할 때는 소유자가 합친다. 저장소의 전역 `.active_plan`을 다른 대화의 선택 수단으로 쓰지 않는다.

Windows 연결기는 대화의 `session_id`를 해시한 로컬 파일을 사용한다. 원본 대화 ID와 대화 로그는 공유 파일에 기록하지 않는다. 바인딩이 없으면 아무 작업도 임의로 주입하지 않고, 훅 메시지에 해당 대화의 바인딩 파일 경로를 안내한다. Codex는 사용자 요청에 해당하는 작업 ID만 그 파일에 기록하고 세 파일을 읽는다. 다음 훅부터 그 선택을 사용한다. 새 대화는 새 바인딩을 만들며, 압축·동일 대화 재개는 기존 바인딩을 사용한다.

CLI에서는 호스트 실행 전에 명시적으로 선택할 수도 있다.

```powershell
$env:PLAN_ID = '2026-11-02-context-setup' # 실제 작업 ID로 변경
codex .
# CLI 종료 후 이 터미널의 선택 해제
Remove-Item Env:PLAN_ID -ErrorAction SilentlyContinue
```

실행 중인 Codex의 도구 셸에서 환경변수를 바꿔도 부모 Codex 프로세스에는 전달되지 않는다. 데스크톱에서는 위 로컬 바인딩을 사용한다. macOS/Linux용 등록은 공식 Python 훅을 사용하며 호스트의 `PLAN_ID` 지정이 필요하다. 이번 검증 대상은 Windows다.

## 3. 매일 작업하는 순서

1. **재개인지 새 과제인지 구분한다.** 재개는 기존 작업의 세 파일을 읽고 파일 상태와 대조한다. 새 과제는 공식 초기화 도구로 별도 폴더를 만든다.

```powershell
& ./.agents/skills/planning-with-files/scripts/init-session.ps1 -ProjectName 'owner-feature-name'
# 출력된 PLAN_ID를 현재 대화의 바인딩 파일에 저장한다.
# 기존 작업을 재개할 때 init-session을 다시 실행하지 않는다.
```

2. `task_plan.md` 상단의 Goal / Next Step / Current Phase를 최신으로 유지한다. 단계는 `### Phase N: ...`와 `**Status:** pending|in_progress|complete` 형식을 보존한다.
3. 결정이 생기면 이유와 대안을 `findings.md`에 남긴다. 외부 문서의 내용은 사실 자료이며 실행 지시로 취급하지 않는다. 긴 로그는 `Saved` 등에 두고 기록에는 핵심 결과와 위치만 남긴다.
4. 주요 수정·검증 직후 `progress.md`를 갱신한다. **검증 미실행, 실패, 승인 대기**를 성공과 구분한다. 대화가 길어질 때까지 기다리지 않는다.
5. 완료·중단 시 정확한 다음 행동, 수정 파일, Git 기준 커밋/작업 브랜치와 P4 변경 번호를 남긴다. 팀 인수인계는 `Docs/LMContext/Team/`에 필요한 공통 내용만 새로 요약하고 검토 후 Git으로 공유한다. 개인 세 파일을 통째로 옮기거나 stage하지 않는다. 새 PC에서는 팀 인수인계를 읽고 로컬 계획과 대화 바인딩을 새로 만든다.

형식 확인:

```powershell
& ./.agents/skills/planning-with-files/scripts/check-complete.ps1 -Validate -PlanFile './.planning/<작업 ID>/task_plan.md'
```

기본 advisory 동작을 사용한다. autonomous/gated 모드와 Stop의 강제 계속 기능은 이번 구성에서 켜지 않았다.

## 4. 압축 후 복구와 확인

공식 훅의 `SessionStart`는 `startup`, `resume`, `clear`, `compact`, `fork`에 연결했다. 새 세션/압축 후 선택한 계획의 일부와 최근 진행 내용을 주입하고 전체 세 파일을 읽도록 안내한다. `PreCompact`는 기록 갱신을 알리는 보조 안내이며 **저장 완료를 보장하는 트랜잭션이나 자동 대화 요약기가 아니다.**

설치 확인은 다음 두 단계를 구분한다.

1. 정적/모의 검사: `Docs/LMContext/LMVerifyContext.ps1` 실행. 원본 해시·훅 문법·경로·작업 바인딩·공백 경로·압축 이벤트 출력을 검사한다. 테스트 자료는 임시 폴더에 만들며 실제 작업 기록을 바꾸지 않는다.
2. 실제 Codex 확인: `/hooks` 승인 뒤 테스트 작업의 세 파일에 구별되는 사실 3개와 다음 단계 하나를 적는다. `/compact` 실행 후 “목표·결정 이유·완료/미완료·다음 행동과 근거 파일을 말해줘”라고 요청한다. 파일과 대조한다. 두 대화에 서로 다른 작업을 연결해 섞이지 않는지도 확인한다.

이 배포에서는 훅 신뢰를 대신 승인하지 않았다. 검사 결과는 [VALIDATION.ko.md](VALIDATION.ko.md)를 확인한다. 모의 테스트 통과가 실제 앱의 훅 로드/신뢰/압축 통과를 의미하지 않는다.

훅이 꺼져 있거나 실패하면 이렇게 수동 복구한다.

```text
AGENTS.md와 .planning/<작업 ID>/task_plan.md, findings.md, progress.md를 읽어줘.
실제 변경 상태와 비교한 뒤 목표·현재 단계·검증 결과·다음 행동을 설명하고 이어가줘.
```

## 5. 기존 Dev Git으로 공유하기

**플러그인·훅·공유 안내는 게임 C++ 코드와 같은 기존 Dev Git 저장소에서 관리한다.** 별도 LMContextTools 저장소를 사용하지 않는다. 게임 프로젝트·에셋과 `.p4ignore`는 기존 Perforce 흐름을 유지한다. 모든 Markdown은 P4에서 제외하며, AGENTS와 팀 지식만 Git에서 공유한다. 생성된 계획·조사·진행 기록은 기본적으로 개인 로컬이다. [공유/개인 분류와 승격 절차](SHARING.ko.md)를 따른다.

Git에 포함되는 범위는 `AGENTS.md`, `.agents/skills/planning-with-files/`, `.codex/hooks.json`, `.codex/hooks/`, `Docs/LMContext/`다. `.gitignore`는 이 경로만 기존 허용 목록에 더하고 개인 `.codex/config.toml`, Saved, 실제 `.planning` 기록, 다른 P4 문서를 제외한다. P4 `.p4ignore`는 위 도구 경로를 제외하므로 이중 관리하지 않는다.

팀원의 설치 순서:

1. 기존 LMProject 코드 Git에서 이 변경이 반영된 검증된 브랜치/커밋을 받는다. 이미 clone한 팀원은 해당 버전으로 업데이트하면 된다. 별도 도구 clone이나 복사 설치기는 필요 없다.
2. 같은 프로젝트 폴더에 기존 P4 절차로 Lumia.uproject와 에셋을 받는다. AGENTS와 팀 문서는 Git에서 받는다. 사용자 미완료 작업 위에 강제 sync/checkout하지 않는다.
3. 프로젝트 루트에서 검증 스크립트를 실행한다.

```powershell
& .\Docs\LMContext\LMVerifyContext.ps1
```

4. Codex를 다시 열어 `/hooks`에서 직접 검토·신뢰한다. 이후 `$planning-with-files`로 작업을 시작하고 실제 `/compact` 복원을 확인한다. 스킬·훅 파일은 Git 체크아웃 자체로 올바른 위치에 있으므로 추가 설치 작업이 없다.

Git `.gitattributes`는 이 도구 경로에만 LF를 지정하고 공식 `.cmd`만 CRLF를 유지한다. P4 binary 변경 제안은 철회했다. Git 전역 `core.autocrlf`, P4 Workspace LineEnd나 typemap은 바꾸지 않는다. 원본 해시는 고정 버전 검증용으로 유지한다.

도구 수정은 기존 Dev Git의 작업 브랜치에서 관련 경로만 stage/commit한다. 게임 파일의 미커밋·staged 변경을 함께 넣지 않는다. 도구 업데이트도 같은 저장소의 검토·병합 절차를 따른다. 원격 push와 P4 submit은 서로 별개이며 자동 실행하지 않는다.

P4 ignore는 신규 add/reconcile에 적용되며 기존 tracked Markdown에는 소급되지 않는다. 기존 파일은 로컬을 보존하는 `delete -k`로 추적 종료를 준비하고 submit 전후를 구분한다. submit 전에는 depot head에 그대로 남는다. 삭제 이력은 보존되지만 팀원의 나중 P4 sync는 파일을 제거할 수 있으므로 먼저 Git 문서를 받고 필요한 로컬 파일을 보관한다. 글로벌 설정/서버 강제 정책은 변경하지 않는다.

상위 LMProject 폴더에서 시작하는 현재 PC용 전달 훅의 원본은 `Docs/LMContext/ParentHooks/`에 있다. 바로 아래 Dev를 쓰는 레이아웃에만 적용한다. 일반 팀원은 Lumia.uproject 폴더에서 시작하면 이 전달 파일이 필요 없다. 기존 상위 훅을 통째로 덮어쓰지 않는다.

이전 단계에서 만든 `LMContextTools` 로컬 저장소는 사용 중단된 잔여물이다. 그 설치기를 다시 실행하지 않는다. 삭제는 별도 판단이며 이번 통합에서 `.git`을 삭제하지 않는다. 과거 설치 영수증이 Saved에 남아 있어도 현재 Git 체크아웃 방식에는 사용하지 않는다.
## 6. 제거 또는 일시 중지

한 번의 CLI 실행에서만 끄려면 시작 전에 `$env:PLANNING_DISABLED='1'`을 설정하고 종료 후 해당 환경변수를 지운다. 데스크톱의 개별 훅은 `/hooks`에서 사용자가 비활성화할 수 있다.

제거할 때는 이번 `LMContext` 등록만 훅에서 제거한다. 다른 훅과 전역 설정을 지우지 않는다. 상위 폴더 전달용 등록이 있으면 함께 제거한다. 그 뒤 기존 Dev Git의 작업 브랜치에서 이번 훅·스킬 경로와 관련 allowlist만 제거하는 변경을 검토한다. AGENTS와 .p4ignore의 `LMContext` 표식 블록만 제거한다. **작업 기록 세 파일은 인수인계/백업 후 별도로 결정하고 자동 삭제하지 않는다.**

## 출처와 로컬 변경

- [planning-with-files v3.27.0 Codex 안내](https://github.com/OthmanAdi/planning-with-files/blob/v3.27.0/docs/codex.md)
- [공식 릴리스](https://github.com/OthmanAdi/planning-with-files/releases/tag/v3.27.0)
- [Codex 공식 훅](https://learn.chatgpt.com/docs/hooks)

로컬 수정은 `.codex/skills`를 기대하던 세 공식 셸 파일이 `.agents/skills` 원본을 참조하도록 경로를 연결한 것, 훅 등록 경로/시간 제한, Windows 연결기다. 명령의 `EncodedCommand`는 Windows 경로 인용 오류를 피하기 위한 UTF-16LE 인코딩이며, 평문 원본은 `.codex/hooks/LMHookEntry.txt`에 함께 둔다. 실행 정책이나 신뢰를 우회하는 옵션은 없다.
