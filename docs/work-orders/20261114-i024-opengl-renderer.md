# 작업 지시: OpenGL 렌더러 (issue #24)

설계: [20261114-i024](../design/20261114-i024-opengl-renderer.md)
브랜치: `i024-opengl-renderer`(main의 v0.0.11 커밋에서)
선행: [repsp issue #24](https://github.com/reexec/repsp/issues/24)(OpenGL 렌더 백엔드). submodule `third_party/repsp`의 `i024-opengl-backend` 브랜치에서 함께 개발한다. reDMP1 커밋에는 `third_party/repsp` 포인터를 넣지 않는다.

1. 앱 옵션 `--renderer`, `--scale`, 정지 보고의 렌더러 통계(설계 §2).
2. 비교 도구와 화면별 비교, 차이의 원인 분석과 수정(설계 §3). 수정은 repsp submodule 브랜치에서, `gl_probe`에 경우를 더한다.
3. 성능 측정(설계 §4).
4. `scripts/build_win64.bat`(작업 트리에 이미 있는 변경)을 커밋하고 가이드에 적는다.
5. 문서: 설계 §5. 작업 로그 `docs/work-logs/20261114-i024-opengl-renderer.md`.
6. 검증: 설계 §6.

개발 환경: FFmpeg 플러그인 빌드가 `build\win64-release-ffmpeg`에 있고, 고정한 FFmpeg은 `third_party\repsp\third_party\_ffmpeg`에 있다(git이 무시). 입력 스크립트는 `%TEMP%\redmp1-build\play-script.txt`를 쓸 수 있다.

지킬 것: 기본 렌더러는 `sw`다. 게임 데이터, 생성 코드, 프레임 덤프와 차이 영상, 입력 스크립트, 세이브 데이터, 실행 로그 원본, FFmpeg 파일은 커밋하지 않는다. 분석 문서에는 원본 게임 사실만. 문서와 로그에 사용자 계정 이름이 든 절대 경로를 쓰지 않는다. PPSSPP(GPL) 코드는 보지 않는다. 작업 단위마다 커밋, 제목 끝에 `(#24)`. push·PR·merge·tag 금지. psprecomp 원격에는 아무것도 하지 않는다.

작업 환경에서 git 커밋, WSL 실행, GL 문맥 생성이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 작업 로그와 보고에 그 사실을 적는다(GL 문맥을 만들 수 없으면 비교는 리뷰어가 한다). Linux에서도 빌드되도록 GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 미리 살핀다.

완료 기준: 설계 §6의 1~6.

---

# Work order: OpenGL renderer (issue #24)

Design: [20261114-i024](../design/20261114-i024-opengl-renderer.md)
Branch: `i024-opengl-renderer` (from the v0.0.11 commit on main)
Depends on: [repsp issue #24](https://github.com/reexec/repsp/issues/24) (the OpenGL render backend), developed alongside on the `i024-opengl-backend` branch of the `third_party/repsp` submodule; reDMP1 commits never include the `third_party/repsp` pointer.

(1) App options `--renderer` and `--scale`, renderer statistics in the stop report (design §2). (2) The comparison tool, comparison per screen, and analysis and fixing of differences (design §3); fixes go on the repsp submodule branch with cases added to `gl_probe`. (3) Performance measurement (design §4). (4) Commit `scripts/build_win64.bat` (the change already in the working tree) and describe it in the guide. (5) Documents: design §5, and the work log `docs/work-logs/20261114-i024-opengl-renderer.md`. (6) Verify: design §6.

Development environment: an FFmpeg plugin build exists in `build\win64-release-ffmpeg`, and the pinned FFmpeg is in `third_party\repsp\third_party\_ffmpeg` (ignored by git). The input script `%TEMP%\redmp1-build\play-script.txt` can be used.

Must hold: the default renderer is `sw`; game data, generated code, frame dumps and difference images, input scripts, save data, raw run logs and FFmpeg files are never committed; analysis documents state facts about the original game only; no absolute path containing a user account name in documents or logs; PPSSPP (GPL) code is not read; commit per unit of work with titles ending in `(#24)`; no push, pull request, merge or tag; nothing is done on psprecomp's remote.

If git commits, WSL, or GL context creation are blocked in the working environment, do not work around it: leave the changes in the working tree and say so in the work log and the report (if no GL context can be created, the reviewer does the comparison). Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`) so the Linux build passes.

Done when: design §6, items 1-6.
