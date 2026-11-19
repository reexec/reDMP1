# 작업 지시: 배경음이 노트보다 늦음 (issue #51, repsp #52)

설계: [20261119-i051](../design/20261119-i051-music-latency.md)
브랜치: reDMP1 `i051-music-latency`(main의 v0.0.25 커밋에서), repsp는 submodule `third_party/repsp`의 `i52-audio-release`(main v0.0.24 커밋에서)

1. **repsp**: `src/hle/audio/hle_audio.cpp`의 blocking 출력 반환 조건을 설계 §2대로 바꾼다(대기열 ≤ min(`kClockQuantumFrames`, `sample_count`), drain은 그대로). 이유를 코드 주석으로 남긴다.
2. **probe**: `audio_probe`에 설계 §3의 1을 넣는다.
3. **측정**: 설계 §3의 2~3. 대기열 평균은 임시 계측으로 재고, 계측 코드는 커밋하지 않는다. 입력 스크립트는 리뷰어가 `%TEMP%\redmp1-build\i041-keysound-script.txt`에 둔 것을 쓴다.
4. **문서**: 설계 §4.

지킬 것:
* 생성 코드를 손으로 고치지 않는다.
* repsp에는 게임 정보와 psprecomp 의존을 넣지 않는다.
* PPSSPP 등 GPL 코드는 보지 않는다.
* 게임 데이터, 생성 코드, 로그 원본, 덤프, wav, 입력 스크립트, 세이브 데이터, `redmp1.ini` 실물은 커밋하지 않는다.
* 사용자 경로는 쓰지 않는다.
* probe는 창 없이 돌고, 실패해도 대화상자 없이 끝나며, 스레드 타이밍에 기대지 않는다.
* 커밋 제목 끝에 reDMP1은 `(#51)`, repsp는 `(#52)`를 붙인다.
* push·PR·merge·tag는 하지 않는다.

git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 보고에 적는다. GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 살핀다.

완료 기준: 설계 §3의 1~3(4는 사용자가 확인).

---

# Work order: background music lags the notes (issue #51, repsp #52)

Design: [20261119-i051](../design/20261119-i051-music-latency.md)

Branches:
* reDMP1: `i051-music-latency`, from the v0.0.25 commit on main.
* repsp: `i52-audio-release` in the submodule `third_party/repsp`, from the v0.0.24 commit on main.

Steps:

1. **repsp**: change the blocking output's release condition in `src/hle/audio/hle_audio.cpp` as design §2 says: the queue at most min(`kClockQuantumFrames`, `sample_count`); a drain is unchanged. Leave the reason in a code comment.
2. **Probe**: add design §3 item 1 to `audio_probe`.
3. **Measurement**: design §3 items 2-3. Measure the queue mean with temporary instrumentation, and do not commit it. Use the input script the reviewer left at `%TEMP%\redmp1-build\i041-keysound-script.txt`.
4. **Documents**: design §4.

**Must hold**:
* Generated code is never edited by hand.
* No game information or psprecomp dependency in repsp.
* PPSSPP and other GPL code is not read.
* Never commit game data, generated code, raw logs, dumps, wav files, input scripts, save data, or an actual `redmp1.ini`.
* No user paths.
* Probes run without a window, exit without a dialog on failure, and do not depend on thread timing.
* Commit titles end in `(#51)` for reDMP1 and `(#52)` for repsp.
* No push, pull request, merge or tag.

If git commits or WSL are blocked, do not work around it. Leave the changes in the working tree and say so in the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`).

**Done when**: design §3 items 1-3 pass (item 4 is the user's check).
