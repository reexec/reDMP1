# 설계: 배경음이 노트보다 늦음 (issue #51, repsp #52)

## 1. 보고와 진단

* 사용자 보고(2026-11-19): 배경음과 노트 타이밍이 맞지 않는다. 음악이 더 늦다.
* **지난 오디오 수정(repsp v0.0.20) 때문인지 확인했다. 원인이 아니다.** 같은 입력 스크립트(플레이 화면까지, 60초, 창 모드)를 세 빌드로 돌렸다.

  | 빌드 | 배경음 채널 끊김 | 중간 버퍼 평균 | 효과음 채널 끊김 |
  |---|---|---|---|
  | repsp v0.0.19 | 137 | 880 | 1,103 |
  | repsp v0.0.20 | 138 | 877 | 39 |
  | repsp v0.0.24 | 136 | 880 | 40 |

  * 배경음 채널의 끊김은 모두 스트림이 끝난 뒤 100 ms 꼬리(34회씩 네 번)다. 곡 중간에 무음이 끼어 점점 밀리는 문제가 아니다.
  * 게임은 노트 시간을 `sceKernelGetSystemTimeWide`(약 44,000회 호출)로 잰다. repsp에서 이 값은 호스트의 `steady_clock`이다.
* **원인**: 배경음이 스피커에 닿기까지의 경로가 길다(임시 계측, 커밋하지 않음).

  | 구간 | 배경음(1024 샘플 블록) | 효과음(256 샘플 블록) |
  |---|---|---|
  | 채널 대기열 | 1793 프레임 = 40.7 ms | 447 프레임 = 10.1 ms |
  | 중간 버퍼(목표 20 ms) | 944 프레임 = 21.4 ms | 같음 |
  | 장치 버퍼(WASAPI) | 441 프레임 = 10 ms | 같음 |

  * blocking 출력(`sceAudioOutput*Blocking`)은 채널 대기열이 한 블록(`sample_count`) 이하가 되면 돌아간다. 게임은 돌려받자마자 다음 블록을 넣으므로, 대기열이 늘 1~2블록이다.

## 2. 변경 (repsp)

* blocking 출력이 돌아가는 조건을 **대기열 ≤ min(믹싱 quantum 128 프레임, `sample_count`)**로 바꾼다. drain(`buf_addr == 0`)은 그대로 대기열이 빌 때 돌아간다.
* 측정(R은 반환 기준, 같은 스크립트, 창 모드):

  | R | 배경음 대기열 | 효과음 대기열 | 곡 중간 끊김 | 효과음 끊김 |
  |---|---|---|---|---|
  | 한 블록(지금) | 1793 | 447 | 0 | 39 |
  | 512 | 1152 | 447 | 0 | 38 |
  | 256 | 894 | 447 | 0 | 41 |
  | **128** | **769** | **320** | **0** | **37** |

  R=128을 두 번 더 돌렸다. 배경음 768/770, 효과음 319/320, 곡 중간 끊김 0, 효과음 끊김 40/39였다.
* 배경음 지연이 약 23 ms 줄고, 효과음과의 차이가 30 ms에서 10 ms 정도로 준다.
* 늦은 생산자는 중간 버퍼(약 20 ms)와 v0.0.20의 대기(활성 채널이 비면 무음 대신 기다림)가 받쳐 준다.
* `sceAudioGetChannelRestLength`도 같은 대기열에서 나오므로 함께 작아진다.
* 실기의 반환 시점은 문서로 확인할 수 없다. 측정과 사용자 청취로 정한다.

## 3. 검증

1. `audio_probe`: 새 반환 조건을 검사한다.
   * 1024 샘플 채널이 대기열 128 이하에서 깨고, 그보다 많을 때는 깨지 않는다.
   * 128보다 작은 `sample_count`는 `sample_count` 기준이다.
   * drain은 그대로다.
   * 기존 시험의 결정적 단계 함수(StepMixClockForTest)를 쓰고, 스레드 타이밍에 기대지 않는다.
