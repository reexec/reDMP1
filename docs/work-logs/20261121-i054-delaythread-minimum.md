# 작업 로그: DelayThread 최소 시간 (issue #54, repsp #57)

* 날짜: 2026-11-21
* 브랜치: `reDMP1` `i054-music-latency-2`, `repsp` `i57-music-latency-2`
* 설계: [20261121-i054-delaythread-minimum.md](../design/20261121-i054-delaythread-minimum.md)
* 작업 지시: [20261121-i054-delaythread-minimum.md](../work-orders/20261121-i054-delaythread-minimum.md)

## 1. 변경 요약

### repsp (`third_party/repsp`, 브랜치 `i57-music-latency-2`)

1. **`sceKernelDelayThread` / `sceKernelDelayThreadCB` 최소 200 µs 적용**
   * `src/hle/kernel/threadman_thread.cpp`: `kMinDelayThreadUs = 200u` 상수를 추가하고, `HleDelayThread`와 `HleDelayThreadCB`에서 요청 시간(`0` 포함)이 200 µs보다 짧으면 200 µs로 올리도록 수정했다. 200 µs 이상 요청과 다른 대기(`WaitSema`, `WaitEventFlag` 등)는 그대로 유지했다.
2. **결정적 마감 시각 검사 훅 및 probe 추가**
   * `include/repsp/runtime/scheduler.h`, `src/runtime/scheduler.cpp`: TCB에 대기 시작 시각(`wait_start`)을 기록하고, 시험용 조회 함수 `Scheduler::GetThreadWaitDeadlineForTest(KernelUid, bool*, int64_t*)`를 추가했다.
   * `tools/kernel_sched_probe/main.cpp`: Check 16을 추가해 `DelayThread(0)`, `DelayThread(10)`, `DelayThread(5000)`, `DelayThreadCB(0)`, `DelayThreadCB(10)`, `DelayThreadCB(5000)`의 등록 마감 기간이 실제 경과 시간에 기대지 않고 `{200, 200, 5000, 200, 200, 5000}` µs인지 결정적으로 검증하도록 했다.

### reDMP1 (브랜치 `i054-music-latency-2`)

1. **`README.md` 갱신**
   * 한국어·영어 `Audio` 메뉴 설명에서 음악 오프셋 기본값 `0`에서 타이밍이 맞으며, 출력 장치 차이를 맞추는 용도임을 반영했다.
2. **작업 로그 작성**
   * 이 문서(`docs/work-logs/20261121-i054-delaythread-minimum.md`).

## 2. 검증 결과

### 2.1 결정적 probe (`kernel_sched_probe`)

* Win64 Release / Debug 모두 `kernel_sched_probe` Check 16 통과(`kernel_sched_probe passed.`).

### 2.2 게임 창 모드 측정 (3회, 임시 계측 후 제거)

* 실행 조건: `redmp1.exe roms --renderer gl --max-vblanks 3600 --input-script <keysound-script>` (Release)
* 임시 계측(`chart_t0`: `0x08861704` → `0x08837808` → `sceKernelGetSystemTimeWide`, `music_first`: `ch1` 비활성→활성 첫 출력)은 측정 후 완전히 제거했다.

| 회차 | 첫 곡 `chart_t0 - music_first` | 플레이 곡 `chart_t0 - music_first` | Dispatches | Present 간격 평균 | `ch1` underrun | 종료 코드 |
|---|---:|---:|---:|---:|---:|---:|
| 1 | +0.34 ms (+340 µs) | +0.27 ms (+268 µs) | 0 | 18.96 ms (3168 frames) | 136 (곡 끝 꼬리만) | 0 |
| 2 | +1.93 ms (+1933 µs) | +0.27 ms (+273 µs) | 0 | 18.93 ms (3172 frames) | 136 (곡 끝 꼬리만) | 0 |
| 3 | +0.34 ms (+344 µs) | +1.96 ms (+1961 µs) | 0 | 18.88 ms (3181 frames) | 136 (곡 끝 꼬리만) | 0 |

* 모든 회차에서 `|chart_t0 - music_first| <= 5 ms`(0.27~1.96 ms), 미발견 주소 dispatch 0, 크래시 0, 곡 중간 끊김 0, 표시 간격 약 19 ms 기준을 충족했다.

### 2.3 빌드 및 CTest (임시 계측 제거 후 최종 코드)

* Win64 Ninja Release: 빌드 성공, CTest **37/37 통과** (7.43 s)
* Win64 Ninja Debug: 빌드 성공, CTest **37/37 통과** (9.53 s)
* Win64 Visual Studio 18 2026 Release: 빌드 성공, CTest **37/37 통과** (7.32 s)
* Linux x64 (WSL): 이 실행 환경에서 WSL 호출이 `Wsl/E_ACCESSDENIED`로 차단되어 실행하지 못했다.

