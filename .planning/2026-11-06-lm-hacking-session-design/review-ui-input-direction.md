# UI 입력 중앙화 · 방향 검토 UI-v1

상태: 사용자 요청 ‘이부분은 수정하지 말자’에 따라 미채택으로 종료했다. UI 입력 중앙화, CommonUI 채택, UI 전체 이전은 진행하지 않는다. 아래는 중단 시점의 검토 메모이며 적용 지침이 아니다. 소스·에셋·설정은 변경하지 않았다.

## 사용자 목표와 현황
- Controller의 휠·상하 키·E에 흩어진 상호작용 입력을 묶고 최종적으로 UI용 입력을 한곳에서 관리한다.
- 현재 ULMInputConfig.AbilityInputs는 모두 PC 공통 콜백→ASC로 간다. UI 탐색 입력을 이 배열에 그대로 추가하지 않는다.
- InteractionHUD는 후보를 표시·선택하며 PC가 GameOnly에서 키를 받아 MoveSelection을 호출한다. E의 세계 상호작용은 기존 HUD.TryInteractSelected→Interactor.TryInteract이며 GA 전환을 검토 중이다.
- 사용자 편집에서 Interact 입력 태그가 추가됐다. 조사 도중 PC getter 추가/raw E 제거를 관찰했으나 최신 재조회에서는 PC가 기존 raw E를 포함한 상태로 복원됐고 getter도 없다. 현재 데이터/GA 등록 완료 여부는 이번에 확인하지 않았다.

## 선택지와 추천
A. Enhanced Input의 UI 전용 액션/IMC와 작은 공통 수신자: 현재 한 HUD의 키 정리에는 작다. 전체 UI의 활성 화면, 모달 우선순위, 입력 소비, 포커스 복귀, UIOnly 입력까지 직접 확장하면 범위가 커진다.

B. CommonUI + Enhanced Input: 플러그인·뷰포트·활성 위젯 연결 비용이 있으나, 엔진의 UCommonUIActionRouterBase와 UCommonActivatableWidget을 이용해 UI 입력 전달과 활성 화면 정책을 관리한다. 모든 UI 입력 중앙화라는 사용자 목표에는 B를 추천한다. 현재 해킹 구현에 즉시 전체 이전이 필수라는 뜻은 아니다.

## 책임 제안
- 키와 조작 의미는 UI 액션/IMC 설정으로 정의한다. 공통 조작과 화면 전용 조작을 구분할 수 있으며 모든 키를 하나의 IMC에 강제로 넣거나 동시에 활성화하지 않는다.
- CommonUI의 구성된 활성 화면 계층과 InputConfig가 입력 수신·소비·우선순위·포커스 및 입력 모드를 담당한다. 보이는 모든 화면에 같은 이벤트를 방송하지 않는다.
- InteractionHUD는 이전/다음 의미를 받아 기존 MoveSelection을 처리한다. 한 번의 키/휠 동작이 기존 목록 선택과 Slate의 기본 포커스 이동으로 중복 실행되지 않도록 연결한다.
- E의 세계 상호작용 요청과 모달의 UI 확인은 의미가 다르다. E를 GA로 실행할지 직접 Interactor에 요청할지는 별도 결정이다. UI에서 처리한 E가 게임 경로에 다시 전달되거나 같은 명령이 UI/ASC 양쪽에서 이중 처리되지 않도록 한다.
- 해킹 컴포넌트는 세션·일시정지·퍼즐·성공 확인·취소·자기 자원 정리를 소유한다. 성공 확인 후 해금만 하고 다음 E에서 본래 기능을 실행하는 기존 결정은 유지한다.
- UI 전환 시 기존 ASC 입력 취소는 계속 필요하다. CommonUI는 LM의 PendingInputs/HeldInputs를 자동 정리하지 않는다. 기존 ExpectedAvatar/요청 신원 검증 계약을 보존한다.
- CommonUI를 채택하면 기존 v4-r1/해킹 안내에서 컴포넌트가 직접 소유하던 입력 모드·포커스·커서 복원은 화면 정책/라우터와 분담을 다시 정해야 한다. 양쪽이 SetInputMode를 독립적으로 덮어쓰지 않는다. 세부 수명 계약은 채택 후 재검토 대상이다.

## 범위와 확인
- 현재는 방향 검토만 수행한다. 새 범용 매니저·인터페이스·입력 enum·모든 UI용 GA는 만들지 않는다.
- 최초 연결에서 HUD 선택 1회, 모달 확인/뒤로, 일시정지 중 UI 조작, 배경 게임 입력 차단, 닫은 뒤 포커스 복귀, E 중복 실행 및 닫힌 화면 재호출 방지를 사용자 실행으로 확인해야 한다.
- CommonUI를 추가하기만 하면 기존 UUserWidget의 모든 입력이 자동 이전되는 것은 아니다. 엔진/프로젝트 설정 및 각 화면의 액션·활성화 연결이 필요하다.

## 근거
- 설치 UE 5.8: CommonUIActionRouterBase는 LocalPlayerSubsystem, CommonActivatableWidget은 InputMapping/InputMappingPriority/GetDesiredInputConfig를 제공한다. CommonInputSettings의 Enhanced Input 지원 옵션 확인.
- 현재 프로젝트 Build.cs/uproject/Config/관련 UI 소스에 CommonUI 사용 설정이 확인되지 않았다. 런타임 플러그인 전체 사용 여부를 단정하지 않는다.
- [공식 CommonUI 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-advanced-multiplatform-user-interfaces-with-common-ui-for-unreal-engine)
- [공식 CommonUI 설정](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine)
- [UE 5.8 릴리즈 노트](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes): Enhanced Input/Common Input/UI 통합 개선. 일부 CommonUI 연동 문서는 5.2 기준 설명을 포함하므로 실제 적용 때 설치 버전의 소스/API를 기준으로 한다.

## 검토 기록
- designer_v4: A/B 비교 및 위 책임 제안, B 추천.
- Ultra: 검토 도중 사용자 취소에 따라 중단. 통과 판정 없음.
- Matt 최종 방향 검증: 미실행. 이 제안으로 재개하지 않는다.
- 실행 검증 없음.
