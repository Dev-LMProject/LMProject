# Markdown 공유·개인 분리

모든 `.md`·`.MD`·`.Md`·`.mD`는 LMProject의 P4 add/reconcile에서 제외한다. **파일 확장자가 Markdown이라는 이유로 Git에 일괄 추가하지 않는다.**

| 내용 | 저장 위치 | 관리 |
| --- | --- | --- |
| 팀 코딩·빌드·검증·복구 규칙 | `AGENTS.md` | Dev Git |
| 상위 폴더의 공통 PR·시각 규칙 원본 | `Docs/LMContext/ProjectRoot/AGENTS.md` | Dev Git; 상위 실제 파일은 로컬 배포본 |
| 공통 설계 결정·인터페이스·팀 인수인계 | `Docs/LMContext/Team/<주제>.md` | 검토 후 Dev Git |
| 사용자 정의 스킬·고정 버전 스킬·템플릿과 공통 사용 안내 | `.agents/skills/`의 승인된 스킬, `Docs/LMContext/` | Dev Git |
| 진행 중 계획·임시 조사·개인 TODO | `.planning/<ID>/` 또는 `.lmcontext-local/` | 로컬만; Git/P4 제외 |
| 대화 연결·로그·머신 경로·상태 | `Saved/LMContext/` | 로컬만; Git/P4 제외 |

## 팀 공유로 승격하기

1. 로컬 기록은 그대로 두고 팀이 결정하거나 이어서 작업하는 데 필요한 사실만 골라 새 문서로 요약한다.
2. 목표·공통 결정과 이유·관련 코드/커밋·검증 결과와 미검증 항목·다음 행동만 남긴다. 개인 TODO, 원문 대화, 인증정보, 개인 경로와 세션 ID는 옮기지 않는다. 애매하면 로컬에 남긴다.
3. `Docs/LMContext/Team/<주제>.md`에 저장하고 내용과 diff를 검토한다. 파일 경로를 명시해 stage한다. `git add .`나 `.planning`의 강제 add는 쓰지 않는다.
4. 기존 프로젝트 브랜치·검토 절차를 따른다. 다른 팀원은 공유 문서를 읽은 뒤 자기 로컬 계획과 바인딩을 만든다. 공유 문서를 개인 실행 로그로 사용하지 않는다.

예: `git add -- AGENTS.md Docs/LMContext/Team/context-preservation.md` (검토한 파일만 지정).

planning-with-files의 원본 스킬과 템플릿은 수정하지 않았다. 프로젝트 AGENTS와 Windows 연결기가 개인 기록 기본값·공유 요약 경로를 안내한다. 훅은 기록을 자동 복사·stage·업로드하지 않는다. 다른 OS에서도 AGENTS의 같은 분류를 따른다.

## 기존 파일의 분류

- Dev/AGENTS.md: 명명·빌드·검증 지침이므로 Git 공유. 기존 본문을 보존한다.
- 상위 LMProject/AGENTS.md: 공통 PR·시각 지침이므로 위 원본을 Git에서 관리한다. 실제 파일은 저장소 밖이며 개인 변경과 비교 후 배포한다.
- 기존 `.planning/2026-11-02-context-setup/` 세 파일: 세션 이력·시도·로컬 상태가 섞여 있어 로컬에 유지한다. 필요한 공통 내용만 `Team/context-preservation.md`로 요약했다.
- DataAsset의 `Character/README.md`: 별도 Perforce 클라이언트 설정 도구 설명이다. 이 작업에서 팀 공통 정책으로 채택하지 않고 로컬에 보존한다.
- DataAsset의 `Character/Tools/AemeathAudio/vgmstream/{README,USAGE}.md`: 외부 오디오 도구 매뉴얼이다. 로컬 원본과 P4 과거 리비전을 보존하며 이번 Dev Git에 자동 편입하지 않는다.

## P4 범위와 한계

이 PC의 LMProject 상위 폴더와 Dev/Main/DataAsset 각 `.p4ignore`에 네 확장자 규칙을 적용한다. 상위 폴더 규칙은 그 아래에서만 유효하다. 별도 위치의 다른 PC에는 각 프로젝트의 `.p4ignore` 변경을 별도로 배포해야 한다. 기존 P4IGNORE 설정은 바꾸지 않는다.

검증은 `p4 ignores -v -i <경로>`와 실제 임시 Markdown의 `p4 add -n` / `p4 reconcile -n -a`로 한다. ignore는 클라이언트 기본 동작이며 이미 추적된 파일이나 명시적 우회 옵션을 서버에서 금지하는 기능은 아니다. 전역 typemap·protect·trigger는 변경하지 않는다.

기존 tracked 파일은 `p4 delete -k`로 **로컬 파일을 남긴 채 미제출 추적 종료**로 준비한다. 열린 edit는 백업/해시 확인 뒤 `revert -k`로 등록만 해제한다. submit 전 depot head는 그대로이며 과거 리비전은 제거하지 않는다. 향후 submit 후 다른 PC의 sync는 해당 파일을 삭제할 수 있으므로 Git 배포와 로컬 보관을 먼저 확인한다. `revert`, `clean`, 강제 sync를 복구용으로 무심코 실행하지 않는다.

공식 근거: [P4IGNORE](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/P4IGNORE.html), [p4 delete -k](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/p4_delete.html).

## Notion의 기존 10줄 중 한 줄 변경안

6번 교체: “개인 기록은 .planning/<ID>/에 로컬 보관하고, 팀 공통 규칙·결정·인수인계만 Docs/LMContext/Team/에 요약해 Git으로 공유한다(Markdown은 P4 제외).”

브라우저 보안정책으로 저장 재확인이 차단돼 이 문구는 아직 Notion에 반영하지 않았다. 기존 10줄 입력·화면 확인 상태와 재접속 저장 미확인 상태를 유지한다.