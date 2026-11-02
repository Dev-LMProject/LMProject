# 저장 타입 변경 대상으로 언급했던 50개 파일

현재 정책은 **Git에서 텍스트로 관리**하는 것이다. 이전의 P4 binary 일괄 변경 제안은 철회했으며 실행하지 않았다.

외부 49개는 planning-with-files v3.27.0, 커밋 `b6580cd24af1d8a95ab268794456e05608a1afd3`, MIT 라이선스다. 46개는 원본과 동일하며 아래 표시된 셸 파일 3개는 `.codex/skills` 대신 `.agents/skills`를 참조하도록 경로만 수정했다. 추가 1개는 이 프로젝트를 위해 작성한 명령 평문이다.

확인 결과: `.md` 11, `.ps1` 8, `.py` 10, `.sh` 19, `.cmd` 1, `.txt` 1. **50개 모두 유효한 UTF-8 텍스트이며 NUL 바이트가 없다.** 이 50개에 `.json`, `.exe`, `.dll`, 게임 코드, 에셋, 대화 로그는 없다. 실제 패키지에는 별도로 작성한 JSON 훅 설정·해시 목록과 LMProject 연결기가 더 있으며, 이들도 텍스트다.

| 상대 경로 | 역할 | 원본 대비 |
| --- | --- | --- |
| `.agents/skills/planning-with-files/examples.md` | 사용 예제 | 동일 |
| `.agents/skills/planning-with-files/plan-inventory.md` | 저장된 과제 목록 조회 형식 | 동일 |
| `.agents/skills/planning-with-files/reference.md` | 파일 기반 계획 원칙 | 동일 |
| `.agents/skills/planning-with-files/SKILL.md` | 에이전트 사용 규칙과 절차 | 동일 |
| `.agents/skills/planning-with-files/scripts/attest-plan.ps1` | 계획 내용 해시 기록·검사 | 동일 |
| `.agents/skills/planning-with-files/scripts/attest-plan.sh` | 계획 내용 해시 기록·검사 | 동일 |
| `.agents/skills/planning-with-files/scripts/check-complete.ps1` | 계획 단계 형식·완료 검사 | 동일 |
| `.agents/skills/planning-with-files/scripts/check-complete.sh` | 계획 단계 형식·완료 검사 | 동일 |
| `.agents/skills/planning-with-files/scripts/gate-stop.sh` | 선택 모드의 종료 판단 연결 | 동일 |
| `.agents/skills/planning-with-files/scripts/init-session.ps1` | 과제 폴더와 기록 생성 | 동일 |
| `.agents/skills/planning-with-files/scripts/init-session.sh` | 과제 폴더와 기록 생성 | 동일 |
| `.agents/skills/planning-with-files/scripts/inject-plan.py` | 선택한 계획의 제한된 내용 주입 | 동일 |
| `.agents/skills/planning-with-files/scripts/inject-plan.sh` | 선택한 계획의 제한된 내용 주입 | 동일 |
| `.agents/skills/planning-with-files/scripts/ledger-append.ps1` | 구조화 진행 기록 추가 | 동일 |
| `.agents/skills/planning-with-files/scripts/ledger-append.sh` | 구조화 진행 기록 추가 | 동일 |
| `.agents/skills/planning-with-files/scripts/ledger-summary.ps1` | 진행 기록 요약 | 동일 |
| `.agents/skills/planning-with-files/scripts/ledger-summary.sh` | 진행 기록 요약 | 동일 |
| `.agents/skills/planning-with-files/scripts/phase-status.ps1` | 단계 상태 갱신 | 동일 |
| `.agents/skills/planning-with-files/scripts/phase-status.sh` | 단계 상태 갱신 | 동일 |
| `.agents/skills/planning-with-files/scripts/plan-doctor.sh` | 설치·과제 선택·주입 진단 | 동일 |
| `.agents/skills/planning-with-files/scripts/resolve-plan-dir.ps1` | 선택 과제 디렉터리 결정 | 동일 |
| `.agents/skills/planning-with-files/scripts/resolve-plan-dir.sh` | 선택 과제 디렉터리 결정 | 동일 |
| `.agents/skills/planning-with-files/scripts/session-catchup.py` | 기본은 대화 이력 미조회, 명시적 옵션에서 로컬 이력 조회 | 동일 |
| `.agents/skills/planning-with-files/scripts/set-active-plan.ps1` | 과제 선택·목록 관리 | 동일 |
| `.agents/skills/planning-with-files/scripts/set-active-plan.sh` | 과제 선택·목록 관리 | 동일 |
| `.agents/skills/planning-with-files/scripts/skill-hook.sh` | 일반 스킬 이벤트 연결 | 동일 |
| `.agents/skills/planning-with-files/templates/analytics_findings.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.agents/skills/planning-with-files/templates/analytics_task_plan.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.agents/skills/planning-with-files/templates/findings.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.agents/skills/planning-with-files/templates/loop.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.agents/skills/planning-with-files/templates/progress.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.agents/skills/planning-with-files/templates/task_plan_autonomous.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.agents/skills/planning-with-files/templates/task_plan.md` | 계획·발견·진행·선택 모드의 빈 기록 양식 | 동일 |
| `.codex/hooks/codex_hook_adapter.py` | Codex 입력·작업 선택·Git Bash 연결 | 동일 |
| `.codex/hooks/context_frame.py` | 계획/진행 발췌를 데이터 경계로 감싸기 | 동일 |
| `.codex/hooks/permission_request.py` | 승인 요청에 계획 존재 안내 | 동일 |
| `.codex/hooks/plugin_dispatch.py` | 공식 플러그인 패키지 이벤트 분배 | 동일 |
| `.codex/hooks/post_tool_use.py` | 편집 후 진행 기록 안내 | 동일 |
| `.codex/hooks/post-tool-use.sh` | 편집 후 진행 기록 안내 | 동일 |
| `.codex/hooks/pre_tool_use.py` | 도구 실행 전 계획 참조 | 동일 |
| `.codex/hooks/pre-compact.sh` | 압축 전 기록 갱신 안내 | 동일 |
| `.codex/hooks/pre-tool-use.sh` | 도구 실행 전 계획 참조 | 동일 |
| `.codex/hooks/pwf-hook.cmd` | 공식 Windows Python 실행 연결 | 동일 |
| `.codex/hooks/resolve-plan-dir.sh` | 선택 과제 디렉터리 결정 | 스킬 참조 경로만 수정 |
| `.codex/hooks/run_sh.py` | 셸 훅 실행과 Codex JSON 출력 | 동일 |
| `.codex/hooks/session-start.sh` | 시작·재개·압축 후 계획 주입 | 스킬 참조 경로만 수정 |
| `.codex/hooks/stop.py` | 완료 상태 안내 및 선택적 gate 처리 | 동일 |
| `.codex/hooks/stop.sh` | 완료 상태 안내 및 선택적 gate 처리 | 스킬 참조 경로만 수정 |
| `.codex/hooks/user-prompt-submit.sh` | 새 요청에서 선택 과제와 최근 진행 발췌 | 동일 |
| `.codex/hooks/LMHookEntry.txt` | 인코딩된 훅 명령의 검토용 평문 | LMProject 작성 |

