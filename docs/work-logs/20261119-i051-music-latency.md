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
