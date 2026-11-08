# 진행 기록

## 2026-11-06
- 노션 원문 6개와 Git 커밋·코드·프로젝트 지침 확인. Notion 커넥터가 페이지를 찾지 못해 브라우저를 이용했다.
- 기존 로컬 UnrealMCP에 읽기 연결. 에디터 설정·애셋·그래프를 변경하지 않는다.
- 사용자에게 완료 후 본래 기능 실행 시점의 문서 차이를 확인하는 선택지를 보냈다. 이후 확인한 업무 카드 말미에서 '다음 Interact'가 명시된 점을 안내했다.
- 설계 담당자 외 독립 검토 예정. 개발 단위 설명 0, 새 구현 적용/검증 0.

- 사용자가 '잠금만 풀고 다음 E 입력에서 실행'을 선택했다. 위키와 카드의 기존 결정과 일치.
- Superpowers 설계자 v1 → 조정자가 design.md에 기록 → 독립 Ponytail Ultra/Builder check 요청. 엔진 원본 CDO·해킹 이름 조회 결과 추가 전달.

- v1 Ultra + Builder check: U1 예전 위젯·시작 호출 재진입, U2 입력 취소 콜백 중 Avatar 교체, U3 Completed→일반 분기의 Busy 우회/외부 SetIsBusy 덮어쓰기, U4 GameOnly/다른 모달 식별 근거(디버그 getter 금지). 구현자는 해소 가능, 추가로 물을 제품 결정 없음. Superpowers 설계자에게 회송.

- 설계자 v2에서 U1–U4 모두 반영; 고정 SHA256 0CA3ADC024514B266CB1D820D5C36EA61386FDB57448C3F6FEBAE201BE9C7151로 Ultra 재검토 요청. User 보충(기존 테스트 태그 사용)은 해당 버전과 일치.

- v2 Ultra: U1~U4 해소, 위젯 Construct 중 실제 제거의 U5 필수 보완. 사용자가 Test1/2 코드 폐기와 해킹 단말·문 사용을 정정해 라이브 태그 등록표로 교차 확인했다. v2의 시험 태그 선택은 폐기하고 v3에서 대체한다.
- 사용자가 문은 기존 ActiveTarget을 사용한다고 확인했다. Hacking/ActiveTarget과 U5를 반영한 v3 SHA256 7DDBCFBC182B62CA035E49A1B7DA158B7BFE7BCA5564A8F26733D0C14D15FD3A를 동일 Ultra가 재검토한다.
## 설계 인계
- 최종본 v3에서 Ultra 통과 및 mattpocock 독립 판정 ‘남은 설계 수정 사항 없음’. 두 검토자가 확인한 문서 해시는 동일하다.
- Git: shd/dev_HackingSystem, b06177d, 추적 파일 미커밋 변경 없음. 로컬 설계·기록과 세션 바인딩만 작성; 노션·애셋·코드·설정 변경 없음.
- 개발 단위 안내: 순서/완료 기준만 전달. 마지막으로 상세 설명한 단위: 없음. 사용자가 적용한 새 단위: 없음. 실행 검증: 없음. 향후 1단위는 단말 Hacking/문 ActiveTarget 및 플레이어 후보 태그와 자물쇠를 연결해 기존 동작을 관찰하는 것.

## 개발 1단위 안내
- 상세 안내: 1/4 ‘단말·문 종류 태그 연결과 자물쇠 표시’. 잠긴 단말·초기 완료 단말·일반 문을 비교하고, 행의 원래 이름에 NeedHacking일 때만 [잠금]을 붙인다.
- 사용자 적용 미확인. 현 코드/태그 관리자/BP 기본값/기존 그래프 연결만 읽음. 게임 소스·BP 변경, 컴파일·PIE 미실행. 다음: 적용 결과 점검 또는 요청 시 2단위(E로 세션 진입·취소) 안내.

