# 작업 로그: lifter 점검 — 본문 끝 판정 전수 점검 및 호스트 스택 (reDMP1 issue #16)

* 작업 일시: 2026-11-15
* 대상 저장소 / 브랜치: `reDMP1` (`i016-lifter-audit`)
* 관련 설계: [20261115-i016-lifter-audit.md](../design/20261115-i016-lifter-audit.md)
* 관련 작업 지시: [20261115-i016-lifter-audit.md](../work-orders/20261115-i016-lifter-audit.md)

---

## 1. 정적 점검 결과 (`tools/lift_audit.py`)

재배치 이미지(`allegrexrecomp relocate roms/PSP_GAME/SYSDIR/BOOT.BIN 0x08804000`, `3,747,092` B)와 생성 코드(`138`개 파일, `8,785`개 본문, `26,000`개 thunk)를 전수 대조했습니다.

### 1.1. 본문 끝 명령 분류별 개수

| 분류 | 개수 | 비고 |
| --- | ---: | --- |
| `jr_ra` | `3,194` | `psp_ra_is_jump` + `PSP_SP_CHECK` + `return;` |
| `j` | `2,674` | 내부 `goto L_*` 또는 꼬리 호출 + `return;` |
| `other` | `2,188` | 다음 함수/레이블로 fall-through (또는 패딩) |
| `jal` | `414` | 호출 뒤 다음 주소 fall-through 확인 |
| `branch` | `220` | 조건 분기 뒤 not-taken fall-through 확인 |
| `branch_likely` | `53` | 지연 슬롯이 `if` 내부에만 위치 + fall-through 확인 |
| `jr_reg` | `38` | `psp_dispatch(_t)` + `return;` |
| `jalr` | `4` | 패치 `0003` 적용 후 모두 fall-through 확인 |
| **불일치 (`Mismatches`)** | **`0`** | 내부 `falls into` 중 제어 명령 직후 누락도 `0`건 |

* 참고로 생성 코드 내 `/* not discovered */` `5`건 중 `2`건(`0x08804114`, `0x088BF588`)은 미실행 분기의 `jal 0x00000000`이고, `3`건(`0x08894DB4`, `0x088BE2E4`, `0x088C2858`)은 `falls into` 대상이 다른 본문 내부 레이블(`psp_at_*`)인 경우로 실행 시 `psp_dispatch`가 해당 thunk를 정상 해석합니다.

---

## 2. 실행 점검 결과 (`REDMP1_TRACE_GUEST=ON`, `--max-vblanks 2100`)

[`src/recomp_abi/trace.c`](../../src/recomp_abi/trace.c)에 `psp_trace_sp`, `psp_trace_sp_leave`, `psp_trace_sp_call` 및 스레드별 호스트 스택 최고 사용량 측정을 구현해 오프닝부터 플레이 화면(`play-script.txt`, `2100` VBlank)까지 실행했습니다.

| 항목 | MSVC Release (`/O2`) | MSVC Debug (`/Od`) | MSVC Debug + `redmp1_recomp` `/O2` | 판정 |
| --- | --- | --- | --- | --- |
| `SP_CALL` | `0` (`unique=0`) | `0` (`unique=0`) | `0` (`unique=0`) | 호출 경계 스택 누수 **0건** |
| `SP_LEAVE` | `0` (`unique=0`) | `0` (`unique=0`) | `0` (`unique=0`) | 상향 블록 이탈 **0건** |
| `SP_CHECK` (`delta < 0`) | `0` | `0` | `0` | 조기 반환 누수 **0건** |
| `SP_CHECK` (`delta > 0`) | `763,691` (`unique=540`) | `318,423` (`unique=376`) | `337,402` (`unique=376`) | **오탐(분할 에필로그)**: 프레임 할당 후 꼬리 전이/fall-through로 진입한 에필로그(`0x088BF594`, `0x088BCA7C` 등)에서 `_sp0`이 프레임 중간값으로 기록되어 정상 복원(`+16`/`+32`/`+48`)이 양수 델타로 보고됨 (`SP_CALL=0`) |

* 정적 점검 불일치 `0`건 및 실행 중 `SP_CALL`/`SP_LEAVE`/음수 `SP_CHECK` 모두 `0`건이므로 새 `patches/psprecomp/` 패치는 추가하지 않았습니다(`0001`~`0003` 유지).

---

## 3. 호스트 스택 측정표 및 결정 (§3.2)

| 게스트 스레드 (진입 주소) | MSVC Release (`/O2`) | MSVC Debug (`/Od`) | MSVC Debug + `redmp1_recomp` `/O2` |
| --- | ---: | ---: | ---: |
| `module_start` (`0x08804128`) | `48 B` (`0.05 KiB`) | `48 B` (`0.05 KiB`) | `48 B` (`0.05 KiB`) |
| `user_main` (`0x088041CC`) | `445,120 B` (`434.69 KiB`) | **`19,747,152 B` (`18.83 MiB`)** | **`352,784 B` (`344.52 KiB`)** |
| `sound_poll_thread` (`0x08851510`) | `224 B` (`0.22 KiB`) | `1,024 B` (`1.00 KiB`) | `208 B` (`0.20 KiB`) |
| `stream_sound_poll_thread` (`0x08854118`) | `48,176 B` (`47.05 KiB`) | `134,912 B` (`131.75 KiB`) | `15,536 B` (`15.17 KiB`) |
| `0x088575CC` | `336 B` (`0.33 KiB`) | `60,528 B` (`59.11 KiB`) | `6,832 B` (`6.67 KiB`) |
| `ScorePlayerThread` (`0x0886141C`) | `179,456 B` (`175.25 KiB`) | — | — |

