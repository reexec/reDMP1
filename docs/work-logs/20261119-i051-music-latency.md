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

## 4. 후속 작업: 스케줄링 두 가지와 음악 오프셋 (`#51`, `#52` / `repsp #54`, `#55`, 2026-11-20)

* 관련 설계: [20261120-i051-scheduling-and-offset.md](../design/20261120-i051-scheduling-and-offset.md)
* 관련 작업 지시: [20261120-i051-scheduling-and-offset.md](../work-orders/20261120-i051-scheduling-and-offset.md)

### 변경 내용 (단위별)
1. **`repsp #54` (GL 잠금 대기 중 게스트 CPU 양보)**:
   * `third_party/repsp/src/gpu/gl_backend.cpp`: 모든 `g_gl.mutex` 잠금(26곳)을 `LockGl()`(`try_lock` 실패 시 `Scheduler::ScopedHostWait` 안에서 대기 후 재시도)로 유지하고, 결정적 테스트 훅 `LockGlMutexForTest`/`UnlockGlMutexForTest`를 `include/repsp/gpu/render_backend.h`와 함께 추가했습니다.
   * `third_party/repsp/tools/gl_probe/main.cpp`: 다른 스레드가 `g_gl.mutex`를 잡은 상태에서 게스트 스레드의 `HasGlTargetForAddress()`가 호스트 대기로 전환되어 다른 게스트 스레드가 실행·해제하는지 결정적으로 검증하는 `TestGlLockReleasesGuestCpu()`를 추가했습니다(GL 컨텍스트 유무와 무관하게 항상 실행).
2. **`repsp #55` (ATRAC 시작 오프셋, 양수·음수)**:
   * `third_party/repsp/include/repsp/hle/atrac.h`, `src/hle/atrac/hle_atrac.cpp`: `SetStartOffsetMs(int32_t)` / `GetStartOffsetMs()`(기본 0)를 추가했습니다. 양수이면 열 때 `raw_pos = min(delay + offset * sample_rate / 1000, endSample + delay)`로 앞당기고, 음수이면 `|offset| * sample_rate / 1000`개의 무음 샘플을 이월 버퍼(`carry_pcm`, `is_silence_pad=true`)에 채워 블록 크기(1024)를 유지한 채 늦춥니다. 루프와 Reset 계열에는 적용하지 않습니다.
   * `third_party/repsp/tools/atrac_probe/main.cpp`: 8번 검증 항목으로 `0 ms`, `+1000 ms`, `-1000 ms`(앞쪽 44100 무음 + 44번째 블록 인덱스 68부터 원시 1561 시작, 블록 크기 1024 유지)와 루프·`ResetPlayPosition` 시 오프셋 미적용을 검증했습니다.
3. **`psprecomp` 로컬 패치 `0006`**:
   * `patches/psprecomp/0006-loop-hook.patch`: `emit.c` 헤더에 `#ifdef PSPRECOMP_LOOP_HOOK` 분기(`void psp_loop_hook(uint32_t addr);`, `PSP_LOOP(a)` 연결; 미정의 시 기존과 동일)를 추가하고 `CMakeLists.txt`의 `ALLEGREXRECOMP_PATCHES`에 등록했습니다(`third_party/psprecomp` 서브모듈은 수정하지 않음).
   * `patches/psprecomp/README.md`: `0006` 항목 및 upstream 상태("준비만 함")를 기록했습니다.
4. **`reDMP1 #52` (루프 되돌이 선점)**:
   * `src/game/dmp1/recomp/CMakeLists.txt`: `redmp1_recomp`에 `PSPRECOMP_LOOP_HOOK` 정의 추가.
   * `src/recomp_abi/cpu.c`: 1024회마다 `repsp_recomp_poll()`을 호출하는 `psp_loop_hook(uint32_t)` 추가.
   * 샌드박스에서 `.gitignore`된 `src/game/dmp1/recomp/generated/` 직접 쓰기가 차단되어, 패치된 `allegrexrecomp`로 `%TEMP%\redmp1-build\i051-gen`에 생성 코드를 재생성하고 `-DREDMP1_GENERATED_DIR`로 연결해 빌드·검증했습니다.
5. **`reDMP1 #51` (Audio 탭 및 `MusicOffsetMs`)**:
   * `src/app/settings.h`, `src/app/settings.cpp`: `[Audio]` 섹션의 `MusicOffsetMs` (`-300..300`, 기본 0, 범위 밖·잘못된 값 시 기본값 유지) 추가.
   * `src/app/settings_ui.h`, `src/app/settings_ui.cpp`: 메뉴에 **Audio** 탭과 `Music offset (ms)` 슬라이더(`-300..+300`, 5 ms 단위, 설명 문구 포함) 추가.
   * `src/app/main.cpp`: 시작 시 `repsp::hle::atrac::SetStartOffsetMs(settings.music_offset_ms)` 적용 및 `[redmp1] Music offset: <n> ms` 로그 출력.
   * `src/app/settings_probe.cpp`: 양수(`125`), 음수(`-85`) 왕복 및 범위 밖(`305`, `-305`, `bogus`) 기본값 유지 검증 추가.