## 2026-11-07 · 1단계 표시 방식 추가 안내
- 행의 기존 NeedHacking 분기와 SetSelected 그래프, ActorName 기본색을 읽기 전용으로 확인했다. 잠금 표시를 자물쇠+빨간 글씨로, 일반/완료를 원래 색으로 복원하는 BP 연결과 이미지 대안을 안내했다. 1단계가 일부 반영된 것은 확인했으나 표시 수정 적용과 실시간 해금·PIE 결과는 확인하지 않았다. 2단계 미안내.
- 이모지 대신 쓸 이미지 선택: Kenney Game Icons (CC0)의 PNG/White/2x/locked.png 원본을 Saved/LMContext/LMHackingUI/T_LM_HackingLock.png로 준비했다. 100×100, 투명 배경·흰색 실루엣을 픽셀 검사로 확인. 출처 https://kenney.nl/assets/game-icons 및 제작자 게시 https://opengameart.org/content/game-icons . Content 임포트, BP 적용, PIE는 아직 수행하지 않았다.
- 1단위 이미지 적용 상세 안내. UnrealMCP로 현재 HorizontalBox_0의 첫 자식 Lock(Image, 변수), 두 번째 ActorName을 확인했다. Lock은 Visible, 브러시 리소스 None, 기본 크기 32×32. OnInitialized에서 OriginNameColor 저장, Construct에서 RefreshLockVisual 호출. 이 함수의 기본 이름·원래 색, NeedHacking의 빨간 색은 이미 연결돼 있다. PNG 지정, 함수 시작에서 Lock을 Collapsed로 초기화하고 잠금 핀에서 표시하도록 안내했다. BP 변경·컴파일·PIE는 실행하지 않았다.

## 2026-11-07 · 2단위 안내
- 사용자의 다음 단계 요청에 따라 v3의 2단위 ‘E → 세션 → 취소’ 안내. 현재 HEAD b06177d 및 Git 추적 변경 없음. TryStartHacking/IsCurrentSessionValid는 false, CancelHacking/ReleaseSession은 비어 있는 상태를 재확인했다.
- 1단위 정적 확인: 에디터의 Lock 브러시에 /Game/UI/T_LM_HackingLock 지정, 기본 Collapsed, RefreshLockVisual 진입의 Collapsed 및 NeedHacking의 HitTestInvisible 연결 확인. 현재 브러시 크기는 100×100이며 화면상의 실제 배치는 실행하지 않아 판단하지 않는다.
- 2단위 설명 범위: 기존 E 호출 경로, 플레이어 Active 태그와 대상 예약, 현재 위젯 신원 검사, 최소 ULMHackingWidget/닫기 UI, 진입·스캔/키 정리·pause/포커스·소유 자원 정리, 외부 제거/조종 교체, PC/ASC의 ExpectedAvatar 입력 정리 연결, 수동 검증 순서. 위젯 클래스/함수 추가는 제안이며 소스와 애셋은 변경하지 않았다. 사용자 적용·빌드·UHT·BP 컴파일·PIE 미확인. 다음 단위는 3 ‘퍼즐 조작 → 로컬 해금’.

## 2026-11-07 · E GA와 두 컴포넌트 결합 재검토
- 사용자 요청: Ponytail Ultra로 E의 GAS 입력 편입 평가. 추가 요구는 해킹과 상호작용의 직접 결합 제거, IsInteractionReserved의 역할 및 BP 조합 가능성 설명. 적용 요청이 아닌 검토 요청으로 수행.
- 실제 ASC 입력 큐/취소, PC E/입력 설정, AbilitySet, Interactor/HUD/Interaction/LMInteractable 및 라이브 InputConfig/StartupAbilitySet/IMC/GA 입력 기본값을 읽었다. 현 GA_LM_AttackInput은 LocalOnly/InstancedPerActor, Dead/Stunned/Input.Blocked 차단. E의 IA/매핑/입력태그는 아직 없다.
- v4-r1 제안은 review-e-ga-decoupling-v4.md. E는 동기 요청 1회의 입력 GA로, 해킹 세션은 컴포넌트 소유. 기존 ILMInteractable에 기본 false인 순수 Busy 조회와 PassThrough/Accepted/Rejected handler를 추가하고 소유 액터 BP가 해킹에 연결. 기존 일반 Busy와 Interact 계약을 보존하면서 두 컴포넌트의 상호 타입 참조를 제거한다. 앞선 GetIsBusy의 직접 Hacking 조회 예시는 이 요구에 맞춰 교체 대상이다.
- Superpowers designer_v4 → Ultra ultra_v4(Builder check 포함, 필수 지적/제품 질문 0) → mattpocock validator_v4(남은 설계 수정 사항 없음) 독립 읽기 검토. 동일 본문 SHA256 82881AB6ED6C141691D7BC52E945EB8210DCDE85C556DDFF354C41A2586E3C41. 제안 상태이며 원본 v3 전체 문서는 아직 교체하지 않았다.
- 검토 중 사용자가 HackingComponent.h에 IsInteractionReserved 선언을 추가했다. 해당 함수 구현·호출을 대신 수정하지 않았다. 소스/BP 변경 및 빌드·UHT·PIE 미실행. BP hook 전체 누락과 GA 자기 취소/재활성화는 구현 검증 대상으로 남음.

