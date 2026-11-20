# 설계: 채보가 음악보다 먼저 시작함 — DelayThread 최소 시간 (issue #54, repsp #57)

앞 설계: [20261120-i051-scheduling-and-offset](20261120-i051-scheduling-and-offset.md).

## 1. 진단

v0.0.26 이후에도 음악을 약 100 ms 앞당겨야 자연스러웠다. 임시 계측(커밋하지 않음)으로 게임 코드를 따라갔다.

* **채보 시계**:
  * `ScorePlayerThread`는 이벤트 시퀀서다(`0x088615B0`). 1 ms마다 시계(ms)를 읽고, 남은 시간을 줄이다가 0이 되면 다음 이벤트를 처리한다.
  * 시작점 t0는 재생 함수(`0x0885F4E8`)가 Start(`0x088617E8`) → reset(`0x08861704`)을 부르는 순간이다.
* **음악**:
  * `stream_sound_poll_thread`는 100 ms마다 요청을 확인한다.
  * 곡을 열면 상태 플래그(event flag 0xF의 bit 4)를 세운다.
  * 재생 요청을 받으면 첫 블록을 낸다.
* **두 스레드를 맞추는 대기 루프**: 재생을 요청한 메인 스레드는 다음 루프로 음악 스레드가 요청을 처리하기를 기다린 뒤 채보를 시작한다. 같은 모양의 루프가 `0x0885244C`, `0x088526C0`, `0x088527E8` 세 곳에 있다.

  ```c
  for (i = 0; i < 1000; i++) {
      if (상태 바이트 0x0896E9CD) break;   /* 음악 스레드가 처리했는가 */
      sceKernelDelayThread(10);          /* 10 µs */
  }
  ```

* **repsp에서**: 10 µs 대기가 실제로 약 29 µs라 1000번이 29 ms다. 음악 스레드의 100 ms 확인보다 먼저 끝나서, 채보가 음악보다 60~70 ms 먼저 시작했다(두 번 측정: 61.8, 67.3 ms).
* **실기에서**: 대기 한 번마다 스레드 전환과 커널 타이머 비용이 들어 10 µs보다 훨씬 길다. 그래서 1000번이 음악 스레드의 확인을 덮고, 채보와 음악이 함께 시작한다. PPSSPP에서 맞는 것도 같은 이유로 본다.
* **임시 최소값 시험**(같은 입력 스크립트):

  | 최소값 | 채보 t0 → 음악 첫 블록 | 표시 간격 | 음악 끊김 |
  |---|---|---|---|
  | 없음 | +59 ms | 19.1 ms | 꼬리만 |
  | 100 µs | +0.2 ms | 19.0 ms | 꼬리만 |
  | 200 µs | +0.3 ms | 19.0 ms | 꼬리만 |

* **사용자 청취(2026-11-21)**: 200 µs, 음악 오프셋 0에서 타이밍이 잘 맞는다.
* 실기의 정확한 최소값은 문서로 확인하지 못했다(**미확인**). 200 µs는 사용자 확인으로 정한 값이다.

## 2. 변경 (repsp)

* `sceKernelDelayThread`와 `sceKernelDelayThreadCB`가 요청 시간을 최소 **200 µs**로 올린다.
  * 0을 포함해 그보다 짧은 요청에 적용한다. 더 긴 요청은 그대로다.
  * 상수 이름과 주석에 이유를 남긴다: 실기에서 짧은 대기는 스레드 전환과 타이머 비용 때문에 훨씬 길고, 게임의 대기 루프가 이를 전제로 한다.
* 다른 대기(`WaitSema`, `WaitEventFlag`의 timeout 등)는 바꾸지 않는다.
* 게임 정보는 넣지 않는다(설명은 일반적인 대기 루프로 쓴다).

## 3. reDMP1

* repsp를 올리고, README의 음악 오프셋 설명을 고친다. 이제 기본 0에서 맞고, 출력 장치 차이를 맞추는 용도다.
* 작업 로그 `docs/work-logs/20261121-i054-delaythread-minimum.md`.

## 4. 검증

1. **probe**(창 없이, 결정적):
   * `DelayThread(10)`과 `DelayThread(0)`이 적어도 200 µs 대기로 등록된다(마감 시각으로 검사).
   * `DelayThread(5000)`은 그대로다.
   * CB 형도 같다.
   * 실제 경과 시간에 기대지 말고, 스케줄러의 마감 시각이나 시험용 훅으로 검사한다.
