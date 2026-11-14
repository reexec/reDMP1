# 작업 지시: 소리 결함 (issue #26)

설계: [20261115-i026](../design/20261115-i026-sound-defects.md)
브랜치: `i026-sound-defects`(main의 v0.0.12 커밋에서)
함께: [repsp issue #26](https://github.com/reexec/repsp/issues/26). submodule `third_party/repsp`의 `i026-sound-defects` 브랜치에서 함께 개발한다. reDMP1 커밋에는 `third_party/repsp` 포인터를 넣지 않는다.

1. 소리의 목록(설계 §2의 1).
2. 반복되는 효과음과 느린 소리의 원인 분석(설계 §2의 2~4). 추측으로 고치지 않는다.
3. 원인이 있는 곳에서 수정(설계 §3).
4. 문서: `docs/analysis/audio.md`(이어 쓰기, 설계 §4), 진단 기능을 쓰는 방법은 `docs/guides/running-and-tracing.md`, 작업 로그 `docs/work-logs/20261115-i026-sound-defects.md`(결함별 증거와 원인, repsp 쪽 변경, 덤프를 남긴 위치, 사용자가 확인할 화면과 소리, 질문 포함).
5. 검증: 설계 §5.

개발 환경: 플러그인 빌드는 `scripts\build_win64.bat Release ffmpeg`(산출물 `build\win64-release-ffmpeg\Release\`) 또는 `%TEMP%\redmp1-build` 아래의 빌드 디렉터리를 쓴다. 고정한 FFmpeg은 `third_party\repsp\third_party\_ffmpeg`에 있다. 입력 스크립트는 `%TEMP%\redmp1-build\play-script.txt`를 쓸 수 있다.

지킬 것: 게임 데이터, 생성 코드, 추출한 소리와 오디오 덤프, 프레임 덤프, 입력 스크립트, 세이브 데이터, 실행 로그 원본, FFmpeg 파일은 커밋하지 않는다. 생성 코드를 손으로 고치지 않는다. 분석 문서에는 원본 게임 사실만. 문서와 로그에 사용자 계정 이름이 든 절대 경로를 쓰지 않는다. PPSSPP(GPL) 코드와 FFmpeg의 구현 소스는 보지 않는다. 작업 단위마다 커밋, 제목 끝에 `(#26)`. push·PR·merge·tag 금지. psprecomp 원격에는 아무것도 하지 않는다.

작업 환경에서 git 커밋이나 WSL 실행이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 작업 로그와 보고에 그 사실을 적는다(FFmpeg 명령줄 도구가 필요한 기준 디코딩을 못 하면 필요한 입력 파일의 위치와 명령을 적어 리뷰어에게 넘긴다). Linux에서도 빌드되도록 GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 미리 살핀다.

완료 기준: 설계 §5의 1~4(5는 사용자가 확인).

---

# Work order: sound defects (issue #26)

Design: [20261115-i026](../design/20261115-i026-sound-defects.md)
Branch: `i026-sound-defects` (from the v0.0.12 commit on main)
Alongside: [repsp issue #26](https://github.com/reexec/repsp/issues/26), developed on the `i026-sound-defects` branch of the `third_party/repsp` submodule; reDMP1 commits never include the `third_party/repsp` pointer.

(1) The inventory of sounds (design §2, step 1). (2) Analysis of the repeating effect and the slow sounds (design §2, steps 2-4); no fixing by guesswork. (3) Fixes where the causes lie (design §3). (4) Documents: `docs/analysis/audio.md` (continued, design §4), how to use the diagnostics in `docs/guides/running-and-tracing.md`, the work log `docs/work-logs/20261115-i026-sound-defects.md` (evidence and cause per defect, repsp changes, where dumps were kept, the screens and sounds for the user to check, questions). (5) Verify: design §5.

Development environment: the plugin build comes from `scripts\build_win64.bat Release ffmpeg` (output in `build\win64-release-ffmpeg\Release\`) or a build directory under `%TEMP%\redmp1-build`; the pinned FFmpeg is in `third_party\repsp\third_party\_ffmpeg`; the input script `%TEMP%\redmp1-build\play-script.txt` can be used.

Must hold: game data, generated code, extracted sounds and audio dumps, frame dumps, input scripts, save data, raw run logs and FFmpeg files are never committed; generated code is never edited by hand; analysis documents state facts about the original game only; no absolute path containing a user account name in documents or logs; PPSSPP (GPL) code and FFmpeg's implementation source are not read; commit per unit of work with titles ending in `(#26)`; no push, pull request, merge or tag; nothing is done on psprecomp's remote.

If git commits or WSL are blocked in the working environment, do not work around it: leave the changes in the working tree and say so in the work log and the report (if a reference decode needing the FFmpeg command-line tool cannot be done, give the input file's location and the command for the reviewer). Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`) so the Linux build passes.

Done when: design §5, items 1-4 (item 5 is the user's check).