## 2026-11-07 · 변경안 적용 안내 및 Smart Object 추가 검토
- 사용자 요청 두 가지를 유지했다: 검토안 적용법 설명, 인터랙션 액터의 Smart Object 활용성 평가. E 입력을 GA로 옮기는 첫 독립 단위를 step-e-ga-guide.md에 작성했다. 태그/선택 getter/기존 두 요청 함수 제거/네이티브 GA 전체 예시/세 입력 에셋 설정/사용자 수동 시험을 포함한다. 직접 결합 분리와 세션 구현은 다음 단위다.
- 기존 ultra_v4가 예시의 엔진 API 및 동기 취소·재활성화 경로를 추가 정적 검토해 필수 수정 사항을 찾지 못했다. 현재 라이브 BP_LM_PlayerCharacter에 InputKey E 노드가 없다는 기존 검토 근거를 확인했다. 다른 BP에 옛 경로가 남은 경우를 안내에 조건부로 추가했다.
- designer_v4와 조정자가 Smart Object를 조사했다. 현재 단말/문에는 추가 이점이 낮고, 위치·다중 슬롯·AI/플레이어 경쟁이 생길 때 선택 도입을 권고한다. UE 5.8 Experimental InteractionInterface는 실제로 기본 상호작용 GA를 제공하지만 현재 LM 계약을 통째로 연결/교체해야 하므로 이번에 채택하지 않는다.
- 마지막 안내 단위: v4-r1 E→GA 전환. 사용자 적용 미확인. Git 추적 변경은 기존 HackingComponent.h 선언뿐이며 작성한 것은 개인 문서다. 빌드·UHT·BP 컴파일·PIE 및 Smart Object 실행 비교는 하지 않았다.

## 2026-11-07 · 입력 통합의 이득과 조회 위치 후속 질문
- E→GA는 현재 해킹 완성의 필수 조건이 아니며, 핵심 이득은 기존 ASC 입력 차단/취소 정책 통일이라고 설명했다. 구현량 대비 보류를 권고했지만 이는 사용자 확정 결정이 아니다.
- GetSelectedInteractionActor는 공통 GAS 입력 함수가 아니라 활성화된 GA가 HUD의 선택 액터 참조를 얻는 조회 연결임을 설명했다. 기존 선언만으로는 BP 노드에 노출되지 않음을 구분했다.
- 사용자 Matt Pocock Skills 요청에 따라 함수 배치의 자연스러움·발견 가능성을 집중 검토했다. codebase-design 및 DEEPENING을 읽고 기존 validator_v4가 별도 소스 검토: 현 구조 PC getter 유지가 타당, 선택 상태 이전은 별도 요구가 생길 때 상태/규칙과 함께 검토. 원본 v4-r1의 동작 계약 변경 없음.
- 현재 사용자 변경은 HackingComponent.h 외 LMGameplayTags.h/.cpp의 InputTag_Interact 추가와 LMPlayerController.h의 getter 선언까지다. getter 정의·호출은 미추가. 조정자 소스/에셋 변경 및 빌드·실행 검증 없음.

