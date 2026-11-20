# 작업 지시: DelayThread 최소 시간 (issue #54, repsp #57)

설계: [20261121-i054-delaythread-minimum](../design/20261121-i054-delaythread-minimum.md)
브랜치: reDMP1 `i054-music-latency-2`(main의 v0.0.26 커밋에서), repsp는 submodule `third_party/repsp`의 `i57-music-latency-2`(main v0.0.25 커밋에서). 둘 다 만들어 두었다.

1. **repsp**: 설계 §2. `src/hle/kernel/threadman_thread.cpp`의 `HleDelayThread`, `HleDelayThreadCB`.
2. **probe**: 설계 §4의 1. 기존 커널·스케줄러 probe가 있으면 거기에 넣는다.
3. **reDMP1**: 설계 §3.
4. **측정**: 설계 §4의 2~3. 입력 스크립트는 `%TEMP%\redmp1-build\i041-keysound-script.txt`를 쓴다. 채보 t0는 `ScorePlayerThread` reset(`0x08861704`) 안의 시계 읽기다. 복귀 주소 `0x0886172C`로 알아본다. 계측은 커밋하지 않는다.

지킬 것:
* 생성 코드를 손으로 고치지 않는다.
* repsp에는 게임 정보와 psprecomp 의존을 넣지 않는다.
* PPSSPP 등 GPL 코드는 보지 않는다.
* 게임 데이터, 생성 코드, 로그 원본, 덤프, wav, 입력 스크립트, 세이브 데이터, `redmp1.ini` 실물은 커밋하지 않는다.
* 사용자 경로는 쓰지 않는다.
* probe는 창 없이 돌고, 실패해도 대화상자 없이 끝나며, 스레드 타이밍에 기대지 않는다.
* 커밋 제목 끝에 reDMP1은 `(#54)`, repsp는 `(#57)`을 붙인다.
* push·PR·merge·tag는 하지 않는다.

git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 보고에 적는다. GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 살핀다.

완료 기준: 설계 §4의 1~3.

---

# Work order: a DelayThread minimum (issue #54, repsp #57)

Design: [20261121-i054-delaythread-minimum](../design/20261121-i054-delaythread-minimum.md)

Branches (both already created):
* reDMP1: `i054-music-latency-2`, from the v0.0.26 commit on main.
* repsp: `i57-music-latency-2` in the submodule `third_party/repsp`, from the v0.0.25 commit on main.

Steps:

1. **repsp**: design §2, in `HleDelayThread` and `HleDelayThreadCB` (`src/hle/kernel/threadman_thread.cpp`).
2. **Probe**: design §4 item 1. Add it to an existing kernel or scheduler probe if there is one.
3. **reDMP1**: design §3.
4. **Measurement**: design §4 items 2-3. Use the input script at `%TEMP%\redmp1-build\i041-keysound-script.txt`. Chart t0 is the clock read inside the `ScorePlayerThread` reset (`0x08861704`), recognisable by its return address `0x0886172C`. Do not commit the instrumentation.

**Must hold**:
* Generated code is never edited by hand.
* No game information or psprecomp dependency in repsp.
* PPSSPP and other GPL code is not read.
* Never commit game data, generated code, raw logs, dumps, wav files, input scripts, save data, or an actual `redmp1.ini`.
* No user paths.
* Probes run without a window, exit without a dialog on failure, and do not depend on thread timing.
* Commit titles end in `(#54)` for reDMP1 and `(#57)` for repsp.
* No push, pull request, merge or tag.

If git commits or WSL are blocked, do not work around it. Leave the changes in the working tree and say so in the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`).

**Done when**: design §4 items 1-3 pass.
