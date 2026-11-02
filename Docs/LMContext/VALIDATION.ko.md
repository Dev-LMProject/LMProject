# 검증 결과 — 2026-11-02

기준: planning-with-files v3.27.0 / upstream commit b6580cd24af1d8a95ab268794456e05608a1afd3.

## 현재 관리 방식

도구·훅·공유 문서는 **기존 Dev Git 저장소**에 통합한다. 같은 저장소의 codex/context-preservation 작업 브랜치에서 관련 파일만 관리하며 사용자 게임 수정과 staging을 보존한다. 별도 LMContextTools 저장소는 사용 중단된 잔여물이고 삭제하지 않았다. P4 binary 변경은 실행하지 않았으며 현재 계획에서 제외했다.

## 확인한 공통 기능

Windows 모의 검사 20개를 통과했다. 버전 고정/해시·등록·구문, startup/resume/compact/fork 복원, 두 대화의 분리, 미연결/잘못된 작업 ID 거부, PLANNING_DISABLED, 공백/하위 폴더, PreCompact 안내, 나머지 훅, 반복 복구와 원본 기록 보존을 확인했다.

공식 초기화 도구의 과제 생성·기존 과제 보존과 계획 형식도 검사했다. 상위 LMProject 전달 훅은 Dev를 읽고, Dev 내부에서는 출력하지 않아 중복 주입하지 않았다.

P4 변경 목록 111에서 도구·안내 add 58개만 `revert -k`로 해제했다. 해제 전후 로컬 파일 해시가 모두 같았다. 기존 사용자 에셋 편집과 프로젝트 지침 편집은 되돌리지 않았다. P4 111의 남은 대상은 프로젝트 AGENTS/.p4ignore와 과제 기록 세 파일이다.

## 남은 사용자 단계

- 작업 브랜치의 Git 변경 검토·병합/공유. 자동 병합·push·P4 submit은 수행하지 않는다.
- `/hooks` 신뢰 승인과 실제 Codex `/compact` 이후 전체 기록 재읽기 확인.
- 다른 PC에서 실제 Git/P4 받기 후 확인. 로컬 새 체크아웃 모의 검증은 원격 배포 테스트가 아니다.

게임 빌드·UHT·BP/PIE 검증은 수행하지 않았다. 이 작업은 게임 코드와 애셋을 변경하지 않는다. 전역 설정, 신뢰 저장소, 실행 정책은 변경하지 않는다.
## 기존 Dev Git 통합에 대한 독립 검증

- 기존 Dev 저장소에 연결된 codex/context-preservation 브랜치에서 관련 파일 64개만 명시적으로 stage했다. Source/Content나 사용자의 기존 staging은 포함하지 않았다.
- 이 브랜치의 index에서 대상 64개만 새 임시 경로에 checkout하여 검사했다. 이전 별도 저장소 검사와 별도로 수행했다.
- 같은 checkout을 두 번 수행한 뒤 64개 파일 해시가 모두 동일했다.
- core.autocrlf=true 조건에서도 셸 LF·공식 CMD CRLF·49개 고정 원본 해시가 보존됐다.
- 이 체크아웃에서 Windows 훅 모의 검사 20개를 다시 통과했다.
- Git ignore가 요청한 도구 경로는 허용하고 개인 config, 다른 개인 스킬, 기존 P4 PDF, Saved, 실제 과제 기록은 제외함을 확인했다.
- 사용자 작업 브랜치로 자동 병합하지 않았다. 실제 `/hooks` 신뢰 및 `/compact` 종단 테스트는 여전히 미실행이다.