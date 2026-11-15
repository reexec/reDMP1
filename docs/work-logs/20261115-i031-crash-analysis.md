# 작업 로그: 크래시 기록 앱 통합, Release PDB 및 덤프 분석 도구 (issue #31)

## 1. 작업 개요

* 대상 저장소/브랜치: `reDMP1` (`i031-crash-analysis`, `v0.0.16` 위 리베이스 상태)
* 선행: `third_party/repsp` (`i030-crash-capture`)
* 목표: 앱 크래시 기록·실행 로그·입력 녹화 통합, MSVC Release PDB 생성, `tools/analyze_crash.py` 덤프 분석기 구현, 일반 실행 비용 측정, 가이드·작업 로그 작성.

## 2. 구현 내용

1. **MSVC Release PDB 생성 (`CMakeLists.txt`, `src/game/dmp1/recomp/CMakeLists.txt`)**:
   * `redmp1`, `redmp1_game`, `redmp1_recomp_abi`, `redmp1_recomp`의 Release 구성에 `/Z7`을 추가하고 `redmp1` 링크 옵션에 `/DEBUG:FULL /OPT:REF /OPT:ICF`를 추가해 `redmp1.pdb`(약 38 MB)가 함께 생성되도록 했다.
2. **게스트 함수 진입 이력 연결 (`src/recomp_abi/exec_trace.cpp`, `src/recomp_abi/trace.c`)**:
   * `redmp1_note_guest_entry(uint32_t)` C 브리지를 추가해 `REDMP1_TRACE_GUEST=ON`일 때 `psp_trace_enter`가 `repsp::runtime::NoteGuestEntry`를 호출하도록 연결했다.
3. **앱 통합 (`src/app/main.cpp`)**:
   * `--crash-dir <경로>`(기본값 `<user_data>/crashes/`), `--no-input-record`(기본 활성), `--debug-crash <kind>`(`access_violation`, `abort`, `cpp_exception`, `unjoined_thread`, `mutex_held_stop`) 옵션을 추가했다.
   * 실행마다 `<crash-dir>/<YYYYMMDD-HHMMSS>/` 폴더를 만들고 `SetLogFile("run.log")`, `InstallCrashHandler`, `RegisterRuntimeCrashProvider`, `SetCrashGuestMemoryRange`, `StartInputRecording("input_recording.txt")`, `SetCrashReportExtra`(앱 버전, 디스크 ID/버전/실행 파일/SHA-1, 렌더러, 배율, 코덱 플러그인, CLI 옵션 — 사용자 계정 경로 제외)를 등록한다.
   * 정상 종료 시 크래시 보고서나 `.dmp`가 없는 실행 폴더는 자동 정리한다(`PruneCrashBaseDirectory`).
4. **미니덤프·보고서 분석기 (`tools/analyze_crash.py`)**:
   * Python 표준 라이브러리와 Windows `dbghelp.dll`(`ctypes`)만 사용해 크래시 폴더, `crash_report_*.txt`, 또는 단일 `.dmp`(Windows `%LOCALAPPDATA%\CrashDumps\*.dmp` 포함)를 분석한다.
   * `ModuleListStream`·`ThreadListStream`·`ExceptionStream`·`MemoryListStream`/`Memory64ListStream`을 파싱해 PDB 심볼/소스 줄 번호, `RIP`/`RSP`/`RBP`, 16 KiB 스택 반환 주소 스캔, C++ 예외(`0xE06D7363`) RTTI/`what()` 복원을 출력한다.
5. **문서**:
   * [crash-reporting.md](../guides/crash-reporting.md), [guides/README.md](../guides/README.md), [running-and-tracing.md](../guides/running-and-tracing.md)를 추가·갱신했다.

## 3. 일반 실행 비용 측정 및 결정 (`REDMP1_TRACE_GUEST`)

* `--headless --max-vblanks 600` (10초 VBlank 기준) 비교 결과:
  * `REDMP1_TRACE_GUEST=OFF`: 벽시계 `10.06s`, `Present: 296 frames (interval mean=33.82 ms, max=100.06 ms)`, `Audio: underruns=34`, `host_ur=12(short=3072)`, 종료 후 빈 크래시 폴더 0개(`remaining_crash_dirs=0`).
  * `REDMP1_TRACE_GUEST=ON` (`NoteGuestEntry` 포함): 벽시계 `10.10s`, `Present: 296 frames (interval mean=33.82 ms, max=99.71 ms)`, `Audio: underruns=34`, `host_ur=11(short=2816)`.
* 600 VBlank 구간에서 프레임 수·오디오 언더런에 유의미한 저하는 없으나, 모든 함수 프롤로그 호출 오버헤드 0을 유지하기 위해 기본 Release 빌드는 `REDMP1_TRACE_GUEST=OFF`로 두고 진단 빌드에서 `-DREDMP1_TRACE_GUEST=ON`으로 켜도록 유지했다.

## 4. 검증 결과

