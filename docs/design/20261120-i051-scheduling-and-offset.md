# 설계: 음악 시작을 늦추는 스케줄링 두 가지와 음악 오프셋 (issue #51, #52, repsp #54, #55)

앞 설계: [20261119-i051](20261119-i051-music-latency.md), [20261120-i051-atrac-encoder-delay](20261120-i051-atrac-encoder-delay.md).

## 1. 진단

* 인코더 지연을 건너뛴 뒤에도 사용자는 음악이 늦다고 했다. 곡 앞부분을 건너뛰는 임시 값으로 귀로 맞춰 보니 **150 ms**였다(2026-11-20).
* 임시 계측(커밋하지 않음)으로 확인한 사실:
  * 곡 시작 자체는 빠르다. 음악 채널의 첫 블록이 장치로 가기까지 5~30 ms다.
  * `sceKernelDelayThread(100 ms)`는 보통 100~102 ms가 걸린다.
  * **우선순위가 높은 스레드가 늦게 실행된다.** 다음은 늦게 실행된 기록이다(타이머가 끝난 뒤 실제로 실행될 때까지). 매번 바로 앞 구간에서 `user_main`(0x20)이 그만큼 CPU를 잡고 있었다.

    | 스레드 | 우선순위 | 늦음 |
    |---|---|---|
    | `sound_poll_thread` | 0x12 | 94~1004 ms |
    | `ScorePlayerThread` | 0x22 | 160 ms |