## 2026-11-07 · UI 입력 중앙화 검토 취소
- 사용자 목표가 휠/상하/E의 InteractionHUD 묶음에서 모든 UI 입력 중앙화로 확장돼 현재 코드와 UE 5.8 CommonUI/EnhancedInput을 읽기 조사했다. designer_v4의 방향 제안을 review-ui-input-direction.md(UI-v1)에 남기고 Ultra 검토를 시작했다. 제품 코드는 작성하지 않았다.
- 조사 중 사용자 PC 편집이 변했고 마지막 확인에서는 기존 raw E 바인딩이 유지되고 선택 getter가 없는 상태였다. 태그 h/cpp와 HackingComponent.h의 사용자 변경만 남아 있었다. 조정자가 되돌린 것은 아니다.
- 사용자가 ‘아니다 그냥 이부분은 수정하지 말자’고 지시해 해당 변경안을 미채택으로 종료하고 실행 중인 Ultra 검토를 중단했다. Matt 최종 방향 검증은 미실행. UI 입력 구조 변경·플러그인 도입·에셋 변경·빌드·PIE 없음. 원래 해킹 전체 요청을 취소한 것으로 해석하지 않는다.

## 2026-11-07 · 현재 개발 단위 최종 재정리
- 사용자가 현재 단계의 구현 내용과 방법을 다시 정리해 달라고 요청했다. UI 중앙화 취소를 유지하고 E만 GAS로 연결하는 기존 v4-r1 단위로 범위를 명시했다. 휠/상하 키와 HUD 선택 상태 소유는 유지한다.
- 최신 확인: 브랜치 shd/dev_HackingSystem, HEAD b06177d, git status clean. InputTag_Interact/PC getter/IsInteractionReserved/새 입력 GA가 없고 PC의 raw E 바인딩은 남아 있다. 이전 사용자 변경을 조정자가 되돌리지 않았다.
- step-e-ga-guide.md의 첫머리 현황과 PC getter 계약 주석, 이전 BP 확인의 시점을 갱신했다. 동작 코드 예시와 기존 정적 검토 계약은 변경하지 않았다. 태그부터 처음 적용하는 순서, 기존 세 입력 에셋 연결, 일반 문 및 동기 취소 수동 확인을 재안내한다.
- 제품 소스/에셋 적용·빌드·UHT·BP 컴파일·PIE는 미실행. 다음 단위는 Hacking/Interaction의 일반 인터페이스/BP 조합 분리다.

## 2026-11-07 · E 입력 GA 구현 진행
- 사용자가 현재 단위 직접 구현 및 C++ 함수 용도 주석/BP 그래프 Comment를 요청했다. E 태그, PC 선택 getter, 일회성 네이티브 GA를 적용하고 raw E와 HUD의 중복 요청 경로를 제거했다. 휠/상하 키는 유지했다. 기존 사용자 HackingComponent.h 변경은 보존했다.
- 독립 구현 검토자 interact_input_implementation_review가 실제 소스와 UE 5.8.4 수명 주기 경로를 검토했으며 필수 수정 사항 0. 정적 검토이며 빌드/게임 검증은 아니다. 변경 범위 diff --check 통과.
- IA_LM_Interact(Boolean)를 생성·저장하고 설명을 읽어 확인했다. GA BP/IMC/InputConfig/StartupAbilitySet 연결은 부모 클래스 등록 뒤 진행한다.
- 사용자에게 기존 빌드 사용자 수행 원칙에 따라 빌드를 요청했으나 사용자가 “이번만 너가 해”로 이번 에디터 종료·빌드·재실행을 직접 승인했다. 추가 승인 없이 진행한다. 아직 빌드/PIE는 실행하지 않았다.

