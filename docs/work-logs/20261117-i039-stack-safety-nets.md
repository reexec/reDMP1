# 2026-11-17 작업 로그: 스택 안전망 정리 (#39, repsp #37)

* 관련 설계: [20261117-i039-stack-safety-nets.md](../design/20261117-i039-stack-safety-nets.md)
* 관련 작업 지시: [20261117-i039-stack-safety-nets.md](../work-orders/20261117-i039-stack-safety-nets.md)

## 1. 구현 요약

1. **`repsp` 기본 게스트 호스트 스택 예약 축소 (`64 MiB` → `8 MiB`, `third_party/repsp`)**
   * `third_party/repsp/include/repsp/runtime/scheduler.h`: `Scheduler::kDefaultGuestHostStackReserveBytes`를 `64 MiB`에서 `8 MiB`(`8ull * 1024ull * 1024ull`, Linux 주 스레드의 흔한 기본값)로 줄이고, 호스트 호출이 게스트 호출 깊이만큼만 중첩되는 실행 모델을 전제로 하며 `SetGuestHostStackReserveBytes(0)`으로 플랫폼 기본값을 선택할 수 있다는 근거 주석을 추가했다.
   * `third_party/repsp/ARCHITECTURE.md`: 한국어/영문 계약 섹션에 8 MiB 기본 예약과 전제 근거를 기록했다(특정 게임의 측정값은 포함하지 않음).
   * `third_party/repsp/tools/kernel_sched_probe/main.cpp`: 테스트 #13의 주석 및 설정값 변경 검증(`16 MiB` 설정 후 기본값 복원)을 새 기본값에 맞췄고, 테스트 #14(`kAddrStackOverflow`)를 추가해 8 MiB 기본 예약 초과 시 `GuardedRun`이 `"Stack overflow"` 정지 보고로 포착함을 검증했다.
2. **`reDMP1` Debug 생성 코드 강제 최적화 및 `REDMP1_RECOMP_UNOPT` 제거**
   * [src/game/dmp1/recomp/CMakeLists.txt](../../src/game/dmp1/recomp/CMakeLists.txt): MSVC Debug/RelWithDebInfo에서 `/RTC1`, `/Od`, `/Ob0`를 지우고 `/O2`/`-O2`를 강제하던 설정과 `REDMP1_RECOMP_UNOPT` 옵션을 제거했다. 이제 Debug는 다른 코드와 같은 표준 플래그(`/Od /RTC1`, `-O0 -g`)로 빌드되고 Release/RelWithDebInfo는 최적화된 상태를 유지한다.
   * `docs/guides/`, `README.md`, `ARCHITECTURE.md`를 점검해 Debug 생성 코드 강제 최적화나 `REDMP1_RECOMP_UNOPT`를 안내하는 기존 문구가 없음을 확인했다.

## 2. 측정 및 검증 결과

### 2.1 Debug 비최적화(`/Od /RTC1`) + `REDMP1_TRACE_GUEST=ON` 호스트 스택 최고 사용량

표준 Debug 빌드(`-DCMAKE_BUILD_TYPE=Debug -DREDMP1_TRACE_GUEST=ON`, 생성 코드 `/Od /RTC1`)로 동일한 입력 스크립트(타이틀 → 메뉴 → 곡 진입, `ScorePlayerThread` 기동 확인)를 `--headless --no-input-record --max-vblanks 3600`까지 실행해 모든 게스트 스레드의 호스트 스택 최고 사용량을 측정했다.

| 스레드 (`uid`) | 예약 크기 | OS 호스트 스택 최고 사용량 | 게스트 호출 체인 최대 | 8배 (`8 × high_water`) | 8 MiB 이내 여부 |
|---|---|---|---|---|---|
| `module_start` (`0x4`) | 8,388,608 B (8 MiB) | 12,288 B (12 KiB) | 48 B | 96 KiB | 예 |
| `user_main` (`0x6`) | 8,388,608 B (8 MiB) | **28,672 B (28 KiB)** | 16,992 B (16.59 KiB) | **224 KiB** | 예 (2.67%) |
| `sound_poll_thread` (`0x11`) | 8,388,608 B (8 MiB) | 16,384 B (16 KiB) | 672 B | 128 KiB | 예 |
| `stream_sound_poll_thread` (`0x13`) | 8,388,608 B (8 MiB) | 16,384 B (16 KiB) | 2,736 B (2.67 KiB) | 128 KiB | 예 |
| `ScorePlayerThread` (`0x1d`) | 8,388,608 B (8 MiB) | 16,384 B (16 KiB) | 2,368 B (2.31 KiB) | 128 KiB | 예 |