* **원인 1: GL 잠금 대기** (repsp #54)
  * 곡 시작 직전에 `sceDisplaySetFrameBuf`가 107~165 ms 걸렸다.
  * 그 안의 `gpu::HasGlTargetForAddress()`가 `g_gl.mutex`를 117 ms 기다렸다. 보통 프레임에서도 15~18 ms를 기다린다.
  * UI 스레드는 flush(`UiExecuteFlush`) 내내 이 잠금을 잡는다. 그동안 게스트 스레드는 게스트 CPU를 쥔 채 호스트 잠금을 기다리고, 다른 게스트 스레드는 아무도 돌지 못한다.
* **원인 2: 양보 없는 계산** (#52)
  * 재컴파일 코드는 HLE 호출에서만 게스트 CPU를 넘긴다. `user_main`이 HLE 호출 없이 100~1000 ms를 계산하면, 타이머가 끝난 높은 우선순위 스레드가 그동안 돌지 못한다.
  * 실기에서는 바로 선점된다.
* 이 게임의 음악 구조(HLE 추적에 스레드 이름을 붙여 확인):
  * `stream_sound_poll_thread`: 100 ms마다 요청을 보고 곡을 연다. 블록마다 `DecodeData` → `SetChannelDataLen` → `OutputPannedBlocking` → `DelayThread(10 ms)`를 반복한다.
  * `ScorePlayerThread`: 1 ms마다 `sceKernelGetSystemTimeWide`를 읽는다(채보 시계).
  * `sound_poll_thread`: SAS(키음)를 맡는다.
* **두 원인을 모두 고친 시험 빌드**에서는 30 ms 넘게 늦은 실행이 없었다(2회).
  * 사용자 청취(2026-11-20): 좋아졌지만 음악을 약 100 ms 더 앞당겨야 자연스럽다.
  * 알려진 호스트 경로는 약 45~70 ms다(채널 대기열, 중간 버퍼 21 ms, 장치 10 ms, OS). 나머지는 아직 원인을 찾지 못했다.
* **사용자 결정(2026-11-20)**:
  * 음악 오프셋 설정을 넣고 머지한다.
  * 남은 원인은 다음 작업으로 분석한다.

## 2. 변경

### 2.1 repsp #54: GL 잠금을 기다리는 동안 게스트 CPU를 놓는다
* `gl_backend.cpp`의 모든 `g_gl.mutex` 잠금을 `LockGl()`로 바꾼다.
  * `try_lock`이 실패하면 `ScopedHostWait` 안에서 잠금이 풀리기를 기다린다. 기다린 뒤에는 잠금을 놓고 다시 시도한다. 그래서 게스트 CPU를 되찾을 때까지 잠금을 쥐고 있지 않는다.
  * 게스트가 아닌 스레드(UI 스레드)와 이미 호스트 대기 중인 스레드에서는 `ScopedHostWait`가 아무 일도 하지 않으므로, 그냥 잠금이 된다.
* 리뷰어의 시제품이 작업 트리에 있다(`LockGl`, 26곳 교체).
* `ScopedHostWait` 계약(게스트 메모리·CPU 상태·커널 객체를 건드리지 않음)은 잠금을 기다리는 동안 지켜진다.

### 2.2 #52: 루프 되돌이에서 선점
* **psprecomp 로컬 패치 `0006`**: `emit.c`가 내는 헤더에서, `PSPRECOMP_LOOP_HOOK`이 정의되면 `PSP_LOOP(a)`를 `psp_loop_hook(a)`로 만들고 그 선언도 낸다. 정의되지 않으면 지금과 같은 출력이다.
  * `PSP_LOOP`는 이미 모든 뒤로 가는 분기에 나온다(이 게임 916곳).
  * upstream 제출은 사용자가 허락할 때 한다. 준비만 한다.
* **reDMP1**:
  * 생성 코드 라이브러리에 `PSPRECOMP_LOOP_HOOK`을 정의한다.
  * `src/recomp_abi/`에 `psp_loop_hook`을 둔다. 1024번에 한 번 `repsp_recomp_poll()`을 부른다(인터럽트 전달과 선점 양보).
  * 게스트 스레드는 한 번에 하나만 돌므로 카운터는 전역 하나로 충분하다.
* 생성 코드를 다시 만든다(`scripts/recomp/regenerate.py`). 생성 코드는 커밋하지 않는다.
* 비용: 되돌이마다 증가 하나와 비교 하나. 표시 간격과 미발견 주소 dispatch로 확인한다.

### 2.3 repsp #55와 #51: 음악 오프셋
* **repsp**: `repsp::hle::atrac::SetStartOffsetMs(uint32_t)`(기본 0)를 넣는다.
  * 트랙을 열 때 `raw_pos = delay + offset × sample_rate / 1000`(끝을 넘지 않게)로 둔다.
  * 루프와 Reset 계열에는 적용하지 않는다.
  * 게임 정보는 넣지 않는다.
* **reDMP1**:
  * 메뉴에 **Audio** 탭을 넣고, 그 안에 "Music offset (ms)" 슬라이더(0~300, 5 단위)를 둔다.
  * 설명을 한 줄 단다: 음악을 이만큼 앞당긴다. 다음 곡부터 적용된다.
  * `redmp1.ini`의 `MusicOffsetMs`(기본 0)에 저장한다.
  * 시작 로그에 값을 적는다.

## 3. 검증

1. **probe**:
   * `atrac_probe`: 오프셋 0과 1000 ms에서 첫 샘플 위치. 루프와 Reset에는 오프셋이 들지 않는다.
   * `gl_probe` 또는 새 probe: 잠금을 다른 스레드가 쥔 동안, 게스트 스레드의 잠금 대기가 호스트 대기로 바뀌어 다른 게스트 스레드가 돈다. 결정적으로 만든다. 스레드 타이밍에 기대지 않고, 시험용 훅이나 동기화로 순서를 정한다.
   * reDMP1 `redmp1_settings_probe`: `MusicOffsetMs` 왕복과 잘못된 값.
2. **게임**(창 모드, 같은 입력 스크립트 3회):
   * 미발견 주소 dispatch 0, 크래시 0
   * 곡 중간 끊김 0
   * 표시 간격이 이전 범위(약 19 ms)
   * 임시 계측으로 30 ms 넘게 늦은 실행이 없다. 계측은 커밋하지 않는다.
3. 소프트웨어 렌더러, `--headless`, 게임 데이터 없는 구성이 빌드되고 동작한다.
4. Win64 Debug/Release(Visual Studio 생성기 포함)와 Linux x64 빌드·CTest(repsp, reDMP1). GCC 경고 없음.
5. 사용자가 오프셋 값을 맞춰 보고 확인한다(사용자 확인).

## 4. 기록

* repsp `ARCHITECTURE.md`: GL 잠금 대기, `SetStartOffsetMs`.
* reDMP1:
  * `patches/psprecomp/README.md`: 0006을 적고, upstream 상태를 "준비만 함"으로 둔다.
  * `README.md`: Audio 탭과 `MusicOffsetMs`.
  * 작업 로그 `docs/work-logs/20261119-i051-music-latency.md`.

---

# Design: two scheduling defects that delay the music start, and a music offset (issues #51, #52, repsp #54, #55)

Previous designs: [20261119-i051](20261119-i051-music-latency.md) and [20261120-i051-atrac-encoder-delay](20261120-i051-atrac-encoder-delay.md).

## 1. Diagnosis

* With the encoder delay skipped, the user still found the music late. Matching by ear with a temporary skip value gave **150 ms** (2026-11-20).
* **Temporary instrumentation** (not committed) showed:
  * The music start itself is quick: the music channel's first block reaches the device in 5-30 ms.
  * `sceKernelDelayThread(100 ms)` normally takes 100-102 ms.
  * **Higher-priority threads run late.** These are the resume delays observed, measured from timer expiry to actually running. Each time, `user_main` (0x20) held the CPU for that long just before.

    | thread | priority | late by |
    |---|---|---|
    | `sound_poll_thread` | 0x12 | 94-1004 ms |
    | `ScorePlayerThread` | 0x22 | 160 ms |
* **Cause 1: waiting for the GL lock** (repsp #54).
  * Right before the music started, `sceDisplaySetFrameBuf` took 107-165 ms.
  * Inside it, `gpu::HasGlTargetForAddress()` waited 117 ms for `g_gl.mutex`. On ordinary frames it waits 15-18 ms.
  * The UI thread holds that lock for a whole flush (`UiExecuteFlush`). Meanwhile the guest thread waits on the host lock while holding the guest CPU, so no other guest thread runs.
* **Cause 2: computation that never yields** (#52).
  * Recompiled code hands over the guest CPU only at HLE calls. When `user_main` computes for 100-1000 ms without one, a higher-priority thread whose timer has expired cannot run.
  * The PSP preempts at once.
* **This title's music** (from the HLE trace with thread names):
  * `stream_sound_poll_thread` checks for requests every 100 ms and opens the track. Per block it runs `DecodeData` → `SetChannelDataLen` → `OutputPannedBlocking` → `DelayThread(10 ms)`.
  * `ScorePlayerThread` reads `sceKernelGetSystemTimeWide` every 1 ms; this is the chart clock.
  * `sound_poll_thread` handles SAS (key sounds).
* **A test build with both causes fixed** had no resume later than 30 ms (two runs).
  * User's listening (2026-11-20): better, but the music needs about 100 ms more to feel right.
  * The known host path is about 45-70 ms (channel queue, mixing buffer 21 ms, device 10 ms, OS). The rest is not explained yet.
* **User decision (2026-11-20)**:
  * Add a music offset setting and merge.
  * Analyse the remaining cause as the next task.

## 2. Changes

### 2.1 repsp #54: release the guest CPU while waiting for the GL lock
* Every `g_gl.mutex` lock in `gl_backend.cpp` becomes `LockGl()`.
  * When `try_lock` fails, it waits for the lock inside a `ScopedHostWait`. After the wait it releases the lock and tries again, so it never holds the lock while waiting to get the guest CPU back.
  * On a non-guest thread (the UI thread), or one already in a host wait, `ScopedHostWait` does nothing, so this is a plain lock.
* The reviewer's prototype is in the working tree (`LockGl`, 26 sites replaced).
* The `ScopedHostWait` contract (no guest memory, CPU state or kernel object access) holds while waiting for the lock.

### 2.2 #52: preemption at loop back-edges
* **Local psprecomp patch `0006`**: in the header `emit.c` writes, when `PSPRECOMP_LOOP_HOOK` is defined, `PSP_LOOP(a)` becomes `psp_loop_hook(a)` and the hook is declared. Without the define, the output is as today.
  * `PSP_LOOP` is already emitted on every backward branch (916 sites in this title).
  * Upstream submission waits for the user's permission; it is only prepared.
* **reDMP1**:
  * defines `PSPRECOMP_LOOP_HOOK` for the generated-code library;
  * puts `psp_loop_hook` in `src/recomp_abi/`. It calls `repsp_recomp_poll()` (interrupt delivery and preemption yield) once every 1024 calls;
  * one global counter is enough, since only one guest thread runs at a time.
* The generated code is regenerated (`scripts/recomp/regenerate.py`). Generated code is not committed.
* Cost: one increment and one compare per back-edge, checked against the present interval and unresolved-address dispatches.

### 2.3 repsp #55 and #51: music offset
* **repsp**: add `repsp::hle::atrac::SetStartOffsetMs(uint32_t)` (default 0).
  * When a track is opened, `raw_pos = delay + offset × sample_rate / 1000`, capped at the end.
  * Loops and the reset functions are not affected.
  * No game information goes in.
* **reDMP1**:
  * an **Audio** tab in the menu, with a "Music offset (ms)" slider (0-300 in steps of 5);
  * a one-line note under it: the music starts this much earlier, from the next track;
  * saved as `MusicOffsetMs` in `redmp1.ini` (default 0);
  * the startup log names the value.

## 3. Verification

1. **Probes**:
   * `atrac_probe`: the first sample position at offsets 0 and 1000 ms. Loops and resets carry no offset.
   * `gl_probe` or a new probe: while another thread holds the lock, a guest thread's lock wait becomes a host wait and another guest thread runs. Make it deterministic: no reliance on thread timing, with the order set by test hooks or synchronisation.
   * reDMP1 `redmp1_settings_probe`: `MusicOffsetMs` round trip and bad values.
2. **Game** (windowed, the same input script, three runs):
   * no unresolved-address dispatches, no crashes;
   * no mid-song underruns;
   * present interval in the earlier range (about 19 ms);
   * no resume later than 30 ms, checked with temporary instrumentation that is not committed.
3. The software renderer, `--headless`, and a configuration without game data build and run.
4. Win64 Debug/Release (including the Visual Studio generator) and Linux x64, build and CTest, for repsp and reDMP1. No GCC warnings.
5. The user tunes the offset and confirms (the user's check).

## 4. Records

* repsp `ARCHITECTURE.md`: the GL lock wait and `SetStartOffsetMs`.
* reDMP1:
  * `patches/psprecomp/README.md`: patch 0006, with its upstream status as "prepared only";
  * `README.md`: the Audio tab and `MusicOffsetMs`;
  * work log: `docs/work-logs/20261119-i051-music-latency.md`.
