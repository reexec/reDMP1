# 작업 지시: 스케줄링 두 가지와 음악 오프셋 (issue #51, #52, repsp #54, #55)

설계: [20261120-i051-scheduling-and-offset](../design/20261120-i051-scheduling-and-offset.md)
브랜치: 지금 브랜치를 그대로 쓴다(reDMP1 `i051-music-latency`, repsp `i52-audio-release`). repsp 작업 트리의 `src/gpu/gl_backend.cpp`에는 리뷰어가 만든 `LockGl` 시제품이 있다.

1. **repsp #54**(설계 §2.1): `LockGl`을 다듬고 probe를 넣는다.
2. **repsp #55**(설계 §2.3 앞부분): `SetStartOffsetMs`와 `atrac_probe` 항목.
3. **psprecomp 0006**(설계 §2.2): `third_party/psprecomp`에 직접 커밋하지 않는다. 기존 패치처럼 `patches/psprecomp/0006-loop-hook.patch`를 만들고, configure가 적용하도록 연결한다. 생성 코드를 다시 만든다.
4. **reDMP1 #52**(설계 §2.2): `PSPRECOMP_LOOP_HOOK`, `psp_loop_hook`.
5. **reDMP1 #51**(설계 §2.3 뒷부분): Audio 탭, `MusicOffsetMs`, settings probe.
6. **측정**(설계 §3의 2~4)과 **문서**(설계 §4).

지킬 것:
* 생성 코드를 손으로 고치지 않는다(리프터 패치와 재생성으로만 바꾼다).
* repsp에는 게임 정보와 psprecomp 의존을 넣지 않는다.
* PPSSPP 등 GPL 코드는 보지 않는다.
* 게임 데이터, 생성 코드, 로그 원본, 덤프, wav, 입력 스크립트, 세이브 데이터, `redmp1.ini` 실물은 커밋하지 않는다.
* 사용자 경로는 쓰지 않는다.
* probe는 창 없이 돌고, 실패해도 대화상자 없이 끝나며, 스레드 타이밍에 기대지 않는다.
* upstream(psprecomp)에는 아무것도 올리지 않는다.
* 커밋 제목 끝에 reDMP1은 `(#51)` 또는 `(#52)`, repsp는 `(#54)` 또는 `(#55)`를 붙인다.
* push·PR·merge·tag는 하지 않는다.

git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 단위별로 무엇을 바꿨는지 보고에 적는다. GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 살핀다.

완료 기준: 설계 §3의 1~4(5는 사용자가 확인).

---

# Work order: two scheduling fixes and a music offset (issues #51, #52, repsp #54, #55)

Design: [20261120-i051-scheduling-and-offset](../design/20261120-i051-scheduling-and-offset.md)

Branches: keep the current ones (reDMP1 `i051-music-latency`, repsp `i52-audio-release`). The repsp working tree holds the reviewer's `LockGl` prototype in `src/gpu/gl_backend.cpp`.

Steps:

1. **repsp #54** (design §2.1): finish `LockGl` and add its probe.
2. **repsp #55** (first part of design §2.3): `SetStartOffsetMs` and its `atrac_probe` items.
3. **psprecomp 0006** (design §2.2): do not commit into `third_party/psprecomp`. As with the existing patches, create `patches/psprecomp/0006-loop-hook.patch` and wire it so configure applies it. Then regenerate the generated code.
4. **reDMP1 #52** (design §2.2): `PSPRECOMP_LOOP_HOOK` and `psp_loop_hook`.
5. **reDMP1 #51** (second part of design §2.3): the Audio tab, `MusicOffsetMs`, and the settings probe.
6. **Measurement** (design §3 items 2-4) and **documents** (design §4).

**Must hold**:
* Generated code is never edited by hand; it changes only through the lifter patch and regeneration.
* No game information or psprecomp dependency in repsp.
* PPSSPP and other GPL code is not read.
* Never commit game data, generated code, raw logs, dumps, wav files, input scripts, save data, or an actual `redmp1.ini`.
* No user paths.
* Probes run without a window, exit without a dialog on failure, and do not depend on thread timing.
* Nothing goes upstream (psprecomp).
* Commit titles end in `(#51)` or `(#52)` for reDMP1, and `(#54)` or `(#55)` for repsp.
* No push, pull request, merge or tag.

If git commits or WSL are blocked, do not work around it. Leave the changes in the working tree and report what changed in each unit. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`).

**Done when**: design §3 items 1-4 pass (item 5 is the user's check).