## 2026-11-07 · E 입력 GA 적용 및 검증 완료
- 사용자 이번 예외 승인에 따라 에디터를 정상 종료했다. 종료 시 기존 GA_LM_AttackInput 저장 확인 창의 변경을 저장해 보존했다. 강제 종료하지 않았다.
- 최초 샌드박스 빌드는 ProgramData의 UBA 캐시 쓰기 제한으로 중단했다. 승인된 정상 권한 실행으로 LumiaEditor/Win64/Development 빌드 성공(26.16초, exit 0). 새 LMInteractInputAbility.cpp 포함 컴파일·링크 및 메타데이터 완료. 에디터를 다시 열어 /Script/Lumia.LMInteractInputAbility와 LM.InputTag.Interact 등록 확인.
- 새 GA_LM_InteractInput을 올바른 native parent로 생성했다. 기본 빈 Activate/End 이벤트를 제거해 BP 이벤트 오버라이드가 없으며, 실제 실행은 C++ 부모가 담당한다. EventGraph에 7줄 한국어 Comment를 작성하고 글자 14/박스 크기를 조정해 화면에서 전체가 읽히는 것을 확인했다.
- GA BP를 warnings_as_errors=True로 컴파일하고 저장했다. IA_LM_Interact(Boolean, triggers/modifiers 없음, paused 입력 false)와 기존 IMC_LM_Player/DA_LM_PlayerInputConfig/DA_LM_PlayerStartupAbilities를 연결했다. 각각 12→13, 3→4, 6→7 항목이며 이전 모든 값·하위 오브젝트 참조가 정확히 유지된다.
- Perforce 인증서 지문 불일치로 자동 checkout과 첫 save가 실패했다. 인증서 신뢰 설정은 변경하지 않았다. 원본 3개 입력 에셋을 Saved/LMContext/LMInteractInput/AssetBackup에 백업하고 그 파일들의 로컬 read-only 속성만 해제해 offline 저장했다. 재시도 save=True 및 dirty=False 확인. P4 checkout/add/reconcile/submit은 미완료이며 연결 신뢰 문제 해결 후 필요하다.
- verify_assets.py의 read-only assert 검증 PASS: IA/GA 기본값, E 매핑 1건, InputConfig/AbilitySet 추가 1건, 기존 배열 보존, 실제 PC/PlayerState의 참조, BP 이벤트 override 없음, 5개 에셋 dirty=False, 디스크 내 한국어 Comment 저장 일치(Windows CRLF 정규화).
- 소스/Content에서 옛 TryInteractSelected·InteractWithSelectedTarget 참조 검색 0건. 변경 소스 범위 git diff --check 통과. 사용자가 편집한 HackingComponent.h는 그대로 보존했다.
- PIE/게임 동작·해킹 취소/조종 교체 시나리오는 실행하지 않았다. 범위는 E→GAS 입력 단위까지이며 Hacking/Interaction 분리와 실제 해킹 세션 완성은 다음 단위다. 커밋·푸시·P4 제출 없음.

- 최종 독립 통합 검토: interact_input_implementation_review가 라이브 입력 에셋과 PlayerCharacter 4개/PlayerController 2개 그래프를 다시 대조했다. 기존 입력 항목 보존, 단일 Interact 연결, 중복 raw 키/별도 Interact 경로 없음 확인. 남은 필수 수정 사항 없음. 리뷰어는 빌드·컴파일·PIE·파일 변경을 실행하지 않았다.

- 사용자의 Oracle SSH 확인 요청 완료. 서버 원본 지문과 PC 저장 지문은 일치하며 외부 연결에서 받은 다른 지문은 Norton Web/Mail Shield가 발급한 대체 인증서임을 확인했다. 원인은 Norton SSL 검사와 Perforce 지문 검사의 충돌. 읽기 진단만 수행, 서버 재시작/인증서·trust·Norton 설정 변경 없음.

## 2026-11-08 · 컴포넌트 결합 분리 단위 안내
- 사용자의 '다음단계'를 기존 reviewed-feature-guide의 다음 수동 안내 요청으로 처리했다. 제품 구현 승인으로 확대하지 않았다. 현재 HEAD b06177d, 이전 E 입력 GA 변경 및 사용자 HackingComponent.h 선언은 유지된 상태다.
- 실제 RequestInteraction의 Hacking include/검색/태그 분기, Hacking.h의 미사용 InteractionComponent/bOwnsBusy, IsInteractionReserved 선언만 존재, TryStartHacking=false 및 세션 함수 미구현을 재확인했다.
- 에디터 읽기 조회로 BP_NeedHacking/BP_HackingCompleted가 각각 Actor 직접 자식임을 확인했다. 상속 공유가 없으므로 두 BP에 모두 hook을 구현하도록 안내한다. CDO의 SCS 컴포넌트 변수 None은 컴포넌트 부재로 해석하지 않았다.
- step-decouple-guide.md에 기존 v4-r1 계약을 적용하는 완결된 교체 예시, UHT 반영 순서, IsInteractionBusy/HandleInteractionRequest 함수 그래프·Comment 문구, 이번 단위와 실제 세션 완성 후의 검증 범위를 구분해 기록했다.
- UE 5.8 생성 인터페이스 헤더와 설치 원본의 IsBeingDestroyed/TGuardValue 위치 확인. 처음 추측한 Misc/GuardValue.h는 존재하지 않아 Templates/UnrealTemplate.h로 교정했다. Epic 공식 인터페이스 문서의 Execute_ 호출 규칙 확인.
- 기존 독립 구현 검토자 재호출은 live agent path not found로 실패했다. 이번 예시의 새 독립 검토를 수행했다고 표시하지 않는다. 기존 v4-r1 설계 검토와 현 소스 대조 근거만 사용한다.
- 이번 수정은 개인 안내·진행 기록뿐이다. 소스/BP/입력 에셋 변경, 빌드/UHT/BP 컴파일/PIE 미실행. 사용자 적용 미확인. 다음 단위는 해킹 세션 시작·UI·취소/자원 복원이다.