## 명령 평문 전체

훅 설정의 `EncodedCommand`와 동일한 내용을 사람이 읽도록 보관한다. 현재 폴더에서 상위 폴더로 올라가며 프로젝트 연결기를 찾는다. 파일 자체를 실행 대상으로 사용하는 것이 아니라 인코딩된 명령의 검토·일치 확인에 사용한다. 파일·환경변수를 영구 변경하거나 네트워크에 전송하는 코드는 없다.

```powershell
$ProgressPreference = 'SilentlyContinue'
$d = Get-Item -LiteralPath .
while ($d) {
    $s = Join-Path $d.FullName '.codex/hooks/LMInvokePlanning.ps1'
    if (Test-Path -LiteralPath $s -PathType Leaf) { & $s; break }
    $d = $d.Parent
}
```

## 줄바꿈 문제를 구분하면

- `.sh` 19개: LF를 CRLF로 바꾸면 Unix/Git Bash 셸이 CR을 구문이나 인수에 포함해 실행 오류를 낼 수 있다. 실제 실행상 고려가 필요하다.
- 나머지 텍스트: LF/CRLF가 달라도 일반적으로 문서 내용이나 Python/PowerShell 코드 의미는 유지된다. 모든 파일을 binary로 바꿀 실행상 근거는 없다.
- 기존 검증기는 원문 바이트 SHA-256 및 평문 명령의 정확한 문자열 일치를 검사하므로 줄바꿈만 바뀌어도 실패한다. 이는 실행 실패와 별개인 검증 설계 문제다. 이 때문에 전부 binary로 묶으려던 제안은 과도했다.
- `text` 유지 + 줄바꿈 정규화 해시는 텍스트 diff/merge를 보존하지만 원본 바이트 해시와 의미를 분리해야 하며, 그것만으로 셸 실행의 CRLF 문제를 해결하지는 않는다.
- P4 `LineEnd: unix/share`는 Workspace 범위여서 다른 프로젝트 텍스트에도 영향을 준다. `text+F/+C/+D/+x`는 저장/실행 비트 옵션이며 파일별 LF 고정 옵션이 아니다. `utf8/unicode`도 줄바꿈 변환을 수행한다.
- 사용자가 선택한 **기존 Dev Git + `.gitattributes`**는 이 도구 파일에만 LF/CRLF를 지정한다. 기존 P4 설정과 파일 타입을 변경하지 않고 텍스트 비교·병합을 유지한다.

출처: [공식 스킬 배포](https://github.com/OthmanAdi/planning-with-files/tree/v3.27.0), [Git 속성](https://git-scm.com/docs/gitattributes), [P4 타입](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/file.types.synopsis.base.html), [P4 수정자](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/file.types.synopsis.modifiers.html), [P4 줄바꿈](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/P4Guide/configuration.workspace.line-endings.html).