* **`--debug-crash` 5종(Release)**: `access_violation`(0.17s), `abort`(0.14s), `cpp_exception`(0.15s), `unjoined_thread`(0.15s), `mutex_held_stop`(0.15s) 모두 `crash_report_*.txt` + `crash_dump_*.dmp`(~2.9 MB) + `run.log` + `input_recording.txt` 생성 확인. (조사 중 `repsp` `MinidumpFilterCallback`의 `RemoveMemoryCallback` 무한 루프를 발견해 함께 수정함.)
* **`tools/analyze_crash.py` PDB 심볼 복원**: `access_violation`에서 `redmp1.exe+0xDC67` (`TriggerDebugCrash+0xA7 (main.cpp:525)`), `mutex_held_stop`에서 `HandleFatalTermination`·`CrashUnhandledExceptionFilter`·`RunBoot` 등 PDB 심볼/줄 번호 복원 확인.
* **`--input-script` 재생**: `280 START`, `500 CROSS` 스크립트로 `--max-vblanks 600` 실행 시 `rc=0`, `Present: 208 frames`, `GE: 95 lists, 23278 commands`로 메뉴 진입 확인.
* **Win64 Debug CTest**: `35/35` 통과 (`9.22 sec`).
* **Win64 Release CTest (수정 후)**: `35/35` 통과 (`6.62 sec`, `mutex_held_stop` 경계 회수 반영 후 재실행 포함).
* **게임 데이터 없는 빌드(`REDMP1_GENERATED_DIR` 비움) CTest**: `32/32` 통과 (`19.61 sec`).

---

# Work Log: App Crash Integration, Release PDBs, and Dump Analyzer (issue #31)

## 1. Overview

* Target repo/branch: `reDMP1` (`i031-crash-analysis`, rebased onto `v0.0.16`)
* Prerequisite: `third_party/repsp` (`i030-crash-capture`)
* Goal: Integrate per-run crash reports/minidumps/logs/input recording into `redmp1`, enable MSVC Release PDB generation, implement `tools/analyze_crash.py`, measure normal-run cost, and update guides.

## 2. Changes

1. **MSVC Release PDBs (`CMakeLists.txt`, `src/game/dmp1/recomp/CMakeLists.txt`)**: Added `/Z7` for Release builds and `/DEBUG:FULL /OPT:REF /OPT:ICF` on `redmp1`, producing `redmp1.pdb` (~38 MB).
2. **Guest Entry Tracing (`src/recomp_abi/exec_trace.cpp`, `trace.c`)**: Bridged `psp_trace_enter` to `repsp::runtime::NoteGuestEntry` under `REDMP1_TRACE_GUEST=ON`.
3. **App Integration (`src/app/main.cpp`)**: Added `--crash-dir`, `--no-input-record`, `--debug-crash <kind>`, per-run `<YYYYMMDD-HHMMSS>/` directories, `SetLogFile`, `InstallCrashHandler`, `RegisterRuntimeCrashProvider`, `SetCrashGuestMemoryRange`, `StartInputRecording`, `SetCrashReportExtra` (sanitized metadata without user paths), and clean-run pruning.
4. **Dump Analyzer (`tools/analyze_crash.py`)**: Stdlib + `dbghelp.dll` via `ctypes` analyzer supporting run directories, `crash_report_*.txt`, and standalone `.dmp` files (including `%LOCALAPPDATA%\CrashDumps\*.dmp`).
5. **Docs**: Added [crash-reporting.md](../guides/crash-reporting.md) and updated [guides/README.md](../guides/README.md) and [running-and-tracing.md](../guides/running-and-tracing.md).

## 3. Normal-Run Cost (`REDMP1_TRACE_GUEST`)

* `--headless --max-vblanks 600`:
  * `OFF`: `10.06s`, `Present: 296 frames (mean=33.82 ms)`, `Audio: underruns=34, host_ur=12`, `remaining_crash_dirs=0`.
  * `ON` (with `NoteGuestEntry`): `10.10s`, `Present: 296 frames (mean=33.82 ms)`, `Audio: underruns=34, host_ur=11`.
* Overhead is negligible in 600 VBlanks, and keeping `REDMP1_TRACE_GUEST=OFF` by default preserves zero prologue cost in standard Release builds while enabling opt-in tracing.

## 4. Verification

* `--debug-crash` (all 5 kinds, Release): each produced `crash_report_*.txt`, `crash_dump_*.dmp` (~2.9 MB), `run.log`, and `input_recording.txt` in `0.14s–0.17s` (also caught and fixed an infinite loop in `repsp` `MinidumpFilterCallback` `RemoveMemoryCallback`).
* `tools/analyze_crash.py`: resolved PDB symbol/line (`TriggerDebugCrash+0xA7 (main.cpp:525)`, `HandleFatalTermination`, `RunBoot`).
* `--input-script` replay to menu (`600` VBlanks): `rc=0`, `Present: 208 frames`, `GE: 95 lists, 23278 commands`.
* Win64 Debug CTest: `35/35` passed (`9.22 sec`).
* Win64 Release CTest: `35/35` passed (`6.62 sec`).
* No-game-data Release CTest: `32/32` passed (`19.61 sec`).
