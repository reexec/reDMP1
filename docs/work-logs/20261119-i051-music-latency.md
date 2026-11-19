# 작업 로그: 배경음이 노트보다 늦음 (`#51` / `repsp #52`)

* 날짜: 2026-11-19
* 브랜치: `reDMP1` (`i051-music-latency`), `third_party/repsp` (`i52-audio-release`)
* 관련 설계: [20261119-i051-music-latency.md](../design/20261119-i051-music-latency.md)
* 관련 작업 지시: [20261119-i051-music-latency.md](../work-orders/20261119-i051-music-latency.md)

---

## 1. 변경 내용

* **`third_party/repsp/src/hle/audio/hle_audio.cpp`**:
  * `WaitConditionMetLocked()`의 blocking 출력 반환 기준을 `ch.queue.size() <= static_cast<size_t>(ch.sample_count)`에서 `ch.queue.size() <= std::min(kClockQuantumFrames, static_cast<size_t>(ch.sample_count))`로 변경하고 이유를 주석으로 남겼습니다.
  * `buf_addr == 0` drain 경로(`wait_for_zero == true`)는 기존대로 대기열이 완전히 빌 때(`ch.queue.empty()`) 반환하도록 유지했습니다.
* **`third_party/repsp/tools/audio_probe/main.cpp`**:
  * 9번 검증 항목을 추가했습니다. `StepMixClockForTest()`를 사용해 스레드 타이밍에 의존하지 않고 결정적으로 다음을 검증합니다.
    1. 1024샘플 채널이 대기열 256 이상에서는 깨어나지 않고 128 이하가 될 때 깨어남(`rest == 1152`).
    2. `buf_addr == 0` drain은 대기열 128에서도 깨어나지 않고 0이 될 때 깨어남(`rest == 0`).
    3. 128보다 작은 `sample_count`(64샘플 채널)는 `sample_count`(64) 기준으로 동작함(`rest == 64`).
* **`third_party/repsp/ARCHITECTURE.md`**:
  * 오디오 계약 절(한국어·영어)에 `min(kClockQuantumFrames, sample_count)` 반환 조건과 이유를 기록했습니다.

---

## 2. 검증 및 측정 결과

### 빌드 및 CTest (임시 계측 제거 후 최종 코드 기준)
* **Win64 Ninja Release** (`%TEMP%\redmp1-build\redmp1-win64-i039-rel`): `37/37` 통과 (`6.03 sec`), `audio_probe` 100회 연속 통과 (`[PASS] audio_probe: 9/9 checks passed`).
* **Win64 Ninja Debug** (`%TEMP%\redmp1-build\redmp1-win64-i039-dbg`): `37/37` 통과 (`7.39 sec`), `audio_probe` 100회 연속 통과 (`[PASS] audio_probe: 9/9 checks passed`).
* **Win64 VS 18 2026 Release** (`%TEMP%\redmp1-build\redmp1-win64-i043-vs`): `37/37` 통과 (`5.95 sec`).
* **WSL / Linux**: 이 환경에서 `Wsl/E_ACCESSDENIED`로 WSL 접근이 차단되어 미실행(규칙에 따라 우회하지 않음).

### 창 모드 플레이 스크립트 3회 측정 (`--renderer gl --max-vblanks 3600 --input-script i041-keysound-script.txt`, 임시 계측 사용 후 계측 코드는 제거)
| 실행 | 배경음(`ch1`, 1024) 평균 대기열 | 배경음 끊김(`ur`) | 효과음(`ch2`, 256) 평균 대기열 | 효과음 끊김(`ur`) | 중간 버퍼 평균(`buf_depth`) | 디스패치 | 종료 코드 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | 678 | 136 (모두 꼬리, 곡 중간 0) | 229 | 34 | 881 | 0 | 0 |
| 2 | 677 | 136 (모두 꼬리, 곡 중간 0) | 229 | 38 | 881 | 0 | 0 |
| 3 | 678 | 136 (모두 꼬리, 곡 중간 0) | 229 | 37 | 882 | 0 | 0 |

---

## 3. 후속 작업: ATRAC 인코더 지연 건너뛰기 (`#51` / `repsp #53`, 2026-11-20)

* 관련 설계: [20261120-i051-atrac-encoder-delay.md](../design/20261120-i051-atrac-encoder-delay.md)
* 관련 작업 지시: [20261120-i051-atrac-encoder-delay.md](../work-orders/20261120-i051-atrac-encoder-delay.md)