## 3. 남은 사항

* 이 실행 환경에서 git 인덱스 잠금(`.git/index.lock`) 및 submodule git 접근이 차단된 경우, 변경은 작업 트리에 그대로 두어 리뷰어/사용자가 커밋할 수 있게 한다.

---

# Work Log: DelayThread Minimum Duration (issue #54, repsp #57)

* Date: 2026-11-21
* Branches: `reDMP1` `i054-music-latency-2`, `repsp` `i57-music-latency-2`
* Design: [20261121-i054-delaythread-minimum.md](../design/20261121-i054-delaythread-minimum.md)
* Work Order: [20261121-i054-delaythread-minimum.md](../work-orders/20261121-i054-delaythread-minimum.md)

## 1. Summary of Changes

### repsp (`third_party/repsp`, branch `i57-music-latency-2`)

1. **Enforced 200 µs minimum in `sceKernelDelayThread` / `sceKernelDelayThreadCB`**
   * `src/hle/kernel/threadman_thread.cpp`: Added `kMinDelayThreadUs = 200u` and clamped requests shorter than 200 µs (including `0`) up to 200 µs in `HleDelayThread` and `HleDelayThreadCB`. Longer requests and other wait primitives (`WaitSema`, `WaitEventFlag`, etc.) remain unchanged.
2. **Added deterministic deadline inspection hook and probe**
   * `include/repsp/runtime/scheduler.h`, `src/runtime/scheduler.cpp`: Recorded `wait_start` on TCBs and exposed `Scheduler::GetThreadWaitDeadlineForTest(KernelUid, bool*, int64_t*)` for deterministic testing.
   * `tools/kernel_sched_probe/main.cpp`: Added Check 16 to deterministically verify that `DelayThread(0)`, `DelayThread(10)`, `DelayThread(5000)`, `DelayThreadCB(0)`, `DelayThreadCB(10)`, and `DelayThreadCB(5000)` register wait deadlines of `{200, 200, 5000, 200, 200, 5000}` µs without relying on wall-clock elapsed time.

### reDMP1 (branch `i054-music-latency-2`)

1. **Updated `README.md`**
   * Updated both Korean and English `Audio` menu descriptions to note that timing is synced at the default `0` offset and the setting is for compensating output-device differences.
2. **Added work log**
   * This document (`docs/work-logs/20261121-i054-delaythread-minimum.md`).

## 2. Verification Results

### 2.1 Deterministic probe (`kernel_sched_probe`)

* Check 16 passed in both Win64 Release and Debug (`kernel_sched_probe passed.`).

### 2.2 Windowed gameplay measurements (3 runs, temporary instrumentation removed afterwards)

* Command: `redmp1.exe roms --renderer gl --max-vblanks 3600 --input-script <keysound-script>` (Release)
* Temporary instrumentation (`chart_t0`: `0x08861704` -> `0x08837808` -> `sceKernelGetSystemTimeWide`; `music_first`: `ch1` inactive-to-active first output) was completely removed after measurement.

| Run | First track `chart_t0 - music_first` | Gameplay track `chart_t0 - music_first` | Dispatches | Mean Present interval | `ch1` underruns | Exit code |
|---|---:|---:|---:|---:|---:|---:|
| 1 | +0.34 ms (+340 µs) | +0.27 ms (+268 µs) | 0 | 18.96 ms (3168 frames) | 136 (stream-end tails only) | 0 |
| 2 | +1.93 ms (+1933 µs) | +0.27 ms (+273 µs) | 0 | 18.93 ms (3172 frames) | 136 (stream-end tails only) | 0 |
| 3 | +0.34 ms (+344 µs) | +1.96 ms (+1961 µs) | 0 | 18.88 ms (3181 frames) | 136 (stream-end tails only) | 0 |

* All 3 runs satisfied `|chart_t0 - music_first| <= 5 ms` (0.27–1.96 ms), 0 unknown-address dispatches, 0 crashes, 0 mid-song underruns, and ~19 ms mean present interval.

### 2.3 Builds and CTest (clean final tree after removing instrumentation)

* Win64 Ninja Release: build succeeded, CTest **37/37 passed** (7.43 s)
* Win64 Ninja Debug: build succeeded, CTest **37/37 passed** (9.53 s)
* Win64 Visual Studio 18 2026 Release: build succeeded, CTest **37/37 passed** (7.32 s)
* Linux x64 (WSL): could not run in this environment because WSL returns `Wsl/E_ACCESSDENIED`.

## 3. Notes

* If git index locking (`.git/index.lock`) or submodule git access is blocked in this sandbox environment, all changes remain clean in the working tree for commit outside the sandbox.
