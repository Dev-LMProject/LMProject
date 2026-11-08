# 해킹 시스템 컴포넌트 · 설계 검토 계획

## Goal
11월 5일 업무 결정 및 사용자가 확정한 동작을 현재 브랜치의 커밋과 대조해, 사용자가 직접 구현할 해킹 설계를 검토한다.

## Next Step
해킹 세션 연결의 선행 단위로 ULMHackingWidget과 최소 WBP_LM_HackingPuzzle, 컨트롤러의 위젯 클래스 설정을 안내한다. 사용자 적용·빌드 확인 후 실제 TryStartHacking과 종료/자원 복원을 연결한다.

## Current Phase
‘다음단계’의 수동 안내를 step-hacking-widget-guide.md에 작성했다. HEAD d37d27e, Git 작업 트리 clean을 재확인했다. TryStartHacking/CancelHacking/ConfirmHacking/ReleaseSession/IsCurrentSessionValid의 미구현 상태와 해킹 위젯 클래스·에셋 부재를 확인했다. 이번 단위는 화면 기본 틀·닫기 요청 경로 준비이며 실제 세션 시작이나 해금까지 구현하는 단위가 아니다. 사용자 적용·컴파일·PIE 미확인, 제품 변경 없음.

## Git 보관 범위 · 사용자 명시 변경
- 2026-11-08 사용자가 지금까지의 기록을 dev_HackingSystem 브랜치에 포함하도록 요청했다. 현재 실제 브랜치명은 shd/dev_HackingSystem이다.
- 기존 로컬 전용 정책의 예외로 이 작업 폴더의 Markdown 9개만 Git 추적 대상에 추가한다. 다른 .planning 과제, Saved/LMContext, P4 관리 범위는 바꾸지 않는다.
- 이 결정을 .gitignore의 해당 폴더 한정 예외로 반영한다. 이후 이 폴더의 .md 변경은 일반 Git 변경으로 표시된다.
- 사용자가 이어서 이 기록은 해당 브랜치에만 포함해야 한다고 명시했다. .planning 기록과 .gitignore의 이 예외는 shd/dev_HackingSystem 전용이며 다른 브랜치로 반영하지 않는다.
- 코드와 기록을 항상 별도 커밋으로 유지한다. 다른 브랜치로 코드를 통합할 때는 이 브랜치 전체를 merge하지 않고 필요한 코드 커밋만 선택해 cherry-pick한다. ce67f90 및 이후 계획/문서 전용 커밋은 제외하고, 대상 브랜치의 결과와 이력에 이 기록이 들어가지 않았는지 확인한다.
- Git ignore 규칙은 이미 추적된 파일의 병합을 자동 차단하지 않는다. 위 내용은 반드시 지킬 통합 절차이며 브랜치명에 따른 자동 차단 기능을 설치했다는 뜻이 아니다.
- 사용자가 진행 중인 해킹 위젯 관련 소스 변경과 staged 상태가 있다. 이번 Git 보관 작업에서는 해당 소스를 검토·수정하거나 커밋에 포함하지 않는다. 앞 Current Phase의 clean 상태는 직전 안내 시점의 기록이다.

## Phases

### Phase 1: 요구·현황 확인
- [x] 일일보고·업무 카드·위키, 소스·호출부와 UnrealMCP 등록표·CDO를 확인했다.
- **Status:** complete

### Phase 2: 설계
- [x] Superpowers 설계자가 v1에서 출발해 프로젝트 계약과 개발 단위를 정리했다.
- **Status:** complete

### Phase 3: 독립 검토와 수정
- [x] Ultra/Builder check v1 U1~U4, 설계자 v2; v2 재검토 U5, 설계자 v3. 사용자가 단말 Hacking·문 ActiveTarget·성공 후 다음 E 확정.
- [x] 같은 v3에서 Ultra 필수 수정 0, mattpocock 독립 검증 ‘남은 설계 수정 사항 없음’.
- **Status:** complete

### Phase 4: 인계
- [x] [최종 설계](design.md) / [근거·지적별 처리](findings.md) / [진행 기록](progress.md) 보존.
- **Status:** complete

### Phase 5: 컴포넌트 결합 분리 단위 안내
- [x] 현 코드·독립 BP 소유 관계 대조 및 C++/BP 수동 적용 안내 작성.
- 안내 완료 후 사용자 적용 현황은 Phase 6에서 별도로 확인했다. 빌드·PIE 결과는 미검증.
- **Status:** complete

### Phase 6: 적용 현황 확인과 테스트 안내
- [x] 소스/입력 에셋/액터 BP 그래프/현재 맵 인스턴스를 읽기 대조했다.
- [x] 누락된 Target 연결과 잠긴 단말 hook, 현재 return false 단계의 기대 결과를 구분했다.
- [x] 일반/완료/잠김/반복 입력/대상 없음의 수동 테스트 절차를 정리했다.
- **Status:** complete

### Phase 7: 상호작용 추적 로그 추가
- [x] 현재 BP_HackingActor 부모와 두 자식의 분기를 읽고 원본 에셋을 백업했다.
- [x] C++ 입력/GA/상호작용/해킹 시작 로그 및 BP 분기/행동 출력 표식을 추가했다.
- [x] 그래프 보존 검증 및 에셋 저장·디스크 확인을 마쳤다. 사용자에게 빌드·BP 컴파일 후의 예상 추적 흐름을 안내한다.
- **Status:** complete

### Phase 8: 구현 점검 및 추적 로그 제거
- [x] 사용자 PIE 기록과 현재 소스를 대조해 기본 분기를 확인했다.
- [x] 로그 제거 요청에 따라 BP 출력 전용 노드를 삭제하고 실행 연결을 복원했다.
- [x] 부모 그래프의 판정/반환/Busy 연결 보존과 에셋 저장을 확인했다.
- **Status:** complete

### Phase 9: 해킹 화면 선행 단위 안내
- [x] 현재 세션 stub·UI 클래스·모듈·컨트롤러 설정과 설치 엔진 API를 대조했다.
- [x] C++ 위젯 부모·최소 BP 화면·닫기 요청·클래스 설정 수동 안내를 기록했다.
- 사용자 적용 및 UHT/빌드/BP 컴파일은 별도 확인 대상이다.
- **Status:** complete

## Decisions Made
- 기존 보드 연산을 유지한다. 세션·위젯은 대상 컴포넌트가 소유하며, 대상 종류 태그·영구(현재 액터 수명) 잠금·원래 ASC의 임시 제한을 분리한다.
- 지정 외부 문은 확인 시 최대 1회 보상을 받는다. 단말 자체는 해금 후 다음 E에서 재검사해 본래 기능을 실행한다.
- 공통 행동 허용 API와 이동/능력/상호작용 실행점 연동은 별도 카드. 이번에 소스·BP·노션 변경, 빌드 및 실행 검증 없음.

## Errors Encountered
- Notion 커넥터 404 → 웹 브라우저에서 원문 확인. 과거 BP에 남은 Test1/2를 종류로 쓰자는 중간 제안은 사용자 정정·라이브 등록표에 따라 폐기했다.
