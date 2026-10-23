Lumia 코드 저장소

로컬 브런치 테스트:서휘도

Git: https://github.com/daewoong12/LMProject.git
개발 브랜치: dev / 검증 브랜치: main

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