### 변경 내용
* **`third_party/repsp/src/hle/atrac/hle_atrac.cpp`**:
  * 원시 샘플 위치 모델(`raw_pos`, `decode_raw_pos`, 열 때 `encoder_delay`, 루프 없으면 `endSample + encoder_delay` 종료, 루프 활성 시 `loopEnd + 1` → `loopStart`)을 적용했습니다.
  * 핸들별 이월 버퍼(`carry_pcm`, `carry_segs`)를 도입해 지연 구간 프레임은 코덱 상태 유지를 위해 디코드 후 버리고, 마지막 블록을 제외하고 매 호출 `samples_per_frame`(1024/2048) 고정 블록 크기를 유지합니다. 스트리밍 데이터 부족 시 이월 버퍼를 잃지 않고 0샘플을 반환합니다.
  * 루프 도달 시 코덱 상태를 초기화하고 같은 호출 안에서 `loopStart`부터 블록을 마저 채웁니다(스트리밍 모드에서는 루프 대상 프레임 위치로 읽기 오프셋을 옮기고 링 버퍼를 비워 재읽기를 유도합니다).
  * 외부 위치 인터페이스(`sceAtracGetSoundSample`의 `loopStart`/`loopEnd`, `sceAtracResetPlayPosition`, `sceAtracGetBufferInfoForReseting`)를 들리는 샘플 기준(`raw - encoder_delay`)으로 통일했습니다.
  * 리뷰어가 작업 트리에 고쳐 둔 `ATRAC_TRACE` Open 줄을 유지하고 Decode 줄에 `raw_pos`를 출력하도록 갱신했습니다.
* **`third_party/repsp/tools/atrac_probe/main.cpp`**:
  * 프레임 번호와 프레임 내 샘플 번호를 스테레오 PCM으로 인코딩하는 `IndexedFrameDecoder`를 추가하고 검증을 7개 항목으로 확장했습니다(`delay=0` 기존 동작, `delay=1561`·`spf=1024` 연속 출력 및 짧은 마지막 블록/끝 판정, 무한 루프 `-1`의 블록 내 이음새 이어붙이기, 스트리밍 데이터 부족 시 이월 버퍼 보존).
* **`third_party/repsp/ARCHITECTURE.md`**:
  * ATRAC 계약 절(한국어·영어)에 원시/들리는 샘플 위치 모델, 지연 건너뛰기, 루프 이어붙이기, 고정 블록 크기 규칙을 추가했습니다.

### 검증 및 측정 결과
* **빌드 및 CTest**:
  * Win64 Ninja Release: `37/37` 통과 (`7.44 sec`), `atrac_probe` `7/7` 통과.
  * Win64 Ninja Debug: `37/37` 통과 (`9.16 sec`), `atrac_probe` `7/7` 통과.
  * Win64 VS 18 2026 Release: `37/37` 통과 (`6.96 sec`).
  * WSL / Linux: `Wsl/E_ACCESSDENIED`로 미실행.
* **창 모드 플레이 스크립트 3회 (`--renderer gl --max-vblanks 3600 --input-script i041-keysound-script.txt`)**:
  * 1회: `Dispatches=0`, `Present=3189` (mean 18.83 ms), `ch1 ur=136` (꼬리만, 곡 중간 0), `Exit=0`
  * 2회: `Dispatches=0`, `Present=3186` (mean 18.85 ms), `ch1 ur=136` (꼬리만, 곡 중간 0), `Exit=0`
  * 3회: `Dispatches=0`, `Present=3192` (mean 18.82 ms), `ch1 ur=136` (꼬리만, 곡 중간 0), `Exit=0`
* **곡 선택 미리듣기 (`--renderer gl --max-vblanks 3600 --input-script i045-songselect-script.txt`, FFmpeg 플러그인 + `REPSP_ATRAC_TRACE=1` + `REPSP_AUDIO_DUMP`)**:
  * 첫 트랙(`delay=1912`) 첫 디코드 `raw_pos=2936` (`1912 + 1024`), 두 번째 트랙(`delay=1561`) 마지막 디코드 `raw_pos=717337` (`1561 + 699 * 1024`), `Dispatches=0`, `ch1 ur=102` (3회 종료 꼬리만, 곡 중간 0), `mix.ch1.wav` 989,184 프레임(`max_abs=32767`) 정상 디코드 확인. (미리듣기 트랙은 게임이 `loopNum=0`으로 열어 3600 vblank 동안 루프에 도달하지 않았으며, 루프 이음새 이어붙이기는 `atrac_probe` Check 6으로 결정적 검증.)