6. **문서**:
   * `third_party/repsp/ARCHITECTURE.md` (GL `LockGl` 호스트 대기, `SetStartOffsetMs(int32_t)`), `README.md` (Audio 탭, `MusicOffsetMs`).

### 검증 및 측정 결과
* **빌드 및 CTest (음수 오프셋 최종 코드 기준)**:
  * Win64 Ninja Release: `37/37` 통과 (`gl_probe`, `atrac_probe` 8/8, `redmp1_settings_probe` 포함).
  * Win64 Ninja Debug: `37/37` 통과.
  * Win64 Ninja `nogamedata`: `34/34` 통과 (`3.71 sec`).
  * Win64 VS 18 2026 Release: `37/37` 통과 (`6.13 sec`).
  * WSL / Linux: `Wsl/E_ACCESSDENIED`로 미실행.
* **창 모드 플레이 스크립트 3회 (`--renderer gl --max-vblanks 3600 --input-script i041-keysound-script.txt`, `REPSP_SCHED_STATS=1`)**:
  * 1회: `Dispatches=0`, `Present=3173` (평균 `18.93 ms`), `ch1 ur=136` (꼬리만, 곡 중간 0), 기상 지연 `wakes=57081 avg=20 us max=2.71 ms (>=10ms=0)`, `Exit=0`
  * 2회: `Dispatches=0`, `Present=3173` (평균 `18.93 ms`), `ch1 ur=136` (꼬리만, 곡 중간 0), 기상 지연 `wakes=57603 avg=21 us max=5.00 ms (>=10ms=0)`, `Exit=0`
  * 3회: `Dispatches=0`, `Present=3176` (평균 `18.91 ms`), `ch1 ur=136` (꼬리만, 곡 중간 0), 기상 지연 `wakes=57069 avg=21 us max=6.66 ms (>=10ms=0)`, `Exit=0`
* **소프트웨어 렌더러 & `--headless` 스모크 (`--max-vblanks 100`)**:
  * `--renderer sw`: `[redmp1] Music offset: 0 ms`, `Dispatches=0`, `Exit=0`
  * `--headless`: `[redmp1] Music offset: 0 ms`, `Dispatches=0`, `Exit=0`

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

## 4. Follow-up: Scheduling Fixes & Music Offset (`#51`, `#52` / `repsp #54`, `#55`, 2026-11-20)
* **Changes**:
  * `repsp #54`: Kept `LockGl()` (`ScopedHostWait` on `g_gl.mutex` contention across all 26 sites in `gl_backend.cpp`), added deterministic test hooks `LockGlMutexForTest`/`UnlockGlMutexForTest`, and added `TestGlLockReleasesGuestCpu()` to `gl_probe`.
  * `repsp #55`: Added `repsp::hle::atrac::SetStartOffsetMs(int32_t)` / `GetStartOffsetMs()` (positive advances `raw_pos` capped at end; negative prepends silence samples into `carry_pcm` preserving 1024-sample blocks; ignored on loops and resets) and added check 8 (`0`, `+1000 ms`, `-1000 ms`) to `atrac_probe`.
  * `psprecomp 0006` & `reDMP1 #52`: Added local patch `patches/psprecomp/0006-loop-hook.patch` (wired in `CMakeLists.txt`, documented in `patches/psprecomp/README.md` as prepared only), defined `PSPRECOMP_LOOP_HOOK` on `redmp1_recomp`, added `psp_loop_hook()` in `src/recomp_abi/cpu.c` polling `repsp_recomp_poll()` every 1024 back-edges, and regenerated lifted C into `%TEMP%\redmp1-build\i051-gen` (used via `-DREDMP1_GENERATED_DIR` because `.gitignore`d `src/game/dmp1/recomp/generated/` is not writable in the sandbox).
  * `reDMP1 #51`: Added `[Audio] MusicOffsetMs` (`-300..300`, default 0) to `settings.{h,cpp}`, **Audio** tab slider (`-300..+300 ms`, step 5) to `settings_ui.{h,cpp}`, startup apply & log in `main.cpp`, and positive/negative/out-of-range tests in `settings_probe.cpp`. Updated `third_party/repsp/ARCHITECTURE.md` and `README.md`.
* **Verification**: Win64 Ninja Release (`37/37`), Debug (`37/37`), `nogamedata` (`34/34`), and VS 18 2026 Release (`37/37`, `6.13 s`) CTest passed; 3x windowed gameplay runs (`--max-vblanks 3600`, `REPSP_SCHED_STATS=1`) passed with 0 dispatches, `ch1 ur=136` (tails only), present mean `18.91–18.93 ms`, and max wake latency `2.71 / 5.00 / 6.66 ms` (zero wakes >= 10 ms, well below 30 ms); `--renderer sw` and `--headless` smoke tests passed. WSL/Linux blocked by `Wsl/E_ACCESSDENIED`.