* 가장 큰 값(`28 KiB`)의 8배는 `224 KiB`로 8 MiB(`8,192 KiB`) 안에 여유 있게 들어간다.
* **Debug 속도 기록**: 비최적화 Debug(`/Od /RTC1`)에서도 `ScorePlayerThread`까지 정상 도달(`Dispatches: 0`, `Exit code: 0`)했으며, `Present: 655 frames (interval mean=91.69 ms, max=6907.01 ms)`를 기록했다.

### 2.2 Release 3회 실행 및 크래시 기록

* **Release 3회 실행** (`--headless --max-vblanks 2400`, 동일 입력 스크립트): 3회 모두 `ScorePlayerThread`에 도달하고 `Exit code: 0`, `Dispatches: 0`, 크래시 0건을 확인했다(`Present`: #1 `1161 frames / mean 34.67 ms`, #2 `1312 frames / mean 30.53 ms`, #3 `1269 frames / mean 31.55 ms`).
* **`--debug-crash` 5종**: `access_violation`, `abort`, `cpp_exception`, `unjoined_thread`, `mutex_held_stop` 5건 모두 `.dmp`, `crash_report_*.txt`, `run.log`가 정상 생성되었다.
* **스택 넘침 정지 보고**: `kernel_sched_probe` 테스트 #14에서 8 MiB 기본 예약 초과 시 `GuardedRun`이 `"Stack overflow"` 정지 보고로 포착함을 확인했다.

### 2.3 빌드 및 CTest 검증

* `repsp` 단독 Release (Ninja): **22/22 통과** (4.18 s)
* `repsp` 단독 Debug (Ninja): **22/22 통과** (5.62 s)
* `reDMP1` Win64 Debug (Ninja, `/Od /RTC1`): **35/35 통과** (52.61 s)
* `reDMP1` Win64 Release (Ninja): **35/35 통과** (6.98 s)
* 게임 데이터 없는 구성 (Release Ninja, 빈 `REDMP1_GENERATED_DIR`): 경고 없이 빌드 완료, **7/7 통과** (`recomp_abi_probe`, `app_smoke_*`, 0.28 s)

## 3. 환경 참고

* 샌드박스 제약으로 `git commit`, `git -C third_party/repsp`, WSL Linux 빌드는 실행하지 못해 변경 사항을 `reDMP1`과 `third_party/repsp` 작업 트리에 두었으며, Visual Studio 생성기(MSBuild)는 이전 작업과 동일하게 샌드박스 내 `allegrexrecomp` 단계에서 정지해 Ninja로 Debug(`/Od /RTC1`)·Release 빌드를 검증했다.

---

# 2026-11-17 Work Log: Reducing the Stack Safety Nets (#39, repsp #37)

* Design: [20261117-i039-stack-safety-nets.md](../design/20261117-i039-stack-safety-nets.md)
* Work Order: [20261117-i039-stack-safety-nets.md](../work-orders/20261117-i039-stack-safety-nets.md)

## 1. Implementation Summary

1. **Lowered `repsp` Default Guest Host Stack Reservation (`64 MiB` → `8 MiB`, `third_party/repsp`)**
   * `third_party/repsp/include/repsp/runtime/scheduler.h`: lowered `Scheduler::kDefaultGuestHostStackReserveBytes` from `64 MiB` to `8 MiB` (`8ull * 1024ull * 1024ull`, matching the typical Linux main-thread default), documenting that it assumes an execution model where host calls nest only as deep as guest calls and that `SetGuestHostStackReserveBytes(0)` selects the platform default.
   * `third_party/repsp/ARCHITECTURE.md`: documented the 8 MiB default reservation and its rationale in both Korean and English sections (without any game-specific measurements).
   * `third_party/repsp/tools/kernel_sched_probe/main.cpp`: updated test #13 to exercise `SetGuestHostStackReserveBytes(16 MiB)` and restore the new default, and added test #14 (`kAddrStackOverflow`) verifying that overflowing the 8 MiB default reservation is still caught by `GuardedRun` as `"Stack overflow"`.
2. **Removed Forced Debug Optimization and `REDMP1_RECOMP_UNOPT` in `reDMP1`**
   * [src/game/dmp1/recomp/CMakeLists.txt](../../src/game/dmp1/recomp/CMakeLists.txt): removed the `/RTC1` / `/Od` / `/Ob0` stripping, forced `/O2` / `-O2` in Debug, and the `REDMP1_RECOMP_UNOPT` option. Debug now builds generated code with the same standard flags (`/Od /RTC1`, `-O0 -g`) as all other code, while Release and RelWithDebInfo stay optimized.
   * Checked `docs/guides/`, `README.md`, and `ARCHITECTURE.md`; no existing entries mentioned optimizing generated code in Debug or `REDMP1_RECOMP_UNOPT`.

## 2. Measurements and Verification

### 2.1 Debug Unoptimized (`/Od /RTC1`) + `REDMP1_TRACE_GUEST=ON` Host Stack High-Water

Measured with a standard Debug build (`-DCMAKE_BUILD_TYPE=Debug -DREDMP1_TRACE_GUEST=ON`, generated code compiled with `/Od /RTC1`) running the gameplay input script (`--headless --no-input-record --max-vblanks 3600`, `ScorePlayerThread` spawned):

| Thread (`uid`) | Reserve | OS Host Stack High-Water | Max Guest Call Chain | `8 × high_water` | Within 8 MiB? |
|---|---|---|---|---|---|
| `module_start` (`0x4`) | 8,388,608 B (8 MiB) | 12,288 B (12 KiB) | 48 B | 96 KiB | Yes |
| `user_main` (`0x6`) | 8,388,608 B (8 MiB) | **28,672 B (28 KiB)** | 16,992 B (16.59 KiB) | **224 KiB** | Yes (2.67%) |
| `sound_poll_thread` (`0x11`) | 8,388,608 B (8 MiB) | 16,384 B (16 KiB) | 672 B | 128 KiB | Yes |
| `stream_sound_poll_thread` (`0x13`) | 8,388,608 B (8 MiB) | 16,384 B (16 KiB) | 2,736 B (2.67 KiB) | 128 KiB | Yes |
| `ScorePlayerThread` (`0x1d`) | 8,388,608 B (8 MiB) | 16,384 B (16 KiB) | 2,368 B (2.31 KiB) | 128 KiB | Yes |

* `8 × 28 KiB = 224 KiB`, well within `8 MiB` (`8,192 KiB`).
* **Debug speed**: unoptimized Debug (`/Od /RTC1`) reached `ScorePlayerThread` cleanly (`Dispatches: 0`, `Exit code: 0`) with `Present: 655 frames (interval mean=91.69 ms, max=6907.01 ms)`.

### 2.2 Release 3 Scripted Runs and Crash Reporting

* **3 Release runs** (`--headless --max-vblanks 2400`, same input script): all 3 reached `ScorePlayerThread` with `Exit code: 0`, `Dispatches: 0`, and 0 crashes (`Present`: #1 `1161 frames / mean 34.67 ms`, #2 `1312 frames / mean 30.53 ms`, #3 `1269 frames / mean 31.55 ms`).
* **`--debug-crash` 5 kinds**: `access_violation`, `abort`, `cpp_exception`, `unjoined_thread`, and `mutex_held_stop` all generated `.dmp`, `crash_report_*.txt`, and `run.log`.
* **Stack overflow stop report**: `kernel_sched_probe` test #14 verified that overflowing the 8 MiB default reservation is caught as `"Stack overflow"` under `GuardedRun`.

### 2.3 Build and CTest Verification

* `repsp` standalone Release (Ninja): **22/22 passed** (4.18 s)
* `repsp` standalone Debug (Ninja): **22/22 passed** (5.62 s)
* `reDMP1` Win64 Debug (Ninja, `/Od /RTC1`): **35/35 passed** (52.61 s)
* `reDMP1` Win64 Release (Ninja): **35/35 passed** (6.98 s)
* No-game-data config (Release Ninja, empty `REDMP1_GENERATED_DIR`): built without warnings, **7/7 passed** (`recomp_abi_probe`, `app_smoke_*`, 0.28 s)

## 3. Environment Notes

* Due to sandbox restrictions, `git commit`, `git -C third_party/repsp`, and WSL Linux builds were not run; changes remain in the working trees of `reDMP1` and `third_party/repsp`. As in prior tasks, the Visual Studio generator (MSBuild) stalled in the sandbox during `allegrexrecomp`, so Win64 Debug (`/Od /RTC1`) and Release were verified via Ninja.
