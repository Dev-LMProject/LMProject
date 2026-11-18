# 상위 LMProject 지침 원본

이 폴더의 AGENTS.md는 `Dev` 바로 위 `LMProject/AGENTS.md`로 배포하는 팀 공통 지침 원본이다. 기존 명명·PR·시각 규칙을 보존했다. 실제 상위 파일은 Dev Git 경계 밖이므로 이 원본을 Git으로 검토하고, 차이를 확인한 뒤 해당 지침만 병합한다. 상위 파일에 개인 수정이 있으면 통째로 덮어쓰지 않는다.

`.p4ignore`는 같은 상위 폴더용 최소 규칙이다. 현재 PC는 이를 배치했고 Dev/Main/DataAsset의 기존 `.p4ignore`에도 Markdown 제외 블록을 추가했다. 다른 PC는 기존 규칙을 보존하면서 네 확장자 규칙을 합치고 `p4 ignores -v -i <파일>`로 확인한다. P4의 `.p4ignore` 배포는 별도 미제출 변경이며 이 Git 파일이 자동 설치하거나 제출하지 않는다.

Dev가 아닌 이름으로 체크아웃한 팀원은 상위 AGENTS의 Dev 상대 경로를 검토해 조정한다. 일반적으로 Lumia.uproject 폴더에서 작업하면 Git의 루트 AGENTS.md를 직접 사용한다.