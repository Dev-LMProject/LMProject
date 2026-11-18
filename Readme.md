# LMProject 공유 스킬과 Codex 컨텍스트 보존

설계 안내·할 일 카드·일일보고·한국어 글쓰기 스킬을 팀이 같은 Git 버전으로 사용한다. 컨텍스트 보존 도구는 작업의 계획·결정·검증·다음 행동을 파일로 남겨 새 세션이나 압축 후 복구를 돕는다. 기록하지 않은 대화의 무손실 복원은 보장하지 않는다. 게임 코드 안내는 [README_CODE.ko.txt](README_CODE.ko.txt)를 따른다.

## 구성과 사용

- `.agents/skills/`: 팀이 함께 사용하는 설계 안내·할 일 카드·일일보고·한국어 글쓰기 스킬과 기존 `planning-with-files` 도구. [스킬 목록과 사용 방법](Docs/LMContext/TEAM_SKILLS.ko.md).
- `.codex/hooks.json`, `.codex/hooks/`: lifecycle 훅과 LMProject Windows 연결기.
- `AGENTS.md`, `Docs/LMContext/Team/`: Git으로 공유하는 팀 지침·검토된 인수인계.
- `.planning/<ID>/`, `Saved/LMContext/`: 개인 작업 기록·대화 바인딩. Git/P4에 공유하지 않는다.

1. 이 구성이 포함된 Git 버전을 받고, `Lumia.uproject`가 있는 프로젝트 폴더를 Codex에서 연다. Windows는 Python 3와 Git for Windows가 필요하다.
2. 최초에는 `/hooks`에서 출처와 명령을 검토하고 직접 신뢰 승인한다.
3. `$planning-with-files 이번 작업을 시작하고 현재 대화에 연결해줘`라고 요청한다. 재개할 때는 기존 작업 ID를 지정한다.
4. 수정·검증 직후 기록을 갱신하고, 압축·재개 후 세 기록을 읽어 목표·진행·다음 행동을 대조한다.

상세: [사용·복구 안내](Docs/LMContext/README.ko.md), [팀/개인 분리](Docs/LMContext/SHARING.ko.md), [검증 기록](Docs/LMContext/VALIDATION.ko.md).

## 포함된 오픈소스

| 이름 | 고정 버전 / 커밋 | 원본 | SPDX | 포함 경로와 사용·변경 범위 |
| --- | --- | --- | --- | --- |
| planning-with-files | v3.27.0 / `b6580cd24af1d8a95ab268794456e05608a1afd3` | [공식 저장소의 고정 커밋](https://github.com/OthmanAdi/planning-with-files/tree/b6580cd24af1d8a95ab268794456e05608a1afd3) | `MIT` | `.agents/skills/planning-with-files/`, `.codex/hooks/`의 소스·템플릿 49개. 46개는 고정 버전 배포본과 동일하고 셸 3개는 스킬 경로만 수정했다. CMD의 CRLF는 upstream 줄바꿈 규칙을 따른다. `.codex/hooks.json`과 `Docs/LMContext/ParentHooks/hooks.json`은 공식 등록 설정을 LMProject에 맞게 구성했다. |

저작권은 **Copyright (c) 2026 Ahmad Adi**다. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 저작권·변경 고지와 MIT 전문을 보존했으며, 기존 [스킬 LICENSE](.agents/skills/planning-with-files/LICENSE)와 [배포 라이선스 원문](Docs/LMContext/LICENSE.planning-with-files)도 유지한다. 파일별 원본 URL·원문/포함본 해시는 [third-party-inventory.json](Docs/LMContext/third-party-inventory.json)에 있다.

`LMInvokePlanning.ps1`, 명령 평문, 상위 폴더 전달 평문, 검증 스크립트는 LMProject용 작성물로 구분했다. Python·Git for Windows/Bash·Windows PowerShell·Codex는 실행 전제이며 이 구성에 해당 프로그램의 바이너리나 패키지를 재배포하지 않는다. 별도 Node/npm 또는 pip 의존 패키지는 포함하지 않는다.

이 고지는 위 외부 구성요소에 적용되며 LMProject 게임 코드 전체에 MIT 등 새로운 오픈소스 라이선스를 부여하지 않는다.
