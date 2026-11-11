# 팀 공유 스킬

사용자 정의 스킬의 원본은 이 Git 저장소의 `.agents/skills/`에 있다. 팀원은 이 변경을 포함한 버전을 받아 저장소 폴더에서 Codex를 사용한다. 별도 개인 스킬 설치나 머신별 절대 경로 설정은 필요하지 않다.

## 포함된 스킬

| 호출 이름 | 용도 | 원본 |
| --- | --- | --- |
| `$reviewed-feature-guide` | 설계 교차 검토 후 전체 개발 단위를 제시하고 요청한 단위를 하나씩 안내 | [SKILL.md](../../.agents/skills/reviewed-feature-guide/SKILL.md) |
| `$lm-task-card` | 설계 단위 체크리스트, 문제와 해결법, 구현 요약을 구분선으로 나눠 Notion 할 일 카드에 기록 | [SKILL.md](../../.agents/skills/lm-task-card/SKILL.md) |
| `$lm-daily-report` | 작성자별 보고 번호와 집계 기간을 적용해 Notion 일일보고 작성 | [SKILL.md](../../.agents/skills/lm-daily-report/SKILL.md) |
| `$korean-writing-care` | 사실과 사용자 말투를 보존하며 자연스럽고 명확한 한국어 작성 | [SKILL.md](../../.agents/skills/korean-writing-care/SKILL.md) |

기존 `planning-with-files`는 별도의 외부 도구이며 [컨텍스트 보존 안내](README.ko.md)를 그대로 따른다. `reviewed-feature-guide`의 기존 [MIT LICENSE](../../.agents/skills/reviewed-feature-guide/LICENSE)와 대체 검토 문서를 함께 보존한다. 외부 플러그인이나 그 소스는 이번 스킬 묶음에 포함하지 않는다.

## 팀원이 사용하는 방법

1. 이 변경이 포함된 Git 버전을 받은 뒤 **저장소 루트 또는 그 하위 폴더**를 Codex에서 연다. 이 PC의 상위 LMProject 폴더가 아니라 `Dev`에 해당하는 Git 저장소가 기준이다. Unreal 작업에는 P4에서 받은 프로젝트·애셋을 별도로 사용한다.
2. 예를 들어 `$lm-task-card 현재 작업을 할 일 카드로 작성해줘` 또는 `$lm-daily-report 오늘 일일보고를 작성해줘`로 호출한다. 스킬 목록에서 경로가 이 저장소의 `.agents/skills/`인지 확인한다. 갱신한 스킬이 나타나지 않으면 Codex를 다시 시작한다.
3. 할 일 카드와 일일보고는 기본적으로 프로젝트 Notion에 저장한다. 각 팀원에게 프로젝트 Notion 접근 권한, 로그인된 브라우저, Codex의 브라우저 조작 기능이 필요하다. Notion API·MCP·커넥터는 사용하지 않는다. `초안만`을 지정하면 게시하지 않는다.
4. 일일보고 작성자는 요청과 해당 작성자의 기존 보고에서 결정한다. 이름이 불분명하면 확인하며, 다른 팀원의 이름을 기본값으로 넣지 않는다. 기존 보고 형식과 집계 규칙은 유지한다.
5. 설계 교차 검토에는 mattpocock-skills, Superpowers, Ponytail 플러그인의 해당 스킬과 독립 검토 수단이 필요하다. 없는 환경에서는 [대체·수동 검토 안내](../../.agents/skills/reviewed-feature-guide/references/portable-review.md)에 따라 가능한 방식을 선택한다. Git 저장소를 받았다는 이유로 플러그인 설치나 검토 완료를 가정하지 않는다.

Codex의 저장소 스킬 탐색 위치와 갱신 동작은 [공식 스킬 문서](https://learn.chatgpt.com/docs/build-skills)를 따른다. 이 문서의 경로는 저장소 루트 기준이며 개인 PC 경로를 공유하지 않는다.

## 기존 개인 설치본 전환

같은 이름의 개인 스킬과 저장소 스킬은 자동 병합되지 않아 둘 다 표시될 수 있다. 이 프로젝트에서는 저장소의 파일을 수정·사용한다.

개인 설치본이 있는 팀원은 별도 변경이 있는지 먼저 비교한 뒤, 필요한 내용을 보존하고 개인 설치본을 스킬 탐색 폴더 밖에 백업한다. 다른 프로젝트에서 계속 사용할 개인 스킬이라면 삭제하지 말고 이 프로젝트에서 저장소 경로를 명시해 선택한다. 스킬 폴더에 백업 사본을 두면 그 사본도 다시 발견될 수 있다.

## Git과 Perforce 관리

네 스킬은 `.gitignore`의 허용 목록에 포함되어 있다. 스킬을 수정하면 해당 스킬 폴더와 필요한 안내만 명시적으로 Git에 추가한다. `.planning/`, `Saved/LMContext/`, 개인 설정과 인증정보는 공유하지 않는다.

함께 배포하는 [`.agents/skills/.p4ignore`](../../.agents/skills/.p4ignore)가 스킬 폴더 전체를 P4 add/reconcile에서 제외한다. 프로젝트 루트의 P4 소유 `.p4ignore`를 수정할 필요가 없다. 각 팀원의 P4 클라이언트는 `.p4ignore`를 읽도록 설정되어 있어야 한다. 기본 파일명이나 상대 파일명으로 탐색하는 환경에서는 하위 폴더의 규칙도 적용된다.

```text
p4 ignores -v -i .agents/skills/lm-task-card/agents/openai.yaml
p4 ignores -v -i .agents/skills/reviewed-feature-guide/LICENSE
```

Markdown 제외 규칙만으로는 스킬의 YAML과 확장자 없는 LICENSE가 제외되지 않는다. 이미 P4가 추적하는 파일은 ignore를 추가해도 추적이 해제되지 않으므로 기존 관리 상태를 먼저 확인한다. 규칙 해석과 기본 파일명은 [P4IGNORE 공식 문서](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/P4IGNORE.html)를 따른다.