---

# Work Log: Background Music Lags Notes (`#51` / `repsp #52`) (English)

* Date: 2026-11-19
* Branches: `reDMP1` (`i051-music-latency`), `third_party/repsp` (`i52-audio-release`)
* Design: [20261119-i051-music-latency.md](../design/20261119-i051-music-latency.md)
* Work Order: [20261119-i051-music-latency.md](../work-orders/20261119-i051-music-latency.md)

## 1. Changes
* **`third_party/repsp/src/hle/audio/hle_audio.cpp`**: Changed `WaitConditionMetLocked()` release threshold for blocking output from `ch.queue.size() <= sample_count` to `ch.queue.size() <= std::min(kClockQuantumFrames, static_cast<size_t>(ch.sample_count))` with an explanatory comment; left `buf_addr == 0` drain (`wait_for_zero`) unchanged (`ch.queue.empty()`).
* **`third_party/repsp/tools/audio_probe/main.cpp`**: Added deterministic check 9 using `StepMixClockForTest()` verifying 1024-sample release at `<= 128`, drain waiting until `0`, and `< 128` (`64`-sample) release at `sample_count`.
* **`third_party/repsp/ARCHITECTURE.md`**: Documented the `min(kClockQuantumFrames, sample_count)` release rule and rationale in both Korean and English sections.

## 2. Verification & Measurements
* **Windowed gameplay script (3 runs, `--renderer gl --max-vblanks 3600`, temporary instrumentation removed afterwards)**:
  * Run 1: `ch1` `q_avg=678`, `ur=136` (stream-end tails only), `ch2` `q_avg=229`, `ur=34`, `buf_depth avg=881`, `Dispatches=0`, `Exit=0`.
  * Run 2: `ch1` `q_avg=677`, `ur=136` (stream-end tails only), `ch2` `q_avg=229`, `ur=38`, `buf_depth avg=881`, `Dispatches=0`, `Exit=0`.
  * Run 3: `ch1` `q_avg=678`, `ur=136` (stream-end tails only), `ch2` `q_avg=229`, `ur=37`, `buf_depth avg=882`, `Dispatches=0`, `Exit=0`.
* **CTest & `audio_probe`**: Verified after removing temporary instrumentation across Win64 Ninja Release (`37/37`, `6.03 s`), Win64 Ninja Debug (`37/37`, `7.39 s`), VS 18 2026 Release (`37/37`, `5.95 s`), and 100x consecutive `audio_probe` runs in Release and Debug. WSL/Linux could not run due to `Wsl/E_ACCESSDENIED`.

## 3. Follow-up: Skipping ATRAC Encoder Delay (`#51` / `repsp #53`, 2026-11-20)
* **Changes**: Updated `third_party/repsp/src/hle/atrac/hle_atrac.cpp` to track raw sample positions (`raw_pos` starting at `encoder_delay`, ending at `endSample + encoder_delay`), decode and discard pre-delay samples, maintain fixed `samples_per_frame` output blocks via a carry buffer (preserving carry across streaming shortages), splice active loops from `loopEnd + 1` to `loopStart` inside the same decode call after codec reset, and report audible positions on `sceAtracGetSoundSample`/`ResetPlayPosition`/`GetBufferInfoForReseting`. Extended `atrac_probe` to 7 checks using `IndexedFrameDecoder` and updated `third_party/repsp/ARCHITECTURE.md`.
* **Verification**: Win64 Ninja Release (`37/37`, `7.44 s`), Debug (`37/37`, `9.16 s`), and VS 18 2026 Release (`37/37`, `6.96 s`) CTest passed; 3x windowed gameplay runs (`--max-vblanks 3600`) passed with 0 dispatches and `ch1 ur=136` (stream-end tails only, 0 mid-song underruns); song-select preview with FFmpeg plugin verified `raw_pos` progression (`1912 + 1024 = 2936` on track 1, `1561 + 699 * 1024 = 717337` on track 2), `mix.ch1.wav` valid PCM, and 0 mid-song underruns. WSL/Linux blocked by `Wsl/E_ACCESSDENIED`.