## 2026-11-08 · 인터페이스 BlueprintPure UHT 오류 정정
- 사용자가 IsInteractionBusy 선언의 UHT 오류를 보고했다. 원인은 앞서 제공한 예시에 인터페이스 함수의 명시적 BlueprintPure가 포함된 것이다.
- 설치 UE 5.8 UhtFunction.cs:700–712는 반환값을 가진 const BlueprintCallable의 AutoBlueprintPure를 설정하고, 772–776은 인터페이스의 명시적 BlueprintPure만 오류 처리함을 확인했다. Epic UFunctions 문서도 const 함수의 기본 pure 노출 규칙을 확인했다.
- LMInteractable.h의 해당 매크로 한 줄만 BlueprintNativeEvent, BlueprintCallable로 수정했다. const와 _Implementation 선언/정의는 유지한다. step-decouple-guide.md의 예시와 설명도 함께 정정했다.
- 선언/정의 const 일치 및 명시적 Pure 제거 정적 assert 통과. 파일 전체 git diff --check는 사용자가 추가한 기존 빈 줄의 탭(33/37행)을 보고했으며 이 별도 공백은 변경하지 않았다. UHT/빌드/BP 컴파일은 실행하지 않았다.

## 2026-11-08 · BP 태그 비교 연결 안내 누락 정정
- 사용자가 GetHackingStateTag 뒤에 실제 비교 연결이 없다고 지적했다. 기존 흐름도에서 데이터 노드와 실행 흐름을 섞고 Equal/Condition 핀을 생략한 설명 오류를 인정했다.
- 설치 BlueprintGameplayTagLibrary.h에서 Equal (GameplayTag)/CompactNodeTitle ==, MakeLiteralGameplayTag를 확인했다. 실제 비교 두 개(NeedHacking/HackedComplete), Literal 태그, 각 Branch Condition과 흰 실행 핀, TryStartHacking의 In Interactor=RequestActor를 구분해 step-decouple-guide.md를 수정했다.
- 공식 웹 검색의 Equal Exactly 노드명은 결과가 없어 근거로 사용하지 않았다. 실제 설치 엔진의 DisplayName을 기준으로 Equal (GameplayTag)를 안내했다.
- 안내 문서만 수정. 사용자 BP 그래프의 현재 연결은 조회하지 않았으며, 소스/BP 변경·빌드·실행 검증은 하지 않았다.


## 2026-11-08 · 현재 구현현황 및 테스트법 요청
- C++ 및 UnrealMCP 라이브 그래프/컴포넌트/입력 매핑을 읽기 확인했다. 현재 PIE는 미실행, BP_HackingCompleted의 미저장 변경은 보존했다.
- 소스 커밋 d37d27e에 분리가 반영되어 있고 두 BP 보완 사항을 발견했다. 원본 소스/BP를 수정하거나 컴파일/저장/PIE/커밋하지 않았다.
- 수동 테스트는 BP Target 연결 및 잠긴 단말 hook 적용 후 일반 Interact, 완료 PassThrough, 잠김 Rejected, 매 누름 1회/재입력, 무대상 입력 순서로 안내한다. 시작 stub 때문에 해킹 UI가 열리지 않는 것이 현재 기대 결과다.
- 도구 시행착오: 이전 완료된 exec session 재조회는 Unknown process id였고 기존 저장된 결과로 재개했다. 일부 파일 경로를 잘못 추정한 rg는 실패하여 Source/Lumia 전체 검색으로 실제 LMAbilitySystem 경로를 확인했다.
- Blueprint Debug Object/Breakpoint/Watch 절차는 Epic UE 5.8 공식 문서로 확인했다. 실행 검증 통과로 보고하지 않는다.

