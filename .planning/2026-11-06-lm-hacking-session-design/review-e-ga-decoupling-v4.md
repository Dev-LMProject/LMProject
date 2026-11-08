# E 입력 GA와 컴포넌트 분리 검토 · v4-r1 제안

사용자 요청: E 상호작용을 기존 GAS 입력 체계에 넣는 것을 Ponytail Ultra로 평가하고, HackingComponent와 InteractionComponent의 직접 결합을 없애며 BP로 조합할 수 있는지 설명한다. 설계 제안이며 구현 승인이 아니다. 기존 v3의 세션 안전 계약·성공 확인 후 다음 E·외부 보상 1회는 유지한다. 이번 기록은 변경부만 다룬다.

## 확인한 현재 코드
- shd/dev_HackingSystem @ b06177d. 검토 시작 시 사용자 변경은 HackingComponent.h 빈 줄 두 개였고, 검토 중 public IsInteractionReserved() 선언이 추가됐다. 구현과 호출은 아직 없다.
- PC의 E BindKey는 ASC를 우회한다. BindAbilityInputs는 Started/Completed/Canceled를 입력 태그로 ASC에 보낸다.
- ASC QueueAbilityInput은 Spec.GetDynamicSpecSourceTags().HasTagExact로 매칭, ProcessAbilityInput은 pause/Input.Blocked/Avatar 없음과 InputGeneration으로 중단한다. ClearAbilityInput은 HeldInputs/PendingInputs/InputPressed에서 모은 핸들을 취소하므로 해킹 시작 스택 안의 Interact GA도 취소할 수 있다.
- 실제 DA_LM_PlayerInputConfig에는 Scan/Attack/Special, StartupAbilities에는 같은 입력 태그의 3 GA가 있다. IMC_LM_Player에 E는 없다. GA_LM_AttackInput은 InstancedPerActor/LocalOnly/재트리거 false, ActivationOwnedTags 없음, Dead/Stunned/Input.Blocked 차단이다.
- Interaction.RequestInteraction은 구체 HackingComponent와 해킹 태그를 조회해 TryStartHacking으로 보낸다. 일반 동작만 bIsBusy=true 후 ILMInteractable::Execute_Interact. GetIsBusy는 bIsBusy만 반환한다.
- Hacking 헤더의 InteractionComponent 약참조와 bOwnsBusy는 세션 미구현 상태의 잔여 선언이다. TryStartHacking은 false, 실제 세션은 미구현이다.

## 추천 계약
### 1. E는 한 번 요청하는 GA
- Boolean IA_LM_Interact + LM.InputTag.Interact + IMC E + InputConfig + StartupAbilitySet의 일치하는 한 GA. 기존 E BindKey 제거. 기존 입력 GA의 InstancedPerActor/LocalOnly/재트리거 false 정책을 따른다.
- 활성화 때 원래 Pawn/PC/ASC와 선택 대상 문맥을 확보하고 기존 Interactor.TryInteract 경로로 1회 전달한다. 공개되지 않은 PC 선택 요청 경로에는 필요한 최소 접근만 연다. 소유자 PlayerState와 Avatar Pawn을 혼동하지 않는다.
- 입력 GA는 UI/pause/Active 태그/세션 수명을 소유하지 않는다. OnEndAbility가 해킹 취소/월드 복원을 호출하지 않는다.
- 추천 순서: 동기 요청 → 여전히 같은 활성화가 살아 있을 때 종료. ClearAbilityInput으로 요청 중 취소된 GA는 이미 종료된 것으로 처리한다. 같은 인스턴스의 재활성화가 가능하므로 현재 활성화 serial을 지역 복사해 반환 후 비교한다. 새로운 활성화를 이전 스택이 종료하지 않는다. 각 Activate에서 serial 증가, 요청 이후에는 로컬 약참조와 serial만 사용한다. 일반 성공·거절·대상없음 경로도 남은 자기 활성화 종료.
- 대안 EndAbility→요청은 End콜백의 Pawn/대상 변경과 scope lock의 종료지연을 추가 처리해야 해 우선 추천하지 않는다.
- 일반 차단은 기존 입력 GA 공통 규칙, 해킹의 지상·공격 등 조건은 해킹 진입에 둔다. GA 도입이 모든 직접 호출 차단을 자동 보장하지 않으며 공통 행동 판정 통합은 기존 별도 범위다.

### 2. 해킹 예약 조회
- IsInteractionReserved는 Starting/Playing/Failed/AwaitingConfirmation와 시작중단/cleanup/완료콜백 전달 중 true다. Completed라도 완료 콜백 동안 정리/전달 가드가 true면 예약을 유지한다.
- Hacking의 BlueprintPure 상태 조회로 노출한다. 상태를 별도 BP bool에 복제하지 않는다.
- 일반 bIsBusy와 해킹 예약은 독립이다. 일반 SetIsBusy(false)는 해킹 예약을 풀지 못하고, 해킹 종료는 외부 일반 Busy를 해제하지 않는다.

