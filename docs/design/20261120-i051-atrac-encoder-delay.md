# 설계: ATRAC 인코더 지연 건너뛰기 (issue #51, repsp #53)

앞 설계: [20261119-i051](20261119-i051-music-latency.md). 반환 기준을 바꾼 뒤에도 사용자는 음악이 "확연히" 늦다고 했다(2026-11-20). 같은 PC와 같은 출력 장치(USB DAC)의 PPSSPP에서는 맞는다.

## 1. 진단

* 곡이 시작될 때 게임의 호출 순서(HLE 추적):
  1. `sceAtracSetDataAndGetID`, `sceAtracGetSoundSample`, `sceAtracGetRemainFrame`
  2. `sceAudioSetChannelDataLen(1, 1024)`
  3. 블록마다 `sceAtracDecodeData` → `sceAudioSetChannelDataLen(1, 1024)` → `sceAudioOutputPannedBlocking(1)`
* 이 게임의 음악은 ATRAC3(tag `0x0270`, 1024 샘플 프레임)이다. `fact` 청크의 두 번째 값(인코더 지연)은 다음과 같다. `ATRAC_TRACE`로 읽었다.

  | 트랙 | endSample | delay | loopStart | loopEnd |
  |---|---|---|---|---|
  | 처음 트랙 | 2494531 | 1912 (43 ms) | 1912 | 2496442 |
  | 플레이 곡 | 4751778 | 1561 (35 ms) | 1561 | 4753338 |

  * `loopStart = delay`, `loopEnd = endSample + delay − 1`이다. 루프 지점은 지연을 포함한 원시(raw) 샘플 위치이고, 들리는 첫 샘플은 원시 위치 `delay`다. `endSample`은 들리는 샘플 수다.
* **원인**: repsp는 `encoder_delay`를 읽기만 하고 쓰지 않는다. 그래서 다음 문제가 생긴다.
  * 지연 구간을 그대로 출력해서, 모든 트랙이 35~43 ms 늦게 시작한다.
  * 끝 판정이 원시 위치를 `endSample`과 비교해서, 트랙 끝의 `delay` 샘플이 잘린다.
  * 루프가 `loopStart`가 아니라 그 앞 프레임 경계에서 다시 시작한다(888 샘플 어긋남).
* 덧붙여 `ATRAC_TRACE`의 Open 줄은 인자가 하나씩 밀려 `delay` 자리에 `loop_start`를 찍는다. 리뷰어가 작업 트리에서 고쳤고, 이 작업에 함께 넣는다.

## 2. 변경 (repsp `src/hle/atrac/hle_atrac.cpp`)

* **원시 위치 모델**: 핸들은 다음에 내보낼 원시 샘플 위치 `raw_pos`를 가진다.
  * 열 때 `raw_pos = delay`
  * 끝: 루프가 없거나 루프 횟수가 0이면 `raw_end = endSample + delay`, 루프 중이면 `loopEnd + 1`
* **블록 크기 유지**: `sceAtracDecodeData`는 지금처럼 한 번에 `samples_per_frame`(1024)개를 낸다. 마지막 블록만 남은 수만큼 낸다.
  * 디코드한 프레임 k는 원시 구간 `[k·spf, (k+1)·spf)`를 덮는다. 출력 창은 `raw_pos`부터라서, 한 번의 출력이 두 프레임에 걸친다.
  * 남은 샘플은 핸들의 이월 버퍼에 두었다가 다음 호출에 쓴다.
  * 첫 호출에서는 `raw_pos` 앞의 프레임(지연 구간)을 디코드만 하고 버린다. 코덱 상태를 위해 디코드는 한다.
  * 블록을 짧게 돌려주지 않는 이유: 이 게임은 블록마다 `sceAudioSetChannelDataLen(1, 1024)`을 고정값으로 부른다. 짧은 블록을 존중하는지 확인할 수 없다.
