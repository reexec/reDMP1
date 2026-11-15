# 설계: 크래시 분석 — 플레이 중 강제 종료를 기록하고 첫 사례를 분석 (issue #31)

## 1. 요구사항

* 사용자가 창 모드로 플레이하다 프로그램이 강제 종료됐다(2026-11-15). Windows가 남긴 덤프에서 알 수 있는 것:
  * C 런타임의 `abort()`에 의한 종료(fast-fail `0xC0000409`, 인자 7). 잘못된 메모리 접근이 아니라 프로그램이 스스로 끝낸 것이다(처리되지 않은 C++ 예외, `std::terminate`, 명시적 abort 중 하나).
  * 덤프는 작업 디렉터리의 `build\win64-release-ffmpeg` 빌드에서 나왔다(PE 타임스탬프 일치). 크래시 스레드의 스택에 실행 파일 안의 두 함수가 번갈아 여러 번 나온다.
  * Release 빌드에 심볼이 없어 함수 이름은 모른다. 정지 보고는 창 모드에서 보이지 않았다. 게스트 상태와 입력은 남지 않았다.
* 앞으로의 크래시를 분석할 수 있게 repsp의 크래시 기록([repsp issue #30](https://github.com/reexec/repsp/issues/30))을 앱에 연결한다.
* 이미 남은 덤프의 분석을 이어 간다(§4).

## 2. 앱 쪽 기록

* **크래시 디렉터리**: 사용자 데이터 디렉터리 아래 `crashes/`(옵션 `--crash-dir`). 실행마다 `<시각>/`에 크래시 보고, 미니덤프, 그 실행의 로그와 입력 기록이 함께 남는다. 크래시가 없던 실행의 것은 정해진 개수만 남기고 지운다.
* **실행 로그**: 항상 파일로도 쓴다(같은 디렉터리). 정지 보고 포함.
* **입력 기록**: 기본으로 켠다(`--no-input-record`로 끈다). 형식은 `--input-script`와 같아서 크래시 직전까지의 입력을 그대로 재생할 수 있다.
* **추가 정보**: 앱 버전, 렌더러, 배율, 코덱 플러그인, 옵션, 세이브 디렉터리, 게임 식별 결과(디스크 ID와 실행 파일 이름; 사용자 경로는 넣지 않는다).
* **게스트 함수 진입 기록**: 생성 코드의 함수 진입 계측(`PSP_SP_*`나 `psp_trace_enter`처럼 lifter가 이미 내는 지점)을 써서 최근 진입 주소를 repsp의 고리 버퍼에 넣는다. 비용을 재서 정상 빌드에서 켤 수 있는지 정한다(켤 수 없으면 진단 빌드에서만).
* **Release 빌드의 심볼**: MSVC Release에서도 PDB를 만든다(`/Zi`, 링크 `/DEBUG:FULL /OPT:REF /OPT:ICF`). 코드는 바뀌지 않는다. PDB는 저장소에 넣지 않고 빌드 디렉터리에 남는다. 배포물에는 넣지 않는다.

## 3. 분석 도구

* `tools/analyze_crash.py`(표준 라이브러리만): 미니덤프에서 예외 코드와 주소, 스레드별 스택의 반환 주소 후보, 모듈을 읽는다. 같은 빌드의 PDB가 있으면 Windows의 `dbghelp.dll`을 `ctypes`로 불러 주소를 함수 이름으로 바꾼다. 크래시 보고가 있으면 함께 읽어 한 화면에 요약한다.
* 디버거(WinDbg 등)가 없어도 쓸 수 있어야 한다.

## 4. 첫 사례의 분석

리뷰어가 이미 분석한 내용(2026-11-15):

* 같은 구성에 디버그 정보만 더해 다시 빌드하니 `.text`의 크기와 배치가 같고 다른 바이트는 23개(`.rdata` 참조)뿐이었다. 그 PDB로 덤프의 주소를 소스 줄로 바꿀 수 있었다.
* `abort`는 MSVC 런타임의 `__scrt_unhandled_exception_filter` → `terminate()`에서 왔다: **처리되지 않은 C++ 예외**가 게스트 스레드의 꼭대기까지 올라왔다.
* 예외는 `msvcp140.dll`이 던진 `std::system_error`이고, 오류 값은 36(`EDEADLK`, "resource deadlock would occur")이다.
* 던진 곳은 `repsp/src/runtime/scheduler.cpp`의 `Scheduler::ThreadEntry`에서 게스트 실행(`GuardedRun`)이 끝난 직후 `std::unique_lock<std::mutex> lock(mutex_)`이다. MSVC의 `std::mutex`는 **이미 그 뮤텍스를 가진 스레드가 다시 잠그면** 이 예외를 던진다.
* 즉 게스트 스레드가 스케줄러 뮤텍스를 쥔 채로 `GuardedRun` 밖으로 빠져나왔다. repsp의 정지는 Windows에서 SEH 예외(`RaiseException`)로 되감는데, `/EHsc`에서는 이 되감기 중에 C++ 소멸자(`unique_lock`, `lock_guard`)가 실행되지 않는다. 뮤텍스를 쥔 프레임 안에서 정지(또는 네이티브 오류)가 일어나면 잠금이 남는다.
* 스택에는 생성 코드(`dmp1_funcs_058.c`의 두 곳)와 인터럽트 전달 코드(`scheduler.cpp`의 `DeliverPendingInterruptsLocked`)의 흔적이 남아 있다. 어느 정지가 뮤텍스를 쥔 채 일어났는지는 아직 특정하지 못했다(정지 이유는 `GuardedRun`의 결과에만 있고 기록되지 않았다).

이 task에서 할 일:

* 뮤텍스를 쥔 채 정지나 네이티브 오류가 일어날 수 있는 경로를 repsp에서 찾는다(정지를 일으키는 모든 곳과, 스케줄러·HLE의 잠금 범위 안에서 부르는 게스트 코드·콜백). 재현 probe를 만든다. 고치는 방향(예: 정지 전에 잠금을 반드시 놓는 규칙의 강제, 되감기에서 소멸자가 실행되도록 하는 방식, `GuardedRun`이 C++ 예외도 잡아 정지로 바꾸는 것)은 설계 판단이므로 근거와 선택지를 질문으로 올린다.
* 이번에 더하는 크래시 보고에 정지 이유와 "그 시점에 스케줄러 뮤텍스를 가진 스레드"를 넣어, 같은 일이 다시 생기면 경로가 바로 보이게 한다.
* 덤프 파일과 PDB는 저장소에 넣지 않는다.

## 5. 이번 task의 검증

1. 앱에서 일부러 크래시를 일으키는 진단 옵션(`--debug-crash <kind>`, 개발용)으로 각 종료 종류를 일으키면, 크래시 디렉터리에 보고·미니덤프·로그·입력 기록이 생긴다.
2. `tools/analyze_crash.py`가 그 미니덤프를 PDB로 함수 이름까지 풀어 낸다.
3. 입력 기록을 `--input-script`로 재생하면 같은 화면 흐름을 따라간다(플레이 화면까지의 입력으로 확인).
4. 정상 실행의 비용: 플레이 화면까지의 실행에서 표시 프레임 수와 오디오 통계가 기록 장치를 켜기 전과 같은 수준이다.
5. Win64 Debug/Release와 Linux x64 빌드·CTest, CI 통과. 게임 데이터 없는 구성도 경고 없이 빌드된다.

---

# Design: crash analysis — capturing forced terminations during play and analyzing the first one (issue #31)

## 1. Requirements

The program terminated abruptly while the user was playing in windowed mode (2026-11-15). From the dump Windows left: the termination was the C runtime's `abort()` (fast-fail `0xC0000409`, parameter 7) — not a bad memory access but the program ending itself (an unhandled C++ exception, `std::terminate`, or an explicit abort); the dump came from the `build\win64-release-ffmpeg` build in the working directory (matching PE time stamp), and the crashing thread's stack shows two functions inside the executable alternating many times; the Release build has no symbols, so their names are unknown; the stop report was not visible in windowed mode; guest state and input were not recorded. Wire repsp's crash capture ([repsp issue #30](https://github.com/reexec/repsp/issues/30)) into the app so future crashes can be analyzed, and continue the analysis of the existing dump (§4).

## 2. Recording in the app

**Crash directory**: `crashes/` under the per-user data directory (option `--crash-dir`); each run gets `<time>/` holding the crash report, minidump, that run's log and its input recording; runs without a crash are pruned to a set number. **Run log**: always written to a file as well (same directory), stop report included. **Input recording**: on by default (`--no-input-record` turns it off), in the `--input-script` format so the input up to a crash replays as is. **Extra information**: app version, renderer, scale, codec plugin, options, save directory, game identification (disc ID and executable name; no user paths). **Guest function entries**: the generated code's function entry instrumentation (points the lifter already emits, such as `PSP_SP_*` or `psp_trace_enter`) feeds recent entry addresses into repsp's ring buffer; its cost is measured to decide whether it can stay on in normal builds (otherwise diagnostic builds only). **Symbols for Release builds**: MSVC Release also produces a PDB (`/Zi`, linking with `/DEBUG:FULL /OPT:REF /OPT:ICF`); the code does not change; the PDB stays in the build directory, out of the repository and out of release packages.

## 3. Analysis tool

`tools/analyze_crash.py` (standard library only) reads from a minidump the exception code and address, return address candidates on each thread's stack, and the modules; when the same build's PDB is present it loads Windows' `dbghelp.dll` through `ctypes` to turn addresses into function names; when a crash report is present it reads that too and summarizes everything on one screen. It must work without a debugger (WinDbg and the like).

## 4. Analyzing the first case

Already analyzed by the reviewer (2026-11-15): rebuilt in the same configuration with only debug information added, the executable's `.text` had the same size and layout and only 23 differing bytes (references into `.rdata`), so its PDB turned the dump's addresses into source lines. The `abort` came from the MSVC runtime's `__scrt_unhandled_exception_filter` calling `terminate()`: an **unhandled C++ exception** reached the top of a guest thread. The exception is a `std::system_error` thrown by `msvcp140.dll` with error value 36 (`EDEADLK`, "resource deadlock would occur"). It was thrown at `std::unique_lock<std::mutex> lock(mutex_)` in `Scheduler::ThreadEntry` (`repsp/src/runtime/scheduler.cpp`), right after guest execution (`GuardedRun`) ended; MSVC's `std::mutex` throws this when **a thread that already holds the mutex locks it again**. So the guest thread came out of `GuardedRun` still holding the scheduler mutex. repsp's stops unwind with an SEH exception (`RaiseException`) on Windows, and under `/EHsc` C++ destructors (`unique_lock`, `lock_guard`) do not run during that unwind; a stop (or native fault) inside a frame holding the mutex leaves it locked. The stack still shows traces of generated code (two places in `dmp1_funcs_058.c`) and of interrupt delivery (`DeliverPendingInterruptsLocked` in `scheduler.cpp`). Which stop happened with the mutex held is not yet identified (the stop reason lives only in `GuardedRun`'s result and was not recorded).

In this task: find the paths in repsp where a stop or native fault can happen while the mutex is held (every place that raises a stop, and guest code or callbacks called within the scheduler's or HLE's lock scopes), and build a reproducing probe. How to fix it (enforcing that locks are always released before a stop, unwinding in a way that runs destructors, or having `GuardedRun` catch C++ exceptions and turn them into stops) is a design decision, so the evidence and options are raised as a question. The crash report added here records the stop reason and "the thread holding the scheduler mutex at that moment", so a recurrence shows the path at once. Dump files and PDBs are never committed.

## 5. Verification for this task

1. A diagnostic option that deliberately crashes the app (`--debug-crash <kind>`, for development) produces, for each kind of termination, a report, minidump, log and input recording in the crash directory.
2. `tools/analyze_crash.py` resolves that minidump down to function names with the PDB.
3. Replaying the input recording with `--input-script` follows the same flow of screens (checked with the input up to gameplay).
4. Cost in normal runs: over the run to gameplay, presented frames and audio statistics stay at the level from before the capture was switched on.
5. Win64 Debug/Release and Linux x64 builds and CTest, and CI, pass; the configuration without game data builds without warnings.