2. 같은 플레이 스크립트 3회(창 모드): 배경음 대기열 평균, 곡 중간 끊김 0, 효과음 끊김이 늘지 않음, 미발견 주소 dispatch 0. 대기열 계측은 커밋하지 않는다.
3. Win64 Debug/Release(Visual Studio 생성기 포함), Linux x64 빌드·CTest. `audio_probe`를 Release와 Debug에서 각각 100회 연속 돌린다.
4. 사용자가 플레이에서 배경음과 노트를 맞춰 본다(사용자 확인).

## 4. 기록

* repsp `ARCHITECTURE.md`의 오디오 절에 반환 조건과 이유를 적는다.
* reDMP1 작업 로그 `docs/work-logs/20261119-i051-music-latency.md`.

---

# Design: background music lags the notes (issue #51, repsp #52)

## 1. Report and diagnosis

* **User report (2026-11-19)**: the background music and the notes are out of step; the music is later.
* **Whether the last audio change (repsp v0.0.20) caused it was checked first. It did not.** The same input script (to gameplay, 60 s, windowed) ran on three builds:

  | build | music channel underruns | mixing buffer mean | effects channel underruns |
  |---|---|---|---|
  | repsp v0.0.19 | 137 | 880 | 1,103 |
  | repsp v0.0.20 | 138 | 877 | 39 |
  | repsp v0.0.24 | 136 | 880 | 40 |

  * Every music channel underrun is a 100 ms tail after a stream ends (four runs of 34). No silence enters mid-song, so the music does not drift.
  * The game clocks its notes with `sceKernelGetSystemTimeWide` (about 44,000 calls). In repsp that is the host's `steady_clock`.
* **Cause**: the music's path to the speaker is long (temporary instrumentation, not committed):

  | stage | music (1024-sample blocks) | effects (256-sample blocks) |
  |---|---|---|
  | channel queue | 1793 frames = 40.7 ms | 447 frames = 10.1 ms |
  | mixing buffer (20 ms target) | 944 frames = 21.4 ms | same |
  | device buffer (WASAPI) | 441 frames = 10 ms | same |

  * A blocking output (`sceAudioOutput*Blocking`) returns once the channel queue holds at most one block (`sample_count`). The game queues the next block as soon as it returns, so the queue always holds one to two blocks.

## 2. Change (repsp)

* A blocking output returns once the queue holds **at most min(one mixing quantum of 128 frames, `sample_count`)**. A drain (`buf_addr == 0`) still returns when the queue is empty.
* **Measured** (R is the release threshold; same script, windowed):

  | R | music queue | effects queue | mid-song underruns | effects underruns |
  |---|---|---|---|---|
  | one block (today) | 1793 | 447 | 0 | 39 |
  | 512 | 1152 | 447 | 0 | 38 |
  | 256 | 894 | 447 | 0 | 41 |
  | **128** | **769** | **320** | **0** | **37** |

  R=128 ran twice more: music 768 and 770, effects 319 and 320, no mid-song underruns, effects underruns 40 and 39.
* The music arrives about 23 ms sooner, and its gap to the effects narrows from 30 ms to about 10 ms.
* A late producer is covered by the mixing buffer (about 20 ms) and by v0.0.20's wait: an empty active channel is waited for instead of mixed as silence.
* `sceAudioGetChannelRestLength` reads the same queue, so it shrinks along with it.
* When the real hardware returns cannot be confirmed from documentation. The threshold is set by measurement and the user's listening.

## 3. Verification

1. **`audio_probe`** checks the new release condition:
   * a 1024-sample channel wakes at a queue of 128 or less, and not above it;
   * a `sample_count` below 128 uses `sample_count`;
   * a drain is unchanged;
   * the existing deterministic step hook (StepMixClockForTest) is used, with no reliance on thread timing.
2. **The same gameplay script, three runs** (windowed): the music queue mean, no mid-song underruns, effects underruns not higher, no unresolved-address dispatches. The queue instrumentation is not committed.
3. **Builds**: Win64 Debug/Release (including the Visual Studio generator) and Linux x64, build and CTest. `audio_probe` runs 100 times in a row in each of Release and Debug.
4. The user checks music against notes in gameplay (the user's check).

## 4. Records

* repsp `ARCHITECTURE.md`, audio section: the release condition and why.
* reDMP1 work log: `docs/work-logs/20261119-i051-music-latency.md`.