* **스트리밍**: 한 호출이 프레임을 둘 이상 디코드할 수 있다. 링 버퍼에 그만큼의 데이터가 없으면, 지금처럼 0개를 돌려주고 다음 호출을 기다린다. 이월 버퍼에 남은 샘플은 잃지 않는다.
* **루프**: `raw_pos`가 `loopEnd + 1`에 닿고 루프 횟수가 남았다면 다음과 같이 처리한다.
  * `raw_pos = loopStart`로 옮긴다.
  * `loopStart / spf` 프레임부터 다시 디코드하고, 그 프레임의 `loopStart` 앞부분은 버린다.
  * 이월 버퍼를 비우고 코덱 상태를 초기화한다.
  * 같은 호출 안에서 블록을 마저 채운다. 그래서 루프 이음새가 블록 안에 들어가도 블록 크기는 그대로다.
* **바깥에서 보는 위치**는 들리는 샘플 기준(원시 − delay)으로 통일한다.
  * `sceAtracResetPlayPosition(sample)`, `sceAtracGetBufferInfoForReseting(sample)`: 원시 위치 `sample + delay`
  * `sceAtracGetSoundSample`: loopStart/loopEnd는 `− delay`, endSample은 지금 값 그대로. 루프가 없으면 −1.
  * 실기의 정확한 값은 문서로 확인하지 못했다(**미확인**). 이 게임은 Reset 계열을 부르지 않는다(0회).
* `fact`가 없거나 delay가 0인 파일은 지금과 같은 출력이 나와야 한다.
* `ATRAC_TRACE` Open 줄의 인자를 바로잡는다. Decode 줄에 `raw_pos`를 찍는다.

## 3. 검증

1. **probe**(`atrac_probe` 또는 기존 ATRAC probe, 창 없이, 결정적). 무음 디코더 대신 프레임 번호와 샘플 번호를 PCM으로 내는 시험용 디코더를 쓴다.
   * delay 1561, spf 1024: 첫 호출이 1024개를 내고, 첫 샘플이 원시 1561이다. 이어지는 호출은 끊김 없이 이어진다.
   * 끝: 들리는 샘플 수가 정확히 endSample이다. 마지막 블록이 짧고, 끝 표시가 맞다.
   * 루프(-1): loopEnd 다음 샘플이 loopStart다. 블록 크기가 유지된다.
   * 스트리밍: 데이터가 모자라면 0개를 내고, 데이터를 넣은 뒤 이월분부터 이어진다.
   * delay 0인 파일은 지금과 같다.
2. **게임**(창 모드, 같은 입력 스크립트 3회):
   * 미발견 주소 dispatch 0, 크래시 0
   * 배경음 곡 중간 끊김 0
   * 곡 선택 미리듣기의 루프 이음새가 튀지 않는다(덤프로 확인).
3. Win64 Debug/Release(Visual Studio 생성기 포함), Linux x64 빌드·CTest.
4. 사용자가 배경음과 노트를 맞춰 본다(사용자 확인).

## 4. 기록

* repsp `ARCHITECTURE.md`의 ATRAC 절에 위치 모델(원시/들리는 샘플, 지연, 루프, 고정 블록 크기)을 적는다.
* reDMP1 작업 로그 `docs/work-logs/20261119-i051-music-latency.md`에 이어서 적는다.

---

# Design: skipping the ATRAC encoder delay (issue #51, repsp #53)

Previous design: [20261119-i051](20261119-i051-music-latency.md). After the release-threshold change, the user still found the music "clearly" late (2026-11-20), while PPSSPP on the same PC and output device (a USB DAC) is in sync.

## 1. Diagnosis

* **The game's calls at song start** (HLE trace):
  1. `sceAtracSetDataAndGetID`, `sceAtracGetSoundSample`, `sceAtracGetRemainFrame`;
  2. `sceAudioSetChannelDataLen(1, 1024)`;
  3. then, per block, `sceAtracDecodeData` → `sceAudioSetChannelDataLen(1, 1024)` → `sceAudioOutputPannedBlocking(1)`.
* **The tracks**: this title's music is ATRAC3 (tag `0x0270`, 1024-sample frames). The `fact` chunk's second value is the encoder delay, read with `ATRAC_TRACE`:

  | track | endSample | delay | loopStart | loopEnd |
  |---|---|---|---|---|
  | first track | 2494531 | 1912 (43 ms) | 1912 | 2496442 |
  | gameplay song | 4751778 | 1561 (35 ms) | 1561 | 4753338 |

  * `loopStart = delay` and `loopEnd = endSample + delay − 1`. So the loop points are raw sample positions that include the delay, the first audible sample is raw position `delay`, and `endSample` counts audible samples.