- 동일 hook 재사용 질의에 현재 인터페이스·컴포넌트 코드를 읽고 소유 액터 공통 부모 재사용과 언어/의존성의 차이를 설명했다. 소스·BP 수정 및 빌드/PIE 없음. 기존 사용자 결정은 변경하지 않았다.

- 자식 BP의 Interact 구현/공통 hook 호출 위치 질의: 현재 소스에서 GA ActivateAbility -> Interactor.TryInteract -> Interaction.RequestInteraction -> EvaluateInteraction/Execute_IsInteractionBusy -> Execute_HandleInteractionRequest -> PassThrough 재검사 -> Execute_Interact 흐름을 재확인했다. 공통 부모는 제안이며 아직 적용으로 기록하지 않는다. 자식 Interact는 본래 행동을 시작하고 bool로 시작 성공을 반환하며, 즉시 동작이면 SetIsBusy(false), 지속 동작이면 종료/취소 시 Busy 해제를 안내한다. 코드/BP 수정·실행 없음.

- 출력 문자열 질의: 첨부된 BP_HackedCompleteTest.InteractAction의 Interactor -> GetObjectName -> PrintString을 확인했다. BP_HackingActor의 함수 override임은 첨부로 확인되지만 부모 전체 구현은 이번에 조회하지 않았다. 최소 안내는 Append 4입력(라벨/이름/실제 줄바꿈+라벨/해킹 태그 문자열), 자신의 Hacking 컴포넌트 getter와 GetDebugStringFromGameplayTag 사용이다. 설치 UE 5.8에서 해당 API 및 문자열 핀 Shift+Enter 줄바꿈 구현을 확인했다. 소스·BP 변경/컴파일/PIE 없음.


## 2026-11-08 · 추적 로그 구현 진행
- 사용자 승인 범위에서 소스 5개 .cpp 및 BP 3개를 변경했다. 원본 에셋은 Saved/LMContext/LMInteractionTrace/AssetBackup에 복사했다.
- git diff --check 통과, 라이브 BP 그래프 비교 통과. 자식 pin setter는 dirty 표시를 만들지 않아 저장 도구의 강제 저장 동작을 확인 중이다. 새 Comment/프레임워크/설정이나 UI 입력 구조 변경은 추가하지 않았다.

- 완료: C++ 5개 파일 로그 추가(42줄 추가/5줄 교체), BP_HackingActor 및 두 테스트 자식 저장 완료. 출력 태그는 [LMInteraction]. 원본 에셋 백업·전후 그래프·검증 결과는 Saved/LMContext/LMInteractionTrace에 보관했다.
- 검증: git diff --check 통과; 기존 BP 분기/반환/Busy 조회/Interact 연결 및 자식 동작 보존 assert PASS; dirty=False 및 디스크 trace prefix 저장 확인. 별도 C++ 빌드/UHT/BP 컴파일/PIE는 실행하지 않았다. 커밋/푸시/P4 제출 없음.
- 도구 메모: 초기 광범위 파일 검색에 Temp 접근 거부가 있었으며 이후 설치 엔진의 정확한 EditorToolset 경로로 좁혀 읽었다. metadata 캐시가 오래된 부분은 live describe_toolset으로 갱신했다.


## 2026-11-08 · 구현 점검과 로그 제거 완료
- 현재 소스와 사용자 실행 기록을 확인했다. 기본 성공/거절/무대상 분기가 기대대로 실행된 기록을 확보했다.
- 추가 지시에 따라 BP 3개의 출력 전용 노드를 제거·저장했다. 부모 실행선 및 기존 기능 그래프 보존 assert PASS, 자식은 빈 InteractAction 유지. C++은 이미 추적 로그가 없는 clean 상태로 변경하지 않았다.
- 진행 중 PIE는 편집을 위해 중지했으며 새 PIE/빌드/컴파일은 수행하지 않았다. 실행 환경 반영은 사용자 재빌드와 BP 컴파일이 필요하다. Git 커밋/푸시 및 P4 제출 없음.

