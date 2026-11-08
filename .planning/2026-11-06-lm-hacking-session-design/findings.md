# 근거와 결정

## Git
- Dev/shd/dev_HackingSystem, HEAD b06177d. origin/dev와의 차이는 12개 파일. 66f5304 보드, fdd63e7 태그·분기, 7f92490 enum, 5626b03 일반 대상 null 접근 수정. 브랜치 추적 파일의 미커밋 변경 없음.
- BuildCells, FLMHackingCell::ReceiveValue, SubmitMove, RetryPuzzle 및 조회 함수는 존재. TryStartHacking / IsCurrentSessionValid는 false, CancelHacking / ConfirmHacking / ReleaseSession / EndPlay의 세션 정리 미구현.
- 상태 원본은 ULMHackingComponent.HackingStateTag 단일 값(LM.State.Hacking.NeedHacking / HackedComplete). Interaction.HasTagContainer.HasAny는 타입 호환성 검사. 위키에 쓰인 어휘/소유자와 다르다.
- 프로젝트는 빌드·UHT·BP 컴파일·PIE·게임 검증을 사용자가 수행한다. 기존 빌드 매니페스트는 본 브랜치 검증 증거가 아니다.

## Notion (웹 본문 확인)
- [11/4 일일보고](https://app.notion.com/p/40eb2fadd6f380139f42db633c2ba027) — 보드, 같은 값 전파, 조작 횟수, 결과 후 다음 상호작용.
- [11/5 일일보고](https://app.notion.com/p/40fb2fadd6f3806f8457eff6a9de78f6) — 잠금 분기·null 수정. 결과 확인 후 즉시 원래 기능 실행을 검토했다는 기록. 공통 행동 판정 API는 할 일.
- [해킹·전선 장치](https://app.notion.com/p/40db2fadd6f38033974edcaee4f94f55) — 끝의 11/5 결정: 원 요청자의 ASC에 일시적 행동 제한 1개를 부여하고 회수. 세션·위젯·참조 정리는 컴포넌트, 컨트롤러 활성 대상 등록 없음. 성공 확인은 해금만, 다음 Interact에서 재판정 후 본래 기능. 전선/체크포인트 후속.
- [공통 해금 카드](https://app.notion.com/p/405b2fadd6f38107bf7cc32d68b71d70) — 행동 허용/금지 공통 API와 기존 행동 호출부는 별도 카드. 해킹 전용 if를 각 행동 코드에 추가하지 않는다.
- [16진수 해킹 위키](https://app.notion.com/p/40db2fadd6f380628b36ed410bf70113) — 정상 상태·지상만 진입, 스캔·입력 정리, 월드 정지, 화면 확인/강제 종료 구분, 외부 수신자 1회, 종료·롤백, 키보드와 마우스, 목록 잠금 표시. 11/4 초안이라 태그 위치·PC 소유는 코드/최신 카드로 교정한다.
- [해금 위키](https://app.notion.com/p/406b2fadd6f380ddbfe6ecc1959feb6c) — 전역 진행 태그/조건은 별도 설계 초안.

## UnrealMCP 읽기 전용 확인
- 실행 중인 에디터의 현재 맵 /Game/BigStarStation/Maps/Map_Demo1, 설치 엔진 5.8.4. /Game 이름 Hacking 검색은 0건. /Game/UI에는 WBP_LM_UserHUD, WBP_LM_InteractionList, WBP_LM_InteractionEntry. 로컬 이름 검색을 제작 여부의 절대 판정으로 사용하지 않는다.
- BP_InteractTest1/2의 Interact 함수 그래프와 CDO의 센서·LMInteraction 확인; 이 두 CDO에는 해킹 컴포넌트 없음. 종류 태그 LM.Interaction.Test1/Test2, BP_LM_PlayerCharacter의 InteractorComponent도 Test1/Test2. 이 값은 코드에서 제거된 과거 설정이 BP 자산에 직렬화된 사례다. 이번 설계의 유효한 시험 태그로 사용하지 않는다.
- BP_LM_PlayerController의 CDO에 WBP_LM_UserHUD 및 DA_LM_PlayerInputConfig. DA_LM_PlayerInputConfig의 IA_LM_Scan, DA_LM_PlayerStartupAbilities의 GA_LM_Scan은 모두 LM.InputTag.Scan. 런타임 활성 여부/종료·쿨다운 실행은 확인하지 않았다.
- MCP의 graph DSL 읽기는 관련 그래프에 빈 문자열을 반환했고 Nodes 속성 읽기는 지원하지 않았다. 노드 본문/실제 게임 실행은 검증 근거에서 제외. 현재 CDO/애셋 레지스트리는 P4 제출 상태 또는 HEAD DLL 일치를 보증하지 않는다.

## 사용자 결정
- 성공 화면 확인은 단말 해금만. 다음 E에서 다시 판정해 본래 기능 실행. ‘추가 입력 없이 실행’ 검토안은 채택하지 않는다.

## 독립 검토 처리 (Superpowers → Ponytail Ultra → mattpocock)
| ID | 조건·판정 | v2 반영 근거 |
|---|---|---|
| U1 높음 | 위젯 A 종료 후 같은 단말의 B가 열리고 A 지연 클릭/원래 시작 스택이 돌아옴 | 새 위젯마다 발신 신원 일치, 현재 예약 유효성 검사. 동기 시작 중 중단은 기록하고 외부 호출 반환 후 실제 취득 자원을 같은 스택에서만 되돌린다. |
| U2 높음 | 기존 FlushPressedKeys·ClearAbilityInput 취소 콜백에서 Pawn/Avatar가 교체됨 | 원래 Pawn·PC·ASC를 비교하고 ClearAbilityInput(ExpectedPawn) 사용, 콜백 후 바뀌면 물리 키 flush 건너뜀. 새 Pawn 입력을 해제하지 않는다. |
| U3 중간 | 해금 태그 선기록 뒤 완료 콜백에서 재요청하거나 BP가 일반 Busy를 덮음 | BP 일반 Busy와 해킹 예약을 분리, 실행 전에 합산. 확정 전달 끝까지 같은 단말 재진입 차단. |
| U4 중간 | 선행 일시정지/다른 UI의 모드·포커스를 덮거나 Playing에서 입장조건을 재사용 | 실제 viewport/capture/포커스로 정상 GameOnly를 확인하고 복구도 같은 소유자 한정. 활성 입력에는 자기 부착 위젯·원래 ASC와 예약을 확인. |
- Builder check: 추가 사용자 결정 없음. 현재 코드와 UE 5.8.4 소스에서 해결할 기술 선택으로 판정.
- 사용자 정정: 종류 구분은 Hacking 단말과 문으로 한다. Test1/Test2는 코드에서 제거됐다. 원본 BP CDO에 문자열이 남은 것과 태그 등록은 다르다.

- HEAD LMGameplayTags.cpp:42–43과 실행 중 GameplayTagsManager.ListTags(LM.Interaction)를 대조했다. 등록된 말단은 LM.Interaction.Actor.Hacking / LM.Interaction.Actor.ActiveTarget. 전체 등록 태그 조회에 이름 Door는 없었다. Test1/Test2 또한 미등록. 실제 태그와 BP의 남은 문자열 및 제안 태그를 분리한다.
- U5 중간: UE 5.8.4 GameViewportSubsystem::AddToScreen은 등록 → TakeWidget/Construct → Slate host 부착 순서. Construct 도중 직접 제거하면 뒤늦게 보이지만 등록되지 않은 잔상이 가능하다. 해당 구간의 실제 제거는 Add 반환 뒤로 미루는 계약을 v3에서 검토.
## 시험 태그와 v3 확정 결정
- 사용자 확인: 해킹 단말은 LM.Interaction.Actor.Hacking, 문은 현 LM.Interaction.Actor.ActiveTarget. 별도 Door/Test 태그 신설 없음. 시험 단말·문과 플레이어의 요청 태그를 같이 맞춘다. 대상 역할 태그는 NeedHacking/HackedComplete 및 세션 동안만 쓰는 플레이어 제한 태그와 별개다.
- U5 반영: 설계자는 자기 위젯 Add 중 RemoveFromParent를 시작 취소로 지연, Add 반환 뒤 실제 제거 1회로 정했다. 원래 위젯의 Construct → 제거 재현을 2단계에 포함했다. v1–v2의 구형 시험 태그 제안은 사용자 정정 및 라이브 등록표로 폐기했다.

## 최종 검토 판정
- Superpowers 설계: v1 → U1~U4 처리한 v2 → U5·종류 태그 정정을 처리한 v3. 제기된 필수 항목 5건 모두 반영, 기각 없음.
- Ponytail Ultra(Builder check 겸임): v3 전체 재검토, U1–U5 모두 해소, 남은 필수 설계 수정·사용자 결정 없음.
- mattpocock codebase-design: 동일 v3 전체를 현재 호출부·태그·ASC·UI와 독립적으로 대조, **남은 설계 수정 사항 없음**. 정적 설계 통과이며 구현 또는 게임 실행 통과가 아니다.
- 최종본 SHA256: 7DDBCFBC182B62CA035E49A1B7DA158B7BFE7BCA5564A8F26733D0C14D15FD3A. 남은 통합 의존성: 다른 행동의 공통 API 연결, 실제 회피 생산자가 외부 이동·회피 상태를 제공하는지 검증. 새 기능 개발 때 사용자가 빌드·UHT·BP 컴파일·PIE를 수행한다.

## 1단위 안내 전 현물 확인
- 같은 HEAD b06177d, Git 추적 변경 없음. 라이브 태그 관리자에서 Hacking/ActiveTarget 및 NeedHacking/HackedComplete 확인. 플레이어 BP InteractorComponent의 기본 요청 목록에는 과거 값만 있어 정식 두 종류로 교체해야 한다. /Game 이름 Hacking 조회 0, 현재 맵 Map_Demo1.
- UnrealMCP find_nodes와 get_node_infos로 실제 핀을 확인했다. WBP_LM_InteractionList의 CreateWidget.TargetActor에 순회 대상이 연결됨. WBP_LM_InteractionEntry의 SizeBox_0/Overlay_0 안에 ActorName(TextBlock), Background, Selected가 있다. Construct → SetText(ActorName), 값은 TargetActor → GetDisplayName → ToText(String). 1단위에서는 그 이름에만 [잠금]을 표시한다.
- BP_InteractTest1의 BeginPlay → SetDetectionCollision(LMInteraction, InteractionSensor), Interact → PrintString → SetIsBusy → true 반환을 확인했다. 센서/인터페이스 재사용과 폐기한 종류 태그는 별개다.

## 2026-11-07 · 1단계 잠금 표시 보완
- 사용자 표시 기준: 잠긴 행에는 자물쇠 이모지 또는 이미지와 빨간 이름, 해제·일반 행에는 자물쇠 없이 원래 이름색. 해금 시점·종류 태그·세션 계약은 변경하지 않는다. 이 보완은 v3의 행 표시 방식 선택이다.
- 실행 중인 UnrealMCP에서 WBP_LM_InteractionEntry를 읽었다. Construct → 유효 TargetActor → 원래 이름 SetText(ActorName) → LMHackingComponent 유효성 → GetHackingStateTag → Switch(NeedHacking) → FormatText/SetText. 현재 Format 핀에 이름 문자열의 ToText 출력이 연결돼 있고 Name 인자 핀은 없다. SetSelected는 Selected 이미지의 Visibility만 바꾼다.
- 현재 ActorName의 디자이너 색은 SlateColor UseColor_Specified RGBA (1,1,1,1), 폰트 애셋은 엔진 Roboto. 이모지 렌더링은 확인하지 않았다. 기존 이름색을 OnInitialized에서 한 번만 보관하고, 표시를 평가할 때마다 일반 이름/기본색을 먼저 적용한 뒤 잠긴 경우만 자물쇠/빨강으로 덮는다. 상태 변경 시 같은 표시 로직을 호출하는 것은 해금 확정 단위에서 연결한다.

## 2026-11-07 · Smart Object 활용 평가
- 소스·Config·uproject의 관련 참조와 Content 파일명 검색에서 현재 SmartObjects/GameplayInteractions 사용 근거를 찾지 못했다. 이름 검색으로 모든 BP 사용 여부나 플러그인 비활성화를 단정하지 않는다.
- 현재 단일 로컬 플레이어의 주변 후보·목록 선택·E 요청·잠금 해킹은 기존 Interactor/Interaction/Hacking 구조가 담당한다. Smart Object를 추가해도 퍼즐·월드 정지·UI 신원·ASC 정리·해금 후 다음 E 계약을 대신하지 않는다. 현재 도입 이점은 낮다고 권고한다.
- 사용 위치/방향 슬롯, 한 오브젝트의 다중 자리, NPC/플레이어 간 예약 경쟁, AI의 활동 검색 요구가 생기면 가치가 커진다. Smart Object는 AI뿐 아니라 플레이어도 사용할 수 있다. 슬롯의 Free/Claimed/Occupied와 HackingStateTag의 잠금 상태는 다른 상태다.
- 선택 도입 시 액터 BP/별도 조합 부분에서 예약을 얻고 실제 행동 세션 종료/실패/취소에 반납한다. 즉시 끝나는 입력 GA 수명에 예약을 묶지 않는다. 두 기존 컴포넌트에 SO 타입 참조를 퍼뜨리지 않는다.
- UE 5.8의 Experimental InteractionInterface에는 UGameplayAbility_Interact 및 Instigator/Target 컴포넌트가 있다. 자체 검색·FInteractionContext 계약을 사용하므로 현재 LM의 후보/목록/선택 및 일회성 입력 GA와 교환 가능한 구현이 아니다. 전체 전환을 이번 범위에 넣지 않는다. 이 실험 기능 표시를 기본 SmartObjects의 상태로 혼동하지 않는다.
- 근거: https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine---overview , https://dev.epicgames.com/documentation/en-us/unreal-engine/API/PluginIndex/InteractionInterface 및 설치 UE_5.8 원본. designer_v4도 같은 범위를 읽기 검토했다. 실행 비교·성능 측정은 하지 않았다.

## 2026-11-07 · 선택 대상 getter 배치에 대한 Matt 집중 검토
- 질문은 LMPlayerController의 GetSelectedInteractionActor가 자연스럽고 발견 가능한 위치인지다. codebase-design SKILL과 DEEPENING을 읽고 최신 소스·호출부·미커밋 변경을 확인했다. 기존 validator_v4도 동일 범위를 독립 검토했다. 전체 새 설계의 세 역할 통과로 표시하지 않는다.
- PC는 이미 HUD 생성/보관·이전/다음 선택 입력·기존 선택 대상 실행 전달을 맡는다. HUD는 SelectedActor/DisplayedTargets·태그 필터와 구독·선택 무효화·Pawn 변경 초기화를 소유한다. 플레이어의 Interactor는 후보/요청 태그/특정 대상 TryInteract 검증을 소유한다.
- 현재 PC getter는 GA가 HUD 클래스·위젯 계층을 모르도록 하는 작은 Adapter로 타당하다. 한 줄 전달 함수라는 이유만으로 제거하지 않는다. 다만 상호작용 도메인으로 처음 탐색하면 Interactor를 먼저 찾을 수 있어 누구에게나 자명한 위치라고 단정하지 않는다.
- 현재 변경 최소 권고는 PC getter 유지 + 선택 출처와 nullptr 계약 주석 + TryInteract의 실행 전 재검증이다. 아직 사용자는 GA 전환 여부를 최종 확정하지 않았다.
- 선택 수명을 HUD와 분리하거나 여러 소비자가 공통 선택을 사용할 실제 요구가 생기면, SelectedActor와 선택 변경·필터 대상/무효화 규칙을 함께 플레이어측 Interactor로 옮길 수 있다. getter만 옮겨 Interactor가 HUD를 역참조하거나 선택 상태를 두 군데에 복제하는 것은 권고하지 않는다. 대상측 LMInteractionComponent는 선택 소유자가 아니다.

## E 입력 GA 구현의 실제 검증 근거
- Build-LumiaEditor-authorized.log: Result: Succeeded / LumiaEditor Win64 Development / 26.16s. 에디터 재실행 후 새 native class와 입력 tag 조회됨.
- Saved/LMContext/LMInteractInput/asset_verification.json: PASS. 원본 입력 항목 전체 보존과 실제 PC/PS 참조 연결 확인. 새 GA는 LocalOnly / InstancedPerActor / retrigger false / Dead·Stunned·Input.Blocked 차단 / 비용·쿨다운·소유태그 없음.
- BlueprintTools.compile_blueprint(warnings_as_errors=True) 성공. K2_ActivateAbility, K2_OnEndAbility 등 이벤트 override 없음. Comment는 BlueprintTools.find_nodes에서 보이지 않아(이 API의 K2 노드 목록) UI의 실제 박스 및 저장된 UTF-16 내용을 대조해 검증했다.
- Perforce 지문 불일치: 인증서/신뢰 설정을 변경하지 않고 대상 3개 파일만 백업 후 쓰기 가능하게 하여 로컬 저장. 원격 checkout/reconcile/submit은 확인되지 않음. 모든 요청 대상 에셋 dirty=False 확인.
- 실행 검증 한계: PIE/게임 플레이와 실제 동기 재진입·취소 시나리오 미실행. 정적 검토/빌드/에셋 연결 통과와 별개다.

## 2026-11-07 · Oracle SSH로 Perforce 지문 불일치 원인 확정
- 사용자 명시 요청으로 SSH 별칭 Oracle에 BatchMode/StrictHostKeyChecking=yes를 사용해 접속했다. SSH 호스트 검증을 우회하지 않았다. 인증서/키/신뢰/보안/서비스 설정은 변경하지 않았다.
- shd-oracle의 p4d(pid 2975441)는 2026-10-27 03:48:16 UTC부터 실행 중. P4SSLDIR=/blkVol/perforce/ssl. certificate.txt/privatekey.txt의 수정 시각은 2026-10-18 14:31:32 UTC이며 인증서 유효기간은 2026-10-18~2028-10-17.
- 서버 p4d -Gf 및 localhost:1666에서 받은 공개키의 SHA-1은 4C:67:5F:44:34:1A:76:98:8D:8B:10:7E:07:4A:A1:D6:5A:28:51:BC. PC p4trust.txt에 저장된 값과 동일하다. 서버 인증서 자체의 SHA-1(10:6A:...)과 Perforce 공개키 지문을 혼동하지 않았다.
- PC에서 도메인 및 IP로 외부 접속하여 수신한 공개키 지문은 3A:A7:F7:22:92:72:5D:DF:54:CF:C0:A1:C2:5C:82:80:E8:F3:5D:8E로 오류 내용과 일치. subject는 shd-oracle이나 issuer는 OU=generated by Norton Antivirus for self-signed certificates, O=Norton Web/Mail Shield, CN=Norton Web/Mail Shield Self-signed Root.
- 확정 원인: PC의 Norton 암호화 통신 검사에서 대체 인증서를 제시해 Perforce 공개키 지문 pinning과 충돌. 기존 PC trust가 오래됐거나 서버 키가 교체됐다는 앞선 가능성은 이번 실제 비교로 배제한다. trust 강제 교체는 실시하지 않음. 필요한 후속은 사용자가 승인한 범위에서 Norton의 해당 연결 검사 예외 방식 확인.
- 진단 시행착오: remote here-doc CRLF를 tr로 정규화. p4d -Gf 최초 호출은 작업 디렉터리 권한 오류로 P4ROOT를 정확히 지정해 읽기 재실행. 서버가 자기 public IP로 접속하는 비교는 timeout이므로 판단 근거에서 제외. Windows Git OpenSSL은 sandbox named-object 제한으로 실패하여 승인된 비샌드박스 읽기 검사로 확인했다.

## 2026-11-08 · 분리 단위 안내의 범위
- v4-r1의 일반 interface hook 두 개와 세 결과 enum 계약을 현재 소스에 대응했다. 순수 Busy 조회는 상태만 읽고, Handler/Interact 반환 전까지 DispatchGuard를 유지하며 PassThrough 내부 재평가만 자기 가드를 제외한다.
- IsInteractionReserved 조회를 추가해도 미구현 세션을 완성하는 것은 아니다. 다음 세션 구현에서 시작 rollback/완료 콜백까지 예약 범위를 보장해야 한다. 이 제한을 안내와 시험 표에 명시했다.
- BP_NeedHacking/BP_HackingCompleted는 둘 다 /Script/Engine.Actor 직접 자식이다. 두 BP 각각 hook 구현 필요. 일반 문은 기존 ActiveTarget/Interact와 default false/PassThrough를 유지한다.

- 인터페이스 조회 선언 정정: BlueprintPure를 명시하면 UE 5.8 UHT가 거절한다. BlueprintNativeEvent + BlueprintCallable + const 반환 함수로 선언하면 AutoBlueprintPure가 설정되며 해당 검증에서 제외된다. HackingComponent 같은 일반 UCLASS의 BlueprintPure에는 이 인터페이스 제한이 적용되지 않는다. 기존 안내의 잘못된 매크로를 수정했다.


## 2026-11-08 · 결합 분리 적용 현황 읽기 확인
- 점검 중 HEAD가 4617f64에서 사용자 커밋 d37d27e로 갱신됐다. 최종 git status는 clean. 제품 변경 없음.
- LMInteractionComponent는 generic IsInteractionBusy/HandleInteractionRequest와 DispatchGuard를 사용하고 Hacking 구체 타입/상태 태그 참조가 없다. Hacking도 Interaction 구체 타입 필드가 제거됐다.
- BP_HackingCompleted: IsInteractionBusy의 LMHacking 유효성/예약 조회 반환 연결 정상. Handler에서 HackingStateTag -> Switch Selection, NeedHacking -> TryStartHacking -> Branch(Accepted/Rejected), HackedComplete -> PassThrough, Default/invalid -> Rejected 연결 확인. 단 TryStartHacking self(Target) 핀은 무연결. RequestActor -> InInteractor는 연결됨. dirty=True(점검 전부터 미저장).
- BP_NeedHacking: 두 hook이 아직 Entry -> Return(false / PassThrough)뿐이다. C++ 해킹 분기가 제거된 상태에서 이를 그대로 실행하면 잠긴 액터도 일반 Interact로 통과할 수 있다. 이 BP와 Completed는 독립 Actor 자식이므로 양쪽에 적용 필요.
- BP_InteractTest: false/PassThrough 유지. 일반 Interact는 기존 Print String 후 SetIsBusy(false), true 반환. 두 단말의 기존 Interact 역시 요청자 이름 출력 후 SetIsBusy(false), true 반환이다.
- 현재 맵 /Game/BigStarStation/Maps/Map_Demo1에 세 BP 인스턴스 존재. Need/Completed는 LMHacking을 소유하며 상태 태그 각각 NeedHacking/HackedComplete. 둘의 종류 컨테이너에는 ActiveTarget와 Hacking이 함께 있고 일반 액터에는 ActiveTarget만 있다. 새 태그 추가 불필요.
- IMC E -> IA_LM_Interact, InputConfig LM.InputTag.Interact, AbilitySet GA_LM_InteractInput 매핑 각각 1건 확인. Started 입력 및 ASC 중복 held 방지 코드 확인.
- TryStartHacking은 false, Cancel/Confirm/ReleaseSession은 미구현, IsCurrentSessionValid도 false. 현재 단계에서는 Reject와 PassThrough 분기만 실제 정상 흐름에서 검증 가능하며 해킹 UI/성공/취소/진행 중 예약은 아직 검증 대상이 아니다.
- 증거: Saved/LMContext/LMInteractionDecouplingReview/{*_graphs.json,final_hook_readback.json,scene_components.json,input_links.json}. 최종 hook 재조회에서도 같은 누락과 dirty 상태 확인.


## 2026-11-08 · 동일 hook의 BP 구현 필요성 질의
- 사용자는 두 인터페이스 hook의 반복 BP 구현 필요성을 질문했다. 구현/리팩터 승인으로 확대하지 않았다.
- 현재 일반 액터의 false/PassThrough와 해킹 액터의 예약·상태 분기는 서로 다르지만, 해킹 단말들끼리의 정책은 같다. 상태 설정 차이 때문에 같은 그래프를 자식 BP마다 복제할 필요는 없다.
- 결합도는 BP/C++ 언어 선택이 아니라 구체 타입 의존 위치로 결정된다. 소유 액터 공통 부모가 조합을 담당하면 Interaction/Hacking 상호 직접 의존 없이 BP 또는 C++ 한 곳에서 구현을 재사용할 수 있다. 최소 에셋 변경 대안은 공통 부모 BP; 고정 규칙을 코드로 관리할 대안은 해킹 가능 액터 C++ 부모이다. 두 방법은 선택 제안이며 최신 설계를 변경/확정하거나 독립 재검토 완료로 기록하지 않는다.
- C++ 부모 채택 시 기존 자식 BP override가 부모 구현을 가리지 않도록 정리해야 한다. Actor 상속을 공유할 수 없는 다른 액터까지 무조건 같은 부모를 강제하지 않는다.


## 2026-11-08 · 추적 로그 추가
- 라이브 공통 부모 BP_HackingActor의 Interact는 InteractAction -> SetIsBusy(false) -> true 반환. 두 자식은 InteractAction만 구현. 이전의 Target 누락은 현재 공통 부모에서 연결돼 있다.
- 맵에는 공통 부모 인스턴스 1개와 두 자식 테스트 인스턴스가 존재한다. 공통 부모의 InteractAction 자체는 빈 함수이므로 자식 전용 출력은 자식 인스턴스에서만 나온다.
- C++ Input.Pressed -> GA.Begin -> TryInteract -> Evaluate/Handle -> Interact -> GA.Result 단계에 공통 표식을 추가했다. 조회 결과를 출력할 때 IsInteractionBusy를 추가 호출하지 않았으며 콜백 이후 이름은 사전 복사본을 쓴다.
- BP 부모에서는 유효성 통과 후 Switch 직전에 요청자/해킹 상태를 Output Log로 출력한다. 자식의 기존 화면/로그 출력에 BP.Action 표식을 추가하고 잠긴 자식의 문자 \n을 실제 개행으로 교정했다.
- Saved/LMContext/LMInteractionTrace/verify_trace_assets.py의 readback PASS: 기존 분기와 enum 반환, Busy 조회, InteractAction 호출 연결 보존. 컴파일/PIE 검증은 아님.

- 저장 확인 완료: AssetTools.save_assets는 내부 save_asset의 only_if_dirty 기본값을 사용한다. 자식 pin 값 변경만으로 dirty가 표시되지 않아 Append 노드를 16 단위 이동해 수정 표시를 만들고, 지정한 BP 세 개만 저장했다. 세 에셋 dirty=False 및 파일의 추적 문자열/해시 확인. 저장 후 그래프 readback 재검증도 PASS.
- GetNameSafe(GetPawn())에서 필요한 Pawn 타입을 명시적으로 include했다. enum 문자열은 UEnum::GetValueAsString과 UObject/Class.h를 사용한다. 소스 검색상 Interaction/Hacking 구체 타입 상호 의존은 재도입되지 않았다.


## 2026-11-08 · 구현 점검 중 로그 제거 요청
- C++ 현재 HEAD d37d27e, 작업 트리 clean. 앞서 추가한 5개 .cpp의 [LMInteraction] 로그는 이미 소스에서 제거된 상태여서 임의로 재적용하지 않았다. 기존 설정 오류 Warning과 GA의 기존 Verbose 실패 메시지는 이번 추가 추적 로그에 포함되지 않는다.
- 사용자 PIE 로그 17:28대 근거: BP_HackedCompleteTest_C_1은 PassThrough/Interact/자식 Action/Started=1 BusyAfterInteract=0/성공이 4회 일치. BP_NeedHackingTest_C_3은 SessionNotImplemented -> Rejected/ExecutionFailed가 3회, Interact/Action 호출은 0회. 무대상 요청은 InvalidRequestContextBeforeCommit 1회. 이는 현재 stub 단계의 기대 결과다.
- 로그 제거를 위해 실행 중 PIE를 중지했다. 시작/재시작 테스트는 하지 않았다. 최초 에셋 load가 PIE 중 허용되지 않아 Asset does not exist로 보였으나 실제 경로는 유지됐다. 중지 후 정상 조회했다. 해당 도구 오류는 게임 런타임 오류가 아니다.
- BP_HackingActor의 추가 로그 노드 4개를 제거하고 IsValid -> Switch 실행선 복원. 두 자식 InteractAction의 출력 전용 노드 각 6개를 제거해 Entry만 남겼다. 부모의 InteractAction 호출 -> SetIsBusy(false) -> true 반환, Busy 조회, 모든 상태 분기와 반환은 그대로다.
- Saved/LMContext/LMInteractionTraceRemoval/verification.json: 부모 기능 그래프가 로그 추가 전과 일치, 자식에 출력 노드 없음, 3개 에셋 저장/dirty=False/디스크 변경 확인. 원본 백업 및 실행 근거도 같은 디렉터리에 보존.
- 제거 후 빌드/BP 컴파일/PIE는 미실행. 기존 엔진 메모리에서 C++ 추적 로그가 실제 출력됐으므로 현재 소스로 재빌드가 필요하다. 과거 로그 파일은 삭제하지 않았다.

## 2026-11-08 · 다음 단위 준비: 해킹 화면
- 현재 HEAD d37d27e, git status clean. TryStartHacking은 false, Cancel/Confirm/ReleaseSession은 빈 함수, IsCurrentSessionValid도 false다. LMUI에는 InteractionHUD/UserHUD만 있다. PC에는 UserHUDClass만 있고 해킹 위젯 클래스 설정은 없다.
- Unreal MCP IsPIERunning=false, /Game의 Hacking 이름 검색에 위젯 에셋 없음. BP_HackingActor/BP_NeedHackingTest/BP_HackingCompleted 및 T_LM_HackingLock만 조회됨. 과거 BP_HackedCompleteTest 경로를 현재 에셋이라고 단정하지 않는다.
- 승인된 v3+v4-r1의 역할 분리는 유지한다. 위젯은 요청을 전달하고 컴포넌트가 세션·화면 수명을 소유하며 PC에는 위젯 클래스 기본값만 둔다. 이번 설명은 기존 세션 단위를 화면 준비와 실제 세션 연결로 세분화하는 것으로 계약 변경/새 독립 설계 검토가 아니다.
- 최소 위젯 부모는 약참조 초기화/해제, 닫기 요청 전달, ESC 입력만 포함한다. 새로 호출하는 Cancel/Confirm에 발신 위젯 인자를 준비한다. SubmitMove/Retry 발신 인자·현재 위젯 동일성 검사는 세션/퍼즐 연결에서 완성해야 한다. 기존 취소/확인 stub을 완성된 검증으로 표시하지 않는다.
- 설치 UE 5.8 UserWidget.h에서 FObjectInitializer 생성자, SetIsFocusable, NativeOnPreviewKeyDown 서명, SlateCore Input/Reply.h와 Input/Events.h를 확인했다. 기존 Lumia.Build.cs에 UMG/InputCore는 있고 SlateCore는 명시돼 있지 않다. 공개 위젯 헤더의 Slate 타입 의존에 SlateCore를 추가하는 예시를 준다.
- Epic 5.8 OnPreviewKeyDown 문서에서 자신/자식에 포커스가 있을 때 preview가 먼저 처리됨을 대조했다. ESC만 처리하고 다른 키는 Super로 위임한다. 버튼 및 ESC가 직접 RemoveFromParent를 호출하지 않는다.
- 읽기 시행착오: PC 경로를 Public/Player로 잘못 추측한 읽기 1회 후 실제 Public/LMPlayer 경로로 수정. Content는 Git ignore 때문에 기본 rg --files에서 빠져 --no-ignore 및 MCP registry로 확인. 큰 묶음 출력이 잘려 이후 읽기를 축소했다.