* **Cause**: repsp parses `encoder_delay` and never uses it. As a result:
  * the delay's samples are output, so every track starts 35-43 ms late;
  * the end test compares the raw position with `endSample`, so a track's last `delay` samples are cut;
  * a loop restarts at the frame boundary before `loopStart` rather than at `loopStart` (888 samples off).
* Also, the `ATRAC_TRACE` open line has its arguments shifted by one and prints `loop_start` under `delay`. The reviewer fixed this in the working tree, and it goes in with this work.

## 2. Change (repsp `src/hle/atrac/hle_atrac.cpp`)

* **Raw position model**: a handle keeps `raw_pos`, the next raw sample to output.
  * On open, `raw_pos = delay`.
  * The end is `raw_end = endSample + delay` with no loop or no loops left, and `loopEnd + 1` while looping.
* **Block size kept**: `sceAtracDecodeData` still outputs `samples_per_frame` (1024) samples per call. Only the last block holds what remains.
  * Decoded frame k covers raw `[k·spf, (k+1)·spf)`. The output window starts at `raw_pos`, so one output spans two frames.
  * Leftover samples wait in a carry buffer in the handle for the next call.
  * On the first call, the frames before `raw_pos` (the delay) are decoded and discarded; decoding them keeps the codec state right.
  * Why not return a short block: this title calls `sceAudioSetChannelDataLen(1, 1024)` with a fixed value before every block, and whether it honours a short block cannot be confirmed.
* **Streaming**: one call may decode more than one frame. If the ring buffer does not hold enough, the call returns 0 samples as it does today and waits for the next. Samples in the carry buffer are not lost.
* **Loop**: when `raw_pos` reaches `loopEnd + 1` and loops remain:
  * `raw_pos` moves to `loopStart`;
  * decoding restarts at frame `loopStart / spf`, discarding that frame's part before `loopStart`;
  * the carry buffer is emptied and the codec state reset;
  * the block is filled within the same call, so the loop seam may fall inside a block and the block size stays the same.
* **Positions seen from outside** are in audible samples (raw − delay):
  * `sceAtracResetPlayPosition(sample)` and `sceAtracGetBufferInfoForReseting(sample)` take raw position `sample + delay`;
  * `sceAtracGetSoundSample` gives loopStart and loopEnd `− delay`, and endSample as today; −1 without a loop.
  * The hardware's exact values are not confirmed by documentation (**unconfirmed**). This title never calls the reset functions (0 calls).
* A file with no `fact`, or a delay of 0, gives the same output as today.
* The `ATRAC_TRACE` open line's arguments are corrected, and the decode line prints `raw_pos`.

## 3. Verification

1. **Probe** (`atrac_probe` or the existing ATRAC probe; no window, deterministic). Instead of the silent decoder, it uses a test decoder whose PCM encodes the frame and sample number.
   * With delay 1561 and spf 1024: the first call outputs 1024 samples starting at raw 1561, and the following calls continue without a gap.
   * End: exactly endSample audible samples, a short last block, and the end flag right.
   * Loop (−1): the sample after loopEnd is loopStart, and the block size holds.
   * Streaming: short of data, a call outputs 0 samples; after data is added, it continues from the carry.
   * A file with delay 0 is unchanged.
2. **Game** (windowed, the same input script, three runs):
   * no unresolved-address dispatches, no crashes;
   * no mid-song music underruns;
   * the song-select preview's loop seam does not click (checked in a dump).
3. Win64 Debug/Release (including the Visual Studio generator) and Linux x64, build and CTest.
4. The user checks music against notes (the user's check).

## 4. Records

* repsp `ARCHITECTURE.md`, ATRAC section: the position model (raw and audible samples, the delay, loops, fixed block size).
* reDMP1 work log: continue `docs/work-logs/20261119-i051-music-latency.md`.
