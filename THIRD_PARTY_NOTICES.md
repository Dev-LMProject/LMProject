# Third-party notices — LMProject Codex context preservation

이 문서는 이 저장소에 포함한 Codex 컨텍스트 보존 스킬·훅의 외부 소스 고지다. 사용자 소유 게임 코드에 대한 라이선스 선언은 아니다.

## planning-with-files

- Upstream: https://github.com/OthmanAdi/planning-with-files
- Version/tag: `v3.27.0`
- Fixed commit: `b6580cd24af1d8a95ab268794456e05608a1afd3`
- SPDX-License-Identifier: `MIT`
- Copyright: `Copyright (c) 2026 Ahmad Adi`
- Original license: [LICENSE at the fixed commit](https://github.com/OthmanAdi/planning-with-files/blob/b6580cd24af1d8a95ab268794456e05608a1afd3/LICENSE)

### 포함 범위와 변경 고지

- 원본 스킬·템플릿·보조 스크립트 및 Codex Python/셸 어댑터 49개를 포함한다. `.codex/hooks/codex_hook_adapter.py`, `context_frame.py` 등도 이 upstream 소스에 속한다.
- 46개는 고정 버전 배포본과 바이트가 같다. 그중 `pwf-hook.cmd`는 upstream `.gitattributes`의 `*.cmd text eol=crlf`에 따라 CRLF로 배포하며, Git의 LF 원문과 내용은 동일하다. 파일별 목록은 Git 원문과 배포본의 해시를 구분한다.
- 다음 3개만 `.codex/skills/` 참조를 `.agents/skills/`로 변경했다: `.codex/hooks/resolve-plan-dir.sh`, `.codex/hooks/session-start.sh`, `.codex/hooks/stop.sh`.
- `.codex/hooks.json`과 `Docs/LMContext/ParentHooks/hooks.json`은 upstream `.codex/hooks.json`에 기반한 LMProject 등록 설정이다. 프로젝트 경로·matcher·시간 제한·Windows 연결 명령과 상위 폴더 전달을 조정했다.
- `.codex/hooks/LMInvokePlanning.ps1`, `.codex/hooks/LMHookEntry.txt`, `Docs/LMContext/ParentHooks/LMParentHookEntry.txt`, `Docs/LMContext/LMVerifyContext.ps1`은 LMProject에서 작성한 연결/검증 파일이다. 외부 원본 49개와 구분하며 이 문서에서 프로젝트 작성물 전체에 새 라이선스를 부여하지 않는다.
- 파일별 mapping과 원문/포함본 SHA-256, Git blob ID는 [third-party-inventory.json](Docs/LMContext/third-party-inventory.json)에 기록했다. 기존 [upstream-lock.json](Docs/LMContext/upstream-lock.json)은 변경하지 않았다.

2026-11-03에 공식 태그가 위 커밋을 가리키는지 확인하고, 고정 커밋의 GitHub 트리 916개 항목과 원문 LICENSE를 조회했다. 해당 트리는 잘리지 않았으며 별도 upstream NOTICE 파일은 없었다. 아래 전문과 기존 두 라이선스 사본은 원문과 동일하다. 원본 소스에 있던 고지는 제거하지 않았다.

### MIT License — unmodified upstream text

```text
MIT License

Copyright (c) 2026 Ahmad Adi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
