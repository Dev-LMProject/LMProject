Lumia 코드 저장소

로컬 브런치 테스트:서휘도
## 서휘도 PR 요청 처리 테스트용 수정2
Git: https://github.com/daewoong12/LMProject.git
개발 브랜치: dev / 검증 브랜치: main

PR 반영 테스트: daewoong/git-workflow에서 dev로 문서 변경 전달

현재 시작점: Blank (2026-10-23)
- C++ 기본 모듈과 빌드 대상 파일 5개만 남겼습니다.
- Perforce의 Content에는 빈 맵 Maps/L_Blank.umap 하나만 있습니다.
- 기본 게임 모드는 엔진의 GameModeBase입니다. Third Person 캐릭터, 입력 에셋,
  전투/플랫폼/횡스크롤 샘플과 이전 P4 테스트 에셋은 제거했습니다.
- 새 코드, 에셋, 맵은 이 Blank 기준에서 작업합니다.

소유권
- Git: Source/**, Plugins/**/Source/**, 이 안내, .gitignore, .gitattributes.
- Perforce: Lumia.uproject, Config, Content, SourceArt, 플러그인 설명/콘텐츠,
  배포용 Binaries, .vsconfig, Build/EditorBuild.json, 팀 가이드.
- 같은 파일을 양쪽 저장소에서 직접 편집하고 제출하지 않습니다.
- 블루프린트(.uasset)와 맵(.umap)은 Perforce에서 관리합니다.

처음 준비
- 이 Git 저장소를 비어 있는 프로젝트 폴더에 clone합니다.
- 그 폴더를 P4V 개인 Workspace의 Root로 지정합니다.
- 서버: ssl:ugit.seohwido.site:1666
- 개발 스트림: //LMProject/Game/dev
- 검증 스트림: //LMProject/Game/main
- 같은 폴더로 Get Latest합니다. 서버가 Source를 제외하므로 Git 코드를 덮어쓰지 않습니다.
- Lumia.uproject를 Rider에서 열고 .vsconfig 기준 C++ 도구를 준비합니다.
- Unreal의 Revision Control은 Perforce, Rider의 코드 버전 관리는 Git을 사용합니다.

매일 작업
- 코드: Git pull -> 수정/빌드/검증 -> commit -> push.
- 에셋/프로젝트 설정: P4V Get Latest -> 수정 -> Submit.
- 코드와 에셋을 함께 수정하면 두 저장소에 각각 제출합니다.
- Git push와 Perforce Submit은 서로 자동 실행되지 않습니다.
- dev/main이라는 이름이 같아도 두 저장소의 브랜치는 자동 연결되지 않습니다.
- 기존 작업을 commit/stash 또는 shelve한 뒤 기준 버전을 바꿉니다.

빌드와 버전 맞추기
- 공용 배포본의 기준은 Perforce의 Build/EditorBuild.json입니다.
- 배포할 때 정확한 Git 커밋, Perforce 입력 변경 번호, 엔진 버전과
  실행 파일 해시를 기록합니다. 커밋되지 않은 코드로 공용 DLL을 만들지 않습니다.
- 현재 실행 기준: Epic Launcher UE 5.8.2, Win64 Development Editor.
- 코드가 필요한 개발자는 해당 커밋과 에셋 버전을 함께 맞춥니다.
- 아트 작업자는 Perforce에 배포된 DLL과 에셋만 받으면 됩니다.
- 새 모듈/플러그인은 설명 파일과 필요한 런타임 의존 파일도 Perforce에 배포합니다.

전환 기준
- p4-code-baseline-cl47 태그는 //LMProject/Game/dev/Source/...@47의 제출된 코드입니다.
- 초기 전환 때 보존했던 로컬 수정/삭제는 이후 Blank 정리 작업에 반영했습니다.
- Git 원격 업로드와 서버 Source 제외가 적용되었습니다.
- 기존 Perforce 코드 이력은 보존되지만, 일반 Workspace에서는 코드가 제외됩니다.
- 현재 배포 기준과 상세 설정은 Perforce의 TEAM_ENVIRONMENT.txt를 확인합니다.
- 자동 빌드/자동 Perforce 배포는 연결되어 있지 않습니다. 배포 담당자가 빌드를 제출합니다.

팀 Git 작업 절차
- 작업 시작 전에 dev를 Pull합니다.
- dev에서 작업별 브랜치를 생성합니다.
- 작업 브랜치에서 Commit과 Push를 진행합니다.
- 작업 브랜치에서 dev로 PR을 생성하고 검토 후 병합합니다.
- 검증이 끝난 dev는 main으로 PR을 생성해 반영합니다.

상호작용 HUD의 BP 연결 (PR #11)

- C++은 감지 후보의 태그 필터, 표시 대상 목록, 순환 선택과 실행 요청을 관리합니다.
- BP는 HUD 배치, 크기, 목록 행 생성, 텍스트, 폰트, 색상, 여백과 선택 강조를 관리합니다.
- 아래 연결은 변경된 C++을 사용자가 빌드한 뒤 적용합니다. 기존 위젯 이름과 디자인은 유지합니다.
- .uasset은 Perforce 관리 대상이므로 Git PR에 포함되지 않습니다. 이 PR은 기존 BP를 자동 변경하지 않습니다.

1. 기존 WBP_InteractionList의 Parent Class를 LMInteractionHUDWidget으로 지정합니다.
   기존 SizeBox/Border/ScrollBox와 디자인을 그대로 사용합니다. C++은 루트 타입이나 ScrollBox 이름을 요구하지 않습니다.
   제목과 조작 안내 TextBlock은 BP 디자이너에 배치합니다. 반복 목록만 비우도록 ScrollBox와 분리합니다.

2. 목록 행용 WBP_LM_InteractionEntry를 UserWidget 기반으로 만듭니다.
   TargetActor 변수(Actor Object Reference)를 Instance Editable, Expose on Spawn으로 설정합니다.
   표시 이름 TextBlock과 선택 표시를 디자이너에서 구성하고 폰트·색상·여백을 BP에서 설정합니다.
   BP Construct에서 TargetActor의 표시 이름을 TextBlock에 넣습니다.
   SetSelected(bool bSelected) BP 함수를 만들고 선택 표시·색상·애니메이션을 여기서 처리합니다.
   색상이나 폰트 값을 C++에서 전달받지 않습니다.

3. WBP_InteractionList의 Event OnInteractionTargetsChanged(Targets)를 구현합니다.
   ScrollBox Clear Children → 보관한 행 위젯 배열 Clear → Targets For Each Loop 순서로 연결합니다.
   각 TargetActor로 Create Widget(WBP_LM_InteractionEntry, Owning Player = Get Owning Player)을 호출합니다.
   TargetActor를 생성 핀에 전달하고 ScrollBox에 Add Child한 뒤 행 위젯 배열에 보관합니다.
   행 간격과 정렬이 필요하면 이 BP에서 반환된 슬롯을 설정합니다.
   Targets가 비었으면 목록 HUD를 Collapsed, 있으면 HitTestInvisible로 설정합니다.
   빈 목록을 숨길 때 Remove From Parent를 호출하지 않습니다. 구독을 유지해야 새 감지를 다시 표시할 수 있습니다.

4. Event OnInteractionSelectionChanged(TargetActor)를 구현합니다.
   보관한 행을 순회하며 Is Valid(TargetActor) AND 행.TargetActor == TargetActor를 SetSelected에 전달합니다.
   선택된 행이 있으면 Scroll Widget Into View를 호출합니다. null이면 모든 행의 선택 표시를 해제합니다.
   C++은 목록 변경 이벤트 다음에 선택 이벤트를 호출하므로 행을 다시 만든 뒤에도 남아 있는 선택이 표시됩니다.
   이 이벤트에서 첫 항목을 자동 선택하거나 Interact를 실행하지 않습니다.

5. 기존 WBP_UserHUD의 Parent Class를 LMUserHUDWidget으로 지정합니다.
   디자이너에 WBP_InteractionList를 자식으로 배치하고 이름을 InteractionHUD로 지정한 뒤 Is Variable을 켭니다.
   이 이름과 타입은 C++ BindWidget 연결에 필요합니다. 루트 패널 종류에는 제약이 없습니다.
   위치·앵커·정렬·여백은 WBP_UserHUD에서, 목록 크기와 글자 스타일은 각 위젯 BP에서 조절합니다.
   부모의 Construct에서 자식 위젯을 중복 생성하지 않습니다.

6. 플레이어 Controller BP의 Class Defaults → UI → UserHUDClass에 WBP_UserHUD를 지정합니다.
   컨트롤러가 이 BP를 생성하고 화면에 등록하므로 별도 Create Widget/Add To Viewport 노드를 추가하지 않습니다.
   UserHUDClass가 비어 있으면 HUD를 생성하지 않고 설정 누락을 알립니다.

7. 휠 선택 입력은 컨트롤러가 받습니다.
   ScrollBox의 Consume Mouse Wheel을 Never로 설정하고, 표시 위젯은 HitTestInvisible로 설정합니다.
   스크롤바 표시·폰트·색상·패딩·정렬은 BP에서 직접 정합니다.

빌드·UHT·BP 컴파일·PIE·게임 테스트는 사용자가 수행합니다. 이 연결 전에는 기존 BP만으로 새 HUD가 동작하지 않습니다.