### 3. 기존 ILMInteractable의 두 BP 연결 지점
- bool IsInteractionBusy() const — BlueprintNativeEvent/순수 조회, native 기본 false.
- ELMInteractionRequestHandling HandleInteractionRequest(AActor* RequestActor) — BlueprintNativeEvent, native 기본 PassThrough.
- enum은 같은 인터페이스 헤더에 PassThrough/Accepted/Rejected 세 값. FLMInteractionResult를 인터페이스에 가져와 헤더 순환 의존을 만들지 않는다.
- Query는 조회만 수행하며 다시 Interaction.GetIsBusy를 호출하지 않는다(재귀 금지). 두 기본 구현을 사용하면 기존 일반 액터 동작을 유지한다.
- Actor BP의 Query는 Hacking.IsInteractionReserved를 반환한다. Handler는 NeedHacking→TryStart true=Accepted/false=Rejected, HackedComplete→PassThrough, 미지원/필수 컴포넌트 무효→Rejected다. TryStartHacking은 BlueprintCallable로 노출한다.
- 해킹 액터에는 두 hook 구현이 필수다. hook 전체 미구현을 일반 컴포넌트가 해킹 클래스 조회 없이 자동 판별할 수는 없으므로 애셋 구성 검사와 테스트에 포함한다. 일반 액터의 default PassThrough는 호환성을 위한 값이다.
- bool 하나의 handler는 미처리와 시작 거절을 구별하지 못한다. 잠긴 대상 시작 실패를 PassThrough로 해석하면 본래 기능이 실행되므로 세 값이 필요하다.

### 4. Interaction의 요청 경계
- GetIsBusy = bIsBusy OR bIsDispatchingRequest OR Owner.IsInteractionBusy(). 순수 Query는 부작용 없이 상태만 반환한다.
- 최초 유효성/태그/인터페이스/Busy 평가 통과 후, 첫 handler callback 전에 bIsDispatchingRequest를 세우고 반환까지 유지한다. 자기 최초 평가 전에 dispatch 가드를 세워 스스로 Busy 처리하지 않는다.
- Handler를 generic interface Execute로 호출한다. 반환 후 Owner/RequestActor/컴포넌트 종료 상태 재검사.
- Accepted/Rejected는 즉시 결과 반환하고 일반 bIsBusy는 쓰지 않는다. 미지원 enum도 Rejected로 처리한다.
- PassThrough에서만 현재 종류 조건·일반 bIsBusy·외부 예약을 재평가한 뒤 기존 bIsBusy=true→Execute_Interact를 실행한다. 내부 재평가는 자신의 dispatch 가드를 제외한다. 기존 일반 성공/실패·Busy 계약을 유지한다.
- Hacking.TryStart의 canonical 호출자는 이 일반 gate를 통과한 actor handler다. Hacking 자체는 원래 requester/Pawn/PC/ASC/world/lock/session 조건을 검사하고 구체 Interaction 타입을 조회하지 않는다. 다른 시스템의 상호작용 요청도 기존 Interactor/RequestInteraction 경로를 사용한다.

### 5. 제거할 직접 참조와 BP 조합
- Interaction.cpp의 해킹 include/FindComponentByClass/해킹 태그 분기, 앞서 제안한 GetIsBusy 직접 해킹 조회.
- Hacking.h의 Interaction 전방선언·필드·bOwnsBusy, 해킹의 일반 SetIsBusy 호출/조회.
- 소유 액터 BP가 두 독립 컴포넌트를 조합한다. 네트워크 새 구조, 범용 handler 등록기, lease 관리기, 매 Tick 동기화, 새 상속 계층은 추가하지 않는다.
- 이벤트로 IsBusy를 복제하는 대안은 종료 누락·외부 Busy 덮어쓰기·Completed 콜백 창을 다시 관리해야 하므로 권장하지 않는다.

## 필수 검증
1. 일반 문 기존 success/failure/Busy 동작 및 미구현 hook 기본값.
2. 잠긴 단말 시작 거절 시 원래 Interact 0회, 완료 후 다음 E만 1회.
3. Starting/Playing/완료 전달 중 SetIsBusy(false) 후 재요청 거절, 해킹 종료 후 별도 일반 Busy 보존.
4. handler가 대상 파괴/태그·Busy 변경 후 PassThrough를 돌려도 안전한 재판정.
5. E 한 번 요청 1회, input 태그·GA 중복 등록 없음, 길게 누름 자동 재요청 없음.
6. 해킹 진입의 ClearAbilityInput이 Interact GA를 취소해도 해킹 화면 유지.
7. 같은 프레임 E/공격·스캔 입력, 취소 중 Pawn교체, GA 종료 중 재활성화에서 이전 호출의 새 입력/새 활성화 침범 없음.
8. Hacking 액터 BP의 hook 누락/필수 컴포넌트 누락 구성 확인.
9. 두 컴포넌트의 소스에서 서로 구체 타입 참조 제거 검색.

## 독립 검토 결과
- Superpowers 설계자의 v4-r1을 Ponytail Ultra/Builder check와 mattpocock 최종 검증자가 실제 코드와 각각 대조했다. Ultra 필수 지적 0, 추가 제품 질문 0, mattpocock 판정 ‘남은 설계 수정 사항 없음’.
- 두 검토자가 읽은 상태 기록 추가 전 본문 SHA256은 82881AB6ED6C141691D7BC52E945EB8210DCDE85C556DDFF354C41A2586E3C41이다. 이후 변경은 검토 결과와 사용자 선언 추가의 현황 기록뿐이다.
- 해킹 액터 BP의 두 hook 구성은 필수이며, 일반 액터 기본값은 false/PassThrough다. 구성 누락 및 GA 동기 취소/재활성화는 구현 단계에서 실행 검증해야 한다.
- 설계 추천안의 검토 결과다. 사용자 코드에 적용하거나 전체 v3 구현을 완료했다는 뜻이 아니다. 소스/BP 변경 및 빌드/PIE는 수행하지 않았다.
