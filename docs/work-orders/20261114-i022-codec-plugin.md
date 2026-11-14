# 작업 지시: 소리와 영상 (issue #22)

설계: [20261114-i022](../design/20261114-i022-codec-plugin.md)
브랜치: `i022-codec-plugin`(main의 v0.0.10 커밋에서)
선행: [repsp issue #21](https://github.com/reexec/repsp/issues/21)(코덱 플러그인). submodule `third_party/repsp`의 `i021-codec-plugin` 브랜치에서 함께 개발한다. reDMP1 커밋에는 `third_party/repsp` 포인터를 넣지 않는다.

1. 빌드 옵션 `REDMP1_CODEC_FFMPEG`와 출력 디렉터리로의 복사(설계 §2).
2. 앱: 플러그인 찾기·불러오기, `--codec-plugin`, `--no-codec-plugin`, 보고(설계 §2). CTest `app_boot` 유지.
3. 검증 수단: 오디오 덤프(없으면 repsp에 더한다), 프레임 덤프(설계 §4).
4. 실행과 수정: 실제 디코더로 드러나는 결함(인코더 지연, 끝 처리, 루프, 재정렬, 색 변환, 버퍼 계산)을 원인이 있는 곳에서 고친다. 설계 판단이 필요하면 고치지 말고 질문으로 올린다.
5. 문서: `docs/guides/`(받는 방법, 빌드, 옵션), `THIRD_PARTY_NOTICES.md` 또는 `CREDITS.md`, 릴리스 절차의 함께 배포 조건, `docs/analysis/audio.md`·`video.md`(이어 쓰기), `ARCHITECTURE.md`, 작업 로그 `docs/work-logs/20261114-i022-codec-plugin.md`(측정 수치, 덤프를 남긴 위치, 사용자가 창 모드로 확인하는 방법, 질문 포함).
6. 검증: 설계 §6.

개발용 FFmpeg: Win64 LGPL 공유 빌드(n8.1)가 `%TEMP%\redmp1-build\deps\ffmpeg\` 아래에 풀려 있다.

지킬 것: FFmpeg의 구현 소스와 PPSSPP 코드를 읽거나 옮기지 않는다. FFmpeg 바이너리·헤더, 게임 데이터, 생성 코드, 프레임·오디오 덤프, 입력 스크립트, 세이브 데이터, 실행 로그 원본은 커밋하지 않는다. 기본 빌드와 CI에 FFmpeg 의존을 넣지 않는다. 분석 문서에는 원본 게임 사실만. 문서와 로그에 사용자 계정 이름이 든 절대 경로를 쓰지 않는다. 작업 단위마다 커밋, 제목 끝에 `(#22)`. push·PR·merge·tag 금지. psprecomp 원격에는 아무것도 하지 않는다.

작업 환경에서 git 커밋, WSL 실행, 네트워크가 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 작업 로그와 보고에 그 사실을 적는다. Linux에서도 빌드되도록 GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 미리 살핀다.

완료 기준: 설계 §6의 1~5.

---

# Work order: sound and video (issue #22)

Design: [20261114-i022](../design/20261114-i022-codec-plugin.md)
Branch: `i022-codec-plugin` (from the v0.0.10 commit on main)
Depends on: [repsp issue #21](https://github.com/reexec/repsp/issues/21) (the codec plugin), developed alongside on the `i021-codec-plugin` branch of the `third_party/repsp` submodule; reDMP1 commits never include the `third_party/repsp` pointer.

(1) The build option `REDMP1_CODEC_FFMPEG` and copying into the output directory (design §2). (2) App: finding and loading the plugin, `--codec-plugin`, `--no-codec-plugin`, the report (design §2); CTest `app_boot` kept. (3) Means of verification: the audio dump (added to repsp if missing) and frame dumps (design §4). (4) Run and fix: defects that a real decoder exposes (encoder delay, end handling, loops, reordering, colour conversion, buffer accounting) are fixed where their cause lies; if a design decision is needed, raise a question instead. (5) Documents: `docs/guides/` (getting FFmpeg, building, options), `THIRD_PARTY_NOTICES.md` or `CREDITS.md`, the conditions for shipping alongside in the release procedure, `docs/analysis/audio.md` and `video.md` (continued), `ARCHITECTURE.md`, the work log `docs/work-logs/20261114-i022-codec-plugin.md` (measurements, where dumps were kept, how the user checks in windowed mode, questions). (6) Verify: design §6.

FFmpeg for development: a Win64 LGPL shared build (n8.1) is unpacked under `%TEMP%\redmp1-build\deps\ffmpeg\`.

Must hold: FFmpeg's implementation source and PPSSPP code are not read or carried over; no FFmpeg binary or header, game data, generated code, frame or audio dump, input script, save data or raw run log is committed; no FFmpeg dependency in the default build or CI; analysis documents state facts about the original game only; no absolute path containing a user account name in documents or logs; commit per unit of work with titles ending in `(#22)`; no push, pull request, merge or tag; nothing is done on psprecomp's remote.

If git commits, WSL or the network are blocked in the working environment, do not work around it: leave the changes in the working tree and say so in the work log and the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`) so the Linux build passes.

Done when: design §6, items 1-5.
