# 설계: 트램펄린 뒤의 스택 안전망 정리 (issue #39)

## 1. 요구사항

* issue #32·#33 동안 둔 두 안전망이 있다. 하나는 repsp가 게스트 스레드마다 잡는 64 MiB 호스트 스택 예약(`Scheduler::kDefaultGuestHostStackReserveBytes`)이다. 다른 하나는 reDMP1이 Debug 구성에서도 생성 코드를 `/O2`·`-O2`로 빌드하는 설정(`src/game/dmp1/recomp/CMakeLists.txt`, `/RTC1`·`/Od`·`/Ob0` 제거 포함)이다.
* 트램펄린(v0.0.19) 뒤에 측정한 값: 생성 코드를 최적화하지 않아도 게스트 스레드의 호스트 스택 최고값이 실행 길이와 무관하게 20 KiB 이하로 일정하다.
* **사용자 결정(2026-11-17)**: 두 안전망을 줄인다.

## 2. 변경

* **repsp**: 기본 예약을 64 MiB에서 **8 MiB**로 줄인다(Linux 주 스레드의 흔한 기본값과 같다). 설정 API(`SetGuestHostStackReserveBytes`)와 0이면 플랫폼 기본값이라는 계약은 그대로다. 이 값은 게임과 무관한 기본값이므로 repsp 문서(설계·`ARCHITECTURE.md`나 해당 헤더 주석)에 근거를 적는다. 근거는 "호출이 게스트 호출 깊이만큼만 쌓이는 실행 모델을 전제로 한다"는 것이다. 특정 게임의 측정값은 쓰지 않는다. 스택 넘침의 정지 보고(`Guest hardware fault: Stack overflow`)와 진단 빌드의 최고값 통계는 그대로 둔다.
* **reDMP1**:
  * Debug 구성에서 생성 코드를 강제로 최적화하던 설정을 없앤다. Debug는 다른 코드와 같은 플래그(`/Od /RTC1`, `-O0 -g`)로 빌드한다. Release·RelWithDebInfo는 지금처럼 최적화한다.
  * 측정용 옵션 `REDMP1_RECOMP_UNOPT`는 Debug가 같은 일을 하므로 없앤다. 문서와 스크립트의 언급도 고친다.
  * repsp를 새 버전으로 고정한다.

## 3. 측정과 검증

1. **Debug 스택**: 표준 Debug 플래그(생성 코드 비최적화, `/RTC1` 포함)와 `REDMP1_TRACE_GUEST=ON`으로 플레이 화면까지 가서 곡 구간을 길게(3,600 vblank 이상) 돌린다. 모든 게스트 스레드의 호스트 스택 최고값을 표로 남긴다. 가장 큰 값의 8배가 8 MiB 안에 들어야 한다. 들지 않으면 멈추고 보고한다(질문).
2. **Release**: 플레이 화면까지 입력 스크립트 실행 3회가 같은 화면에 이르고, 미발견 주소 dispatch 0건, 크래시 0건이다. 스택 최고값이 1과 같은 범위다.
3. **Debug 속도**: 비최적화 Debug 빌드가 플레이 화면에 이르는지와 표시 간격 통계를 적는다(느려지는 것은 허용; 기록만 한다).
4. **크래시 기록**: `--debug-crash` 다섯 종류가 보고를 남긴다. 새 예약에서도 스택 넘침이 정지 보고로 잡히는지 확인한다. repsp 쪽 probe(기존 `kernel_sched_probe` 등)가 예약과 최고값을 그대로 검증하는지 보고, 기본값을 직접 단정하는 테스트가 있으면 새 값에 맞춘다.
5. Win64 Debug/Release(Visual Studio 생성기 포함)와 Linux x64 Debug/Release 빌드·CTest(repsp, reDMP1). 게임 데이터 없는 구성도 경고 없이 빌드된다. Visual Studio Debug에서 `/RTC1`이 생성 코드와 함께 문제없이 빌드되는지 확인한다.
6. 사용자가 Release 창 모드로 곡 하나를 끝까지 플레이한다(사용자 확인).

## 4. 기록

* repsp: 설계 사본 또는 해당 문서에 기본값과 근거. 릴리스 노트는 리뷰어가 쓴다.
* reDMP1: 작업 로그 `docs/work-logs/20261117-i039-stack-safety-nets.md`(스택표, Debug 속도, 질문). 빌드 안내(`docs/guides/` 등)에서 Debug 생성 코드 최적화를 언급하는 곳.

---

# Design: reducing the stack safety nets after the trampoline (issue #39)

## 1. Requirements

Two safety nets were added during issues #32 and #33. One is the 64 MiB host stack that repsp reserves per guest thread (`Scheduler::kDefaultGuestHostStackReserveBytes`). The other is reDMP1 building the generated code with `/O2`/`-O2` even in Debug (`src/game/dmp1/recomp/CMakeLists.txt`, including stripping `/RTC1`, `/Od` and `/Ob0`). Measured after the trampoline (v0.0.19): even with unoptimized generated code, every guest thread's host stack high-water stays at 20 KiB or less, independent of run length. **User decision (2026-11-17)**: reduce both.

## 2. Changes

**repsp**: the default reservation goes from 64 MiB to **8 MiB** (the usual Linux main-thread default). The setter (`SetGuestHostStackReserveBytes`) and the contract that 0 means the platform default stay. Since this is a game-independent default, the reasoning goes into repsp's documents (design, `ARCHITECTURE.md` or the header comment): the default assumes an execution model where host calls nest only as deep as guest calls. No particular game's measurements go there. The stack-overflow stop report (`Guest hardware fault: Stack overflow`) and the diagnostic high-water statistics stay.

**reDMP1**:
- Drop the forced optimization of the generated code in Debug. Debug builds it with the same flags as other code (`/Od /RTC1`, `-O0 -g`); Release and RelWithDebInfo stay optimized.
- Remove the measurement option `REDMP1_RECOMP_UNOPT`, since Debug now does the same, and update the documents and scripts that mention it.
- Pin repsp to the new version.

## 3. Measurement and verification

1. **Debug stack**: with standard Debug flags (generated code unoptimized, `/RTC1` included) and `REDMP1_TRACE_GUEST=ON`, run to gameplay and let the song stretch run long (3,600 vblanks or more). Tabulate every guest thread's host stack high-water. Eight times the largest must fit in 8 MiB; if it does not, stop and report (question).
2. **Release**: three scripted runs to gameplay reach the same screens, with zero undiscovered-address dispatches and zero crashes. Stack high-water is in the same range as in item 1.
3. **Debug speed**: record whether the unoptimized Debug build reaches gameplay, and its presentation interval statistics. Slower is acceptable; this is recorded only.
4. **Crash capture**: the five `--debug-crash` kinds leave reports, and a stack overflow is still caught as a stop report with the new reservation. Check that repsp's probes (`kernel_sched_probe` and others) still verify reservation and high-water; adjust any test that asserts the old default directly.
5. Win64 Debug/Release (including the Visual Studio generator) and Linux x64 Debug/Release builds and CTest (repsp and reDMP1). The configuration without game data builds without warnings. Visual Studio Debug builds the generated code with `/RTC1` without problems.
6. The user plays one song to the end in Release, windowed (the user's check).

## 4. Record

- **repsp**: the default and its reasoning in a design copy or the relevant document. The reviewer writes the release notes.
- **reDMP1**: work log `docs/work-logs/20261117-i039-stack-safety-nets.md` (stack table, Debug speed, questions), and any build guide (`docs/guides/` or elsewhere) that mentions optimizing generated code in Debug.
