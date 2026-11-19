# 작업 지시: ATRAC 인코더 지연 건너뛰기 (issue #51, repsp #53)

설계: [20261120-i051-atrac-encoder-delay](../design/20261120-i051-atrac-encoder-delay.md)
브랜치: 지금 브랜치를 그대로 쓴다. reDMP1 `i051-music-latency`, repsp `i52-audio-release`. 반환 기준 변경(#52)은 커밋돼 있고, repsp 작업 트리에는 리뷰어가 고친 `ATRAC_TRACE` Open 줄이 있다.

1. **repsp**: 설계 §2대로 `src/hle/atrac/hle_atrac.cpp`의 위치 모델을 바꾼다.
2. **probe**: 설계 §3의 1. 기존 ATRAC probe가 있으면 거기에, 없으면 `tools/atrac_probe`를 만들고 CTest에 등록한다.
3. **측정**: 설계 §3의 2~3. 입력 스크립트는 `%TEMP%\redmp1-build\i041-keysound-script.txt`를 쓴다. 곡 선택 미리듣기는 `%TEMP%\redmp1-build\i045-songselect-script.txt`를 쓴다.
4. **문서**: 설계 §4.

지킬 것:
* 생성 코드를 손으로 고치지 않는다.
* repsp에는 게임 정보와 psprecomp 의존을 넣지 않는다.
* PPSSPP 등 GPL 코드는 보지 않는다.
* 게임 데이터, 생성 코드, 로그 원본, 덤프, wav, 입력 스크립트, 세이브 데이터, `redmp1.ini` 실물은 커밋하지 않는다.
* 사용자 경로는 쓰지 않는다.
* probe는 창 없이 돌고, 실패해도 대화상자 없이 끝나며, 스레드 타이밍에 기대지 않는다.
* 커밋 제목 끝에 reDMP1은 `(#51)`, repsp는 `(#53)`을 붙인다.
* push·PR·merge·tag는 하지 않는다.

git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 보고에 적는다. GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 살핀다.

완료 기준: 설계 §3의 1~3(4는 사용자가 확인). 실기 동작을 확인할 수 없는 부분(예: `sceAtracGetSoundSample`의 값)은 보고에 질문으로 남긴다.

---

# Work order: skipping the ATRAC encoder delay (issue #51, repsp #53)

Design: [20261120-i051-atrac-encoder-delay](../design/20261120-i051-atrac-encoder-delay.md)

Branches: keep the current ones, reDMP1 `i051-music-latency` and repsp `i52-audio-release`. The release-threshold change (#52) is committed. The repsp working tree holds the reviewer's fix to the `ATRAC_TRACE` open line.

Steps:

1. **repsp**: change the position model in `src/hle/atrac/hle_atrac.cpp` as design §2 says.
2. **Probe**: design §3 item 1. Add it to the existing ATRAC probe if there is one; otherwise create `tools/atrac_probe` and register it with CTest.
3. **Measurement**: design §3 items 2-3. Use the input script at `%TEMP%\redmp1-build\i041-keysound-script.txt`, and `%TEMP%\redmp1-build\i045-songselect-script.txt` for the song-select preview.
4. **Documents**: design §4.

**Must hold**:
* Generated code is never edited by hand.
* No game information or psprecomp dependency in repsp.
* PPSSPP and other GPL code is not read.
* Never commit game data, generated code, raw logs, dumps, wav files, input scripts, save data, or an actual `redmp1.ini`.
* No user paths.
* Probes run without a window, exit without a dialog on failure, and do not depend on thread timing.
* Commit titles end in `(#51)` for reDMP1 and `(#53)` for repsp.
* No push, pull request, merge or tag.

If git commits or WSL are blocked, do not work around it. Leave the changes in the working tree and say so in the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`).

**Done when**: design §3 items 1-3 pass (item 4 is the user's check). Leave anything the hardware's behaviour cannot confirm (such as `sceAtracGetSoundSample`'s values) as a question in the report.