2. **게임**(창 모드, 같은 입력 스크립트 3회):
   * 미발견 주소 dispatch 0, 크래시 0, 곡 중간 끊김 0
   * 표시 간격이 이전 범위(약 19 ms)
   * 임시 계측으로 채보 t0와 음악 첫 블록의 차이가 5 ms 이하. 계측은 커밋하지 않는다.
3. Win64 Debug/Release(Visual Studio 생성기 포함)와 Linux x64 빌드·CTest(repsp, reDMP1).
4. 사용자가 음악 오프셋 0에서 확인한다(확인됨).

---

# Design: the chart starts before the music — a DelayThread minimum (issue #54, repsp #57)

Previous design: [20261120-i051-scheduling-and-offset](20261120-i051-scheduling-and-offset.md).

## 1. Diagnosis

After v0.0.26 the music still needed about 100 ms of offset to feel right. Temporary instrumentation (not committed) followed the game code:

* **Chart clock**:
  * `ScorePlayerThread` is an event sequencer (`0x088615B0`). Every 1 ms it reads the clock (ms), counts down the time to the next event, and handles events as they come due.
  * Its start, t0, is the moment the play function (`0x0885F4E8`) calls Start (`0x088617E8`) → reset (`0x08861704`).
* **Music**:
  * `stream_sound_poll_thread` checks for requests every 100 ms.
  * It sets a status flag (bit 4 of event flag 0xF) when a track is open.
  * It outputs the first block once it has taken the play request.
* **The wait loop between them**: having requested playback, the main thread waits for the music thread to take the request before starting the chart. The same loop appears three times (`0x0885244C`, `0x088526C0`, `0x088527E8`):

  ```c
  for (i = 0; i < 1000; i++) {
      if (status byte at 0x0896E9CD) break;   /* has the music thread acted? */
      sceKernelDelayThread(10);               /* 10 µs */
  }
  ```

* **In repsp**: a 10 µs delay actually takes about 29 µs, so 1000 of them last 29 ms. The loop ends before the music thread's 100 ms check, and the chart started 60-70 ms before the music (two measurements: 61.8 and 67.3 ms).
* **On the PSP**: each delay costs far more than 10 µs (thread switch and kernel timer), so 1000 of them span the music thread's check, and chart and music start together. PPSSPP being in sync is taken to have the same reason.
* **Trial minimums** (same input script):

  | minimum | chart t0 → first music block | present interval | music underruns |
  |---|---|---|---|
  | none | +59 ms | 19.1 ms | tails only |
  | 100 µs | +0.2 ms | 19.0 ms | tails only |
  | 200 µs | +0.3 ms | 19.0 ms | tails only |

* **User's listening (2026-11-21)**: with 200 µs and a music offset of 0, the timing is right.
* The hardware's exact minimum is not confirmed by documentation (**unconfirmed**); 200 µs is the value the user confirmed.

## 2. Change (repsp)

* `sceKernelDelayThread` and `sceKernelDelayThreadCB` raise the requested time to at least **200 µs**.
  * This applies to any shorter request, 0 included. Longer requests are unchanged.
  * The constant's name and comment give the reason: on the hardware a short delay takes far longer because of the thread switch and the timer, and games' wait loops rely on it.
* Other waits (`WaitSema`, `WaitEventFlag` timeouts and so on) are unchanged.
* No game information goes in; the comment describes a generic wait loop.

## 3. reDMP1

* Update repsp, and correct the README's description of the music offset: the default of 0 is now right, and the setting is for differences between output devices.
* Work log: `docs/work-logs/20261121-i054-delaythread-minimum.md`.

## 4. Verification

1. **Probe** (no window, deterministic):
   * `DelayThread(10)` and `DelayThread(0)` register a wait of at least 200 µs (checked by deadline).
   * `DelayThread(5000)` is unchanged.
   * The CB form behaves the same.
   * Check with the scheduler's deadline or a test hook, not with elapsed real time.
2. **Game** (windowed, the same input script, three runs):
   * no unresolved-address dispatches, no crashes, no mid-song underruns;
   * present interval in the earlier range (about 19 ms);
   * chart t0 and the first music block within 5 ms of each other, checked with temporary instrumentation that is not committed.
3. Win64 Debug/Release (including the Visual Studio generator) and Linux x64, build and CTest, for repsp and reDMP1.
4. The user checks with a music offset of 0 (confirmed).