## 2026-11-08 · 다음 단계 수동 안내: 해킹 화면 준비
- reviewed-feature-guide의 ‘다음단계’ 신호에 맞춰 제품은 편집하지 않고 step-hacking-widget-guide.md를 작성했다. 기존 세션 단위의 선행 UI 준비만 분리했으며 요구/소유 관계/해금 시점 계약은 유지한다. 새 독립 설계 검토를 했다고 주장하지 않는다.
- 새 ULMHackingWidget의 약참조 연결/해제·RequestClose·ESC preview 예시, Cancel/Confirm의 발신 위젯 인자, PC의 클래스 설정, SlateCore 모듈, WBP 생성과 Comment 및 빌드/에셋 구성 확인 절차를 포함했다.
- E로 실제 화면을 띄우거나 닫는 단위가 아님을 명시했다. 현재 세션 동일성 검사 및 외부 제거/시작 취소/자원 복원, 보드 입력/성공 확정은 각각 후속 단위에 남아 있다. SourceWidget 인자만으로 해당 안전 검증이 완성됐다고 기록하지 않는다.
- 읽기 검증: 현재 HEAD/clean 소스, MCP PIE=false 및 해킹 위젯 에셋 부재, 설치 UE 5.8 생성자·입력 API와 Epic 5.8 OnPreviewKeyDown 설명을 확인했다. 제품 수정/에디터 조작/컴파일/PIE 테스트는 수행하지 않았다.
- 다음: 사용자 적용 확인 후 실제 세션 시작·취소/자원 복원 단위를 안내한다. 개인 계획·안내 파일만 수정했으며 Git/P4에 추가하지 않는다.

## 2026-11-08 · 사용자 요청에 따른 브랜치 기록 보관
- 사용자가 기존 로컬 기록을 dev_HackingSystem 브랜치에 포함하도록 명시했다. 실제 브랜치 shd/dev_HackingSystem에서 이 폴더의 Markdown 9개와 .gitignore 예외만 별도 커밋 대상으로 준비한다. 이전 로컬 전용 기록 정책은 이 폴더에 한해 변경된 것으로 본다.
- .gitignore는 다른 .planning 과제를 계속 제외하고 이 폴더의 .md만 허용한다. 기본 제외 정책이나 Perforce 설정을 전역으로 바꾸지 않는다.
- 현재 사용자의 소스 변경 6개(그중 새 위젯 .h/.cpp는 staged 후 추가 수정)가 있으며, 원래 index 내용을 별도로 확인했다. 이번 문서 커밋에 소스를 포함하지 않는다.
- 문서의 비밀키/인증 토큰 형태 검색은 일치 0건이었다. Git 추적/무시 규칙과 커밋 범위를 확인하며, 이번 요청에는 C++ 빌드·BP 컴파일·PIE 검증이 필요하지 않다.

## 2026-11-08 · 브랜치 전용 보관 조건 명확화
- 앞선 기록 추가는 ce67f90으로 완료됐다. .gitignore와 계획 문서 9개만 들어 있고, 현재 이 커밋을 포함한 브랜치는 shd/dev_HackingSystem뿐이다. main/dev의 .planning 트리는 비어 있다. 소스 index도 기존 상태와 동일함을 확인했다.
- 사용자가 기록은 해당 브랜치에서만 포함해야 한다고 재확인했다. 다른 브랜치에 코드를 통합할 때 문서/예외 규칙 커밋을 제외하고 코드 커밋만 cherry-pick한다는 필수 절차를 task_plan과 .gitignore 주석에 명시했다. 이후에도 코드와 이 기록은 별도 커밋으로 유지한다.
- 현재 Git 설정만으로 추적 파일을 브랜치명에 고정하거나 일반 merge에서 자동 제외하는 기능은 없다. 자동 차단을 구현했다고 주장하지 않으며, 임의의 merge driver·Git hook·전역 설정을 추가하지 않는다. 다른 사람이 직접 전체 브랜치를 병합하면 이 조건이 지켜지지 않으므로 통합 시 위 절차가 필요하다.
- 기존 기록을 Git notes나 다른 저장소로 이동하거나, 기존 커밋을 reset/rebase하지 않았다. 현재 요청의 범위는 지정 브랜치의 보관 조건과 이후 코드 통합 절차를 명확히 하는 것이다.