* **원인 호출 사슬 (Debug `/Od` 최고치)**: `0x0887FC34 -> 0x088804D0 -> 0x08880590 -> 0x088805F0` (분할된 압축 해제 루프의 꼬리 호출·fall-through가 `/Od`에서 점프로 변환되지 않아 `18.83 MiB`까지 누적).
* **결정 (설계 §3.2 선택지 A 채택)**: [`src/game/dmp1/recomp/CMakeLists.txt`](../../src/game/dmp1/recomp/CMakeLists.txt)를 분리해 `redmp1_recomp` 타깃만 모든 구성에서 최적화(`/O2`, GCC/Clang `-O2`, MSVC Debug의 `/RTC1`/`/Od` 제거)로 컴파일하고 [`CMakeLists.txt`](../../CMakeLists.txt)에서 `/STACK:67108864` 예약을 제거했습니다. Debug 최고치가 `18.83 MiB` → `344.52 KiB` (`< 1 MiB`)로 감소해 기본 스택으로 플레이 화면까지 정상 실행됩니다.

---

## 4. 변경 파일 및 검증 결과

* [`tools/lift_audit.py`](../../tools/lift_audit.py): 표준 라이브러리 기반 본문 끝 판정 정적 점검 도구 추가.
* [`src/recomp_abi/recomp_abi.h`](../../src/recomp_abi/recomp_abi.h), [`src/recomp_abi/trace.c`](../../src/recomp_abi/trace.c): `psp_trace_sp`, `psp_trace_sp_leave`, `psp_trace_sp_call` 및 스레드별 호스트 스택 최고 사용량·호출 사슬 추적 구현.
* [`src/game/dmp1/recomp/CMakeLists.txt`](../../src/game/dmp1/recomp/CMakeLists.txt), [`CMakeLists.txt`](../../CMakeLists.txt), [`src/app/main.cpp`](../../src/app/main.cpp): `redmp1_recomp` 최적화 빌드 분리, `/STACK:67108864` 제거, `REDMP1_TRACE_GUEST` 요약 출력 연결.
* [`docs/guides/running-and-tracing.md`](../guides/running-and-tracing.md): `tools/lift_audit.py` 및 `REDMP1_TRACE_GUEST` 스택 균형·호스트 스택 진단 설명 추가.
* **Win64 CTest**:
  * `Release+FFmpeg` (`%TEMP%\redmp1-build\redmp1-win64-rel-ff`): `35/35` 통과.
  * `Debug` (`%TEMP%\redmp1-build\redmp1-win64-dbg`): `34/34` 통과.
  * `Release no-FFmpeg` (`%TEMP%\redmp1-build\redmp1-win64-rel-noff`): `34/34` 통과.
  * 게임 데이터 없는 구성(`-DREDMP1_GENERATED_DIR=__nonexistent__`): 빌드 및 스모크/ABI 테스트 통과.
* **미수행 (샌드박스 제약)**: `git commit`, WSL/Linux x64 GCC 빌드·CTest 및 CI는 작업 트리에 변경을 둔 채 리뷰어에게 남겼습니다.

---

# Work Log: Lifter Audit — End-of-Body Decisions and Host Stack (reDMP1 issue #16)

* Date: 2026-11-15
* Target repository / branch: `reDMP1` (`i016-lifter-audit`)
* Related design: [20261115-i016-lifter-audit.md](../design/20261115-i016-lifter-audit.md)
* Related work order: [20261115-i016-lifter-audit.md](../work-orders/20261115-i016-lifter-audit.md)

## 1. Static Audit (`tools/lift_audit.py`)

Audited all `8,785` bodies across `138` generated files (`26,000` thunks) against the relocated `BOOT.BIN` image (`3,747,092` B at `0x08804000`): `jr_ra` (`3,194`), `j` (`2,674`), `other` (`2,188`), `jal` (`414`), `branch` (`220`), `branch_likely` (`53`), `jr_reg` (`38`), `jalr` (`4`). **Mismatches found: `0`** (including `0` interior `falls into` sites preceded by control instructions).

## 2. Run-Time Stack Balance & Host Stack Measurements (`--max-vblanks 2100`)

* `SP_CALL` = `0`, `SP_LEAVE` = `0`, negative `SP_CHECK` (`delta < 0`) = `0` in both Release and Debug. All positive `SP_CHECK` reports (`763,691` / `540` unique in Release; `318,423` / `376` in Debug `/Od`; `337,402` / `376` in Debug `/O2`) come from split epilogue bodies entered after frame allocation via tail transfers (`SP_CALL = 0`), so no new `patches/psprecomp/` patch is required.
* **Host stack high-water**:
  * MSVC Release (`/O2`): `user_main` `445,120 B (434.69 KiB)`.
  * MSVC Debug (`/Od`): `user_main` **`19,747,152 B (18.83 MiB)`** (`0x0887FC34->0x088804D0->0x08880590->0x088805F0` decompressor loop).
  * **Option A applied** ([`src/game/dmp1/recomp/CMakeLists.txt`](../../src/game/dmp1/recomp/CMakeLists.txt) compiles `redmp1_recomp` with `/O2` / `-O2` even in Debug; `/STACK:67108864` removed): `user_main` Debug high-water dropped 56× to **`352,784 B (344.52 KiB)`** (`< 1 MiB`), running to gameplay on the default stack.

## 3. Verification

* Win64 `Release+FFmpeg` (`35/35`), `Debug` (`34/34`), `Release no-FFmpeg` (`34/34`), and no-game-data configuration (`-DREDMP1_GENERATED_DIR=__nonexistent__`) all built and passed CTest.
* `git commit` and WSL/Linux GCC build/CTest are left to the reviewer due to sandbox constraints.
