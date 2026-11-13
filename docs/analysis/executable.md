# 실행 파일 분석: DJMAX Portable (ULKS-46050)

이 문서는 원본 UMD의 기본 실행 파일(`PSP_GAME/SYSDIR/BOOT.BIN` 및 `EBOOT.BIN`)에 대해 `allegrexrecomp` 분석 도구로 확인한 사실을 기록한다.

---

## 1. 모듈 신원 및 컨테이너

| 항목 | 값 |
|---|---|
| 모듈 이름 | `djmax` |
| 모듈 버전 | 1.1 |
| 모듈 속성 | `0x0000` (User-mode PRX) |
| 실행 형식 | ELF32 MIPS LE (PRX, relocatable) |
| 파일 크기 | 1,463,072 바이트 |
| 엔트리 포인트 | `0x00000128` |
| GP ($gp) 레지스터 | `0x00026660` |
| 내보내기 함수 | 1 (`module_start` / `0x00000128`) |

* **확인됨**: 원본 실행 파일은 재배치 가능한 사용자 모드 PRX 모듈이다.
* **확인됨**: `BOOT.BIN`은 평문 ELF32(`\x7fELF`)이며, `EBOOT.BIN`은 Sony `~PSP` 암호화 헤더로 감싸진 형태이다.

---

## 2. 세그먼트 및 섹션 배치

| 세그먼트 | 가상 주소 | 파일 크기 | 메모리 크기 | 플래그 | 내용 |
|---|---|---|---|---|---|
| Segment 0 | `0x00000000` | 1,028,240 바이트 | 1,028,240 바이트 | `rwx` | 코드(`.text`), 읽기 전용 데이터, 임포트 스텁, 모듈 정보 |
| Segment 1 | `0x000FB090` | 124,516 바이트 | 2,718,852 바이트 | `rw-` | 초기화된 데이터(`.data`), 미초기화 데이터(`.bss`) |

* **확인됨**: BSS 영역 크기는 2,594,336 바이트(`2,718,852 - 124,516`)이다.
* **확인됨**: 섹션 수는 총 38개이며, `.text` 섹션은 오프셋 `0x00000040`부터 822,320 바이트이다.
* **확인됨**: 임포트 스텁(stub) 테이블은 오프셋 `0x000C8C8C`부터 1,272 바이트에 위치한다.
* **확인됨**: 모듈 정보 구조체(`psp_module_info`)는 `0x000C9340`에 위치한다.

---

## 3. 코드 분석 및 함수 커버리지

`allegrexrecomp funcs`를 통해 수행한 정적 흐름 분석 결과는 다음과 같다.

```mermaid
flowchart TD
    ENTRY["엔트리 및 재배치 시드 (5,967개)"] --> TRACE["제어 흐름 추적 (Control Flow Analysis)"]
    TRACE --> FUNCS["발견된 함수: 9,159개"]
    TRACE --> COV[".text 커버리지: 92.4% (761,108 / 823,684 바이트)"]
    TRACE --> INST["디코딩된 명령어: 190,277개"]
```

* **시드(Seeds)**: 엔트리 포인트(`0x00000128`) + 1개 export + 5,965개 재배치 포인터 = 총 5,967개 시드에서 탐색 시작.
* **발견 함수 수**: 9,159개.
* **도달 코드 크기**: 761,108 바이트 (`.text`의 92.4%).
* **간접 점프 테이블**: 212개 점프 테이블 식별 및 5,264개 분기 타깃 해석 완료 (미해결 간접 점프 0건).
* **명령어 통계**:
  * 총 디코딩 명령어: 190,277개.
  * VFPU 명령어: 208개 (0.11%).
  * 유효하지 않은/데이터 명령어: 68개 (0.04%).
* **함수 특성**:
  * VFPU 연산을 사용하는 함수: 67개 (0.7%).
  * 계산된 점프(switch/table)를 포함하는 함수: 380개.
  * 최대 함수: `0x0003C11C` (코드 크기 3,852 바이트).

---

## 4. 임포트 펌웨어 라이브러리 및 NID 통계

게임이 임포트하는 PSP 펌웨어 라이브러리는 20개이며, 총 159개 함수(고유 호출 144개)를 요구한다.

| 라이브러리 | 임포트 함수 수 | 주요 기능 |
|---|---|---|
| `ThreadManForUser` | 27 | 스레드 생성, 대기, 이벤트 플래그, 세마포어, 딜레이 |
| `sceMpeg` | 24 | PMF 동영상 디코딩 스트림 관리 |
| `sceSasCore` | 18 | SAS 사운드 코어 음원 합성 |
| `IoFileMgrForUser` | 12 | 파일 열기, 읽기, 쓰기, 탐색, 디렉터리 조작 |
| `sceAtrac3plus` | 11 | ATRAC3plus 음악 압축 해제 |
| `sceGe_user` | 10 | 그래픽 엔진 디스플레이 리스트 큐 및 동기화 |
| `sceAudio` | 9 | 오디오 채널 출력 및 볼륨 제어 |
| `sceUtility` | 8 | 시스템 저장 데이터 UI 및 파라미터 대화상자 |
| `UtilsForUser` | 7 | 시간 측정, 버퍼 복사 등 유틸리티 |
| `sceDisplay` | 6 | 디스플레이 모드, 프레임버퍼 설정, VBLANK 대기 |
| `sceUmdUser` | 6 | UMD 드라이브 상태 및 오류 검사 |
| `ModuleMgrForUser` | 5 | 디스크 내부 PRX 모듈 로드/시작/중지 |
| `sceCtrl` | 3 | 패드 입력 읽기 (`sceCtrlReadBufferPositive` 등) |
| `StdioForUser` | 3 | 표준 입출력 |
| `SysMemUserForUser` | 3 | 사용자 메모리 블록 할당/해제 |
| `Kernel_Library` | 2 | CPU 인터럽트 제어 |
| `LoadExecForUser` | 2 | 게임 재실행 및 종료 (`sceKernelExitGame`) |
| `sceDmac` | 1 | DMA 복사 |
| `scePower` | 1 | 전원 관리 및 배터리 정보 |
| `sceSuspendForUser` | 1 | 전원 절전 핸들러 |

---

## 5. BOOT.BIN과 EBOOT.BIN 비교 결론

`allegrexrecomp info`를 통해 `BOOT.BIN`과 `EBOOT.BIN`의 헤더 정보를 대조한 결과는 다음과 같다.

```mermaid
flowchart LR
    EBOOT["EBOOT.BIN (~PSP 암호화 모듈)<br/>헤더 tag: 0xC0CB167C<br/>복호화 대상 크기: 1,463,072 바이트<br/>엔트리: 0x00000128"]
    BOOT["BOOT.BIN (평문 ELF32)<br/>파일 크기: 1,463,072 바이트<br/>엔트리: 0x00000128"]
    EBOOT -. "완전 일치 (Exact Match)" .- BOOT
```

* **확인됨 (헤더 값 일치)**: `EBOOT.BIN`의 `~PSP` 헤더에 기록된 복호화 후 ELF 크기(`elf size`: 1,463,072 바이트), 부트 엔트리(`boot_entry`: `0x00000128`), 세그먼트 0 크기(1,028,240 바이트), 세그먼트 1 메모리 크기(2,718,852 바이트), BSS 크기(2,594,336 바이트)는 평문 ELF인 `BOOT.BIN`의 파일 크기 및 세그먼트 구조와 1바이트 오차 없이 완전히 일치한다.
* **추정 (내용 동일성)**: 헤더의 모든 메타데이터와 세그먼트 배치가 완전히 일치하므로 `BOOT.BIN`은 `EBOOT.BIN`의 암호화 전 원본 평문 실행 파일과 완전히 동일한 코드 및 데이터 배치를 가질 것으로 추정된다.
* **lift 입력 근거**: 외부 복호화 키(`psp_keys.txt`) 없이도 즉시 분석과 기계적 C 코드 생성을 진행할 수 있고 헤더 규격이 일치하므로 평문 `BOOT.BIN`을 lift 입력으로 사용한다.
* **남은 확인**: psprecomp에 복호화 기능이 구현되거나 사용자 키가 확보되어 `EBOOT.BIN`을 완전히 복호화할 수 있게 되면, 두 바이너리의 코드 및 데이터 영역을 바이트 단위로 대조하여 내용의 완전 일치 여부를 확정한다.

---

## 6. PRX 기본 주소 재배치(0x08804000) 및 평행이동 불변성(Lift Invariance)

PSP 사용자 모드에서 타이틀 PRX 모듈은 기본 주소 `0x08804000`에 로드된다. 단일 재배치 분석 경로를 적용한 후, 기준 주소 `0x00000000`과 `0x08804000`의 `allegrexrecomp funcs` 분석 결과는 기준 주소 오프셋(`0x08804000`)을 제외하면 100% 동일하다.

```mermaid
flowchart TD
    subgraph Issue1["패치 전 (issue #1, 미재배치 base 0)"]
        S0["Seeds: 5,967개"]
        F0["발견 함수: 9,159개"]
        C0["커버리지: 92.4% (761,108 / 823,684 B)"]
    end
    subgraph Final["최종 단일 재배치 경로 (base 0 / 0x08804000 공통)"]
        S1["Seeds: 4,211개 (1 entry + 1 export + 4,209 reloc)"]
        F1["발견 함수: 8,785개 (주소·크기·reached 100% 평행이동 일치)"]
        C1["커버리지: 92.0% (757,820 / 823,684 B)"]
        VFPU["VFPU 트랩: 0개 (0.00%)"]
    end
    Issue1 --> Final
```

* **평행이동 불변성 검증 (`lift_invariance`)**:
  - `base = 0x00000000`과 `base = 0x08804000` 모두에서 시드 **4,211개**, 함수 **8,785개**, 도달 코드 **757,820 바이트 (92.0%)**로 완전히 일치한다.
  - 8,785개 함수 전부에 대해 `addr(0x08804000) - 0x08804000 == addr(0)`이며 크기, 명령어 수, 속성 플래그(`ret`, `indirect`)까지 단 1건의 차이도 없다.
* **issue #1(패치 전) 대비 수치 변화 및 원인 (9,159 -> 8,785개, 761,108 B -> 757,820 B)**:
  1. **시드 수 감소 (5,967 -> 4,211)**:
     - 패치 전에는 `psp_collect_pointer_seeds`가 `SHT_PRXRELOC`의 `ofsbase`/`addrbase` 세그먼트 인덱스를 구분하지 않아 세그먼트 1(`.data`/`.bss`)을 가리키는 포인터를 잘못된 코드 주소로 오인하여 가짜 시드가 다수 포함되었다. 단일 재배치 경로에서는 세그먼트별 베이스(`S = base + seg[addrbase].vaddr`)를 정확히 적용해 유효 포인터 4,209개만 시드로 수집한다.
  2. **거짓 분할(false split) 해소 (`0x15C` 예시)**:
     - 패치 전에는 `looks_like_code` 휴리스틱과 잘못된 시드로 인해 함수 내부 명령어가 별도 함수 시작점으로 쪼개지는 거짓 분할이 발생했다.
     - 대표 예로 진입 함수(`0x00000128` / `0x08804128`, 크기 164바이트 / 41 명령어) 내부의 `0x0000015C` 위치(`sw $ra, 8($sp)`)는 패치 전에는 별도 함수로 거짓 분할되었으나, 단일 재배치 경로에서는 진입 함수의 일부로 정상 유지된다.
  3. **비재배치 상수 `lui+addiu` 오탐 제거**:
     - `analyze.c`의 `lui+addiu`/`ori` 선형 스캔은 재배치 대상이 아닌 일반 숫자 상수를 코드 주소로 오인해 기준 주소가 `0`일 때 `.text` 범위와 우연히 겹쳐 가짜 함수를 분할했다. PRX 재배치 이미지(`an->image != NULL`)에서는 유효 포인터가 이미 재배치 테이블에 모두 존재하므로 해당 스캔을 건너뛰어 기준 주소 불변성을 확보했다.
  4. **남은 차이 원인 (함수 -374개, 도달 크기 -3,288 B)**:
     - 위 세 가지 원인(세그먼트 베이스 오독 시드 제거, 거짓 분할 해소, 비재배치 상수 오탐 제거)으로 인해 미도달 데이터/가짜 경로가 배제되면서 8,785개 함수와 757,820 바이트로 수렴하였다.
  5. **놓친 함수가 없다는 확인 (리뷰, 2026-11-14)** (**확인됨**): 패치 전 분석에만 있고 지금의 어느 함수에도 속하지 않는 시작점 139개를 재배치된 이미지 전체의 포인터 워드와 `jal` 대상에서 찾았다.
     - 20개는 import stub 영역(`0x000C8C8C`부터 1,272바이트)이다. 지금은 `psp_import_*` thunk로 처리되므로 함수 목록에 없는 것이 맞다.
     - 107개는 `.text`(`0x40`부터 822,320바이트, 끝 `0x000C8D70`) 바깥의 읽기 전용 데이터(모듈 정보 `0x000C9340`, export 표, 문자열 등)를 코드로 읽은 가짜 함수다. 몇 개는 크기가 5만~10만 바이트에 이르렀다. 이것을 가리키는 워드는 데이터 표의 포인터다.
     - 12개(212바이트)만 `.text` 안에 있으며, 포인터로도 `jal` 대상으로도 **한 번도 참조되지 않는다**: `0x000063A0`, `0x00022808`(첫 워드 `00000000`), `0x0009AE0C`, `0x000A4190`, `0x000A4194`, `0x000A47B8`, `0x000A47C4`, `0x000A47C8`, `0x000A56EC`, `0x000A56F0`, `0x000B9CEC`, `0x000C69AC`. 함수 중간 조각이나 패딩이다.
     - 그러므로 줄어든 3,288바이트는 가짜 시작점에서만 도달되던 조각이며, 지금의 8,785개가 패치 전 9,159개보다 정확하다.
* **방출 및 MSVC 빌드 결과**:
  - 8,785개 함수(17,215 interior entries)가 138개 번들 C 파일(`dmp1_funcs_000..137.c`)로 분할 방출되었으며, 189,455개 명령어 중 VFPU 트랩은 0개(0.00%)다.

---

## 7. 최초 펌웨어 호출(First Firmware Call) 정적 및 동적 분석

게스트 엔트리 포인트(`0x08804128`)에서 최초로 호출하는 PSP 펌웨어 라이브러리 API는 다음과 같이 확인되었다.

```mermaid
sequenceDiagram
    participant Guest as 게스트 진입점 (0x08804128)
    participant Thunk as 임포트 스텁 (0x088CCD6C)
    participant HLE as repsp NID Handler
    Guest->>Thunk: jal 0x088CCD6C
    Note over Thunk: ThreadManForUser:0x446D8DE6<br/>(sceKernelCreateThread)
    Thunk->>HLE: psp_hle_call(0x446D8DE6)
    Note over HLE: STOPPED_ON_FIRST_HLE_CALL
```

* **호출 위치**: `0x0880419C` (`jal 0x088CCD6C`)
* **임포트 스텁**: `psp_import_088CCD6C` -> `ThreadManForUser:0x446D8DE6`
* **함수 기호(Symbol)**: `sceKernelCreateThread`
* **출처(Provenance)**: 공용 PSP SDK(psp-sdk) NID 데이터베이스에 정식 등재된 기호로, 비공개나 난독화된 기호가 아닌 정규 공개 API이다.
* **동적 검증(`redmp1 roms --lift-check`)**: GuardedRun 환경에서 게스트 코드를 구동한 결과, 0회의 간접 디스패치 후 정확히 `ThreadManForUser:0x446D8DE6`에서 최초 HLE 트랩이 포착되고 성공적으로 정지하였다.

---

# Executable Analysis: DJMAX Portable (ULKS-46050) (English)

This document records facts confirmed by analyzing the original UMD executables (`PSP_GAME/SYSDIR/BOOT.BIN` and `EBOOT.BIN`) using `allegrexrecomp`.

## 1. Module Identity and Container

* **Name**: `djmax` (version 1.1, attributes `0x0000`, user-mode PRX).
* **Format**: ELF32 MIPS LE (PRX, relocatable), 1,463,072 bytes.
* **Entry point**: `0x00000128`, GP: `0x00026660`.
* **Exported functions**: 1 (`module_start`).

## 2. Segments and Sections

* **Segment 0**: Virtual addr `0x00000000`, file 1,028,240 bytes, mem 1,028,240 bytes, flags `rwx` (code, rodata, stubs, modinfo).
* **Segment 1**: Virtual addr `0x000FB090`, file 124,516 bytes, mem 2,718,852 bytes, flags `rw-` (data, bss).
* **BSS size**: 2,594,336 bytes.
* **Sections**: 38 sections, `.text` is 822,320 bytes at offset `0x00000040`.

## 3. Code Analysis and Function Coverage

* **Seeds**: 5,967 (entry + 1 export + 5,965 relocation pointers).
* **Functions found**: 9,159.
* **Coverage**: 761,108 bytes reached (92.4% of `.text`).
* **Instructions**: 190,277 instructions decoded (208 VFPU = 0.11%, 68 invalid/data = 0.04%).
* **Indirect jumps**: 212 jump tables resolved to 5,264 targets; 0 unresolved computed-jump sites.

## 4. Imported Firmware Libraries

20 libraries and 159 functions (144 distinct calls), dominated by `ThreadManForUser` (27), `sceMpeg` (24), `sceSasCore` (18), `IoFileMgrForUser` (12), `sceAtrac3plus` (11), `sceGe_user` (10), `sceAudio` (9), `sceUtility` (8), and `UtilsForUser` (7).

## 5. BOOT.BIN vs EBOOT.BIN Comparison Conclusion

* **Confirmed (Matching Headers)**: `EBOOT.BIN`'s `~PSP` header specifies an unencrypted ELF size of 1,463,072 bytes, an entry of `0x00000128`, segment 0 size of 1,028,240 bytes, segment 1 memory size of 2,718,852 bytes, and BSS size of 2,594,336 bytes, exactly matching the unencrypted `BOOT.BIN` file dimensions.
* **Inferred (Identical Content)**: Because all container dimensions, entry points, and segment mappings match without discrepancy, `BOOT.BIN` is inferred to contain identical code and data to decrypted `EBOOT.BIN`.
* **Grounds for Lifting BOOT.BIN**: Plain `BOOT.BIN` requires no external decryption keys (`psp_keys.txt`) and matches all container parameters, making it the practical choice for lifting and analysis.
* **Open Checks**: Compare byte-by-byte against decrypted `EBOOT.BIN` once decryption becomes available (via psprecomp feature updates or user-provided keys).

## 6. PRX Base Address Relocation (0x08804000) and Function Analysis

* **Relocation Base**: Loaded at `0x08804000` (standard PSP user module base).
* **Seed Refinement (5,967 -> 4,211)**: the seed harvesting now honors `ofsbase`/`addrbase`, and one analysis path serves every base, eliminating false seeds from misread data words and non-relocated constants.
* **Functions & Coverage**: 8,785 functions (split across 138 C translation units), 757,820 bytes reached (92.0% of `.text`), identical at base 0 and `0x08804000` after the shift (`lift_invariance`). 0 VFPU trap instructions emitted (0.00%).
* **No function lost (review, 2026-11-14)** (**confirmed**): of the 139 starts found only before the fixes and inside no current function, 20 are the import stub area (`0x000C8C8C`, 1,272 bytes; now `psp_import_*` thunks), 107 are read-only data beyond `.text` (which ends at `0x000C8D70`) decoded as code, some 50-100 KB long, and only 12 (212 bytes) lie inside `.text`, none ever referenced by a pointer word or a `jal` anywhere in the relocated image (mid-function fragments or padding). The 3,288 bytes no longer reached were reachable only from false starts; the current 8,785 functions are more accurate than the earlier 9,159.
* **MSVC Build Metrics**: Release build of `redmp1_recomp.lib` completes in ~22.8 seconds; static library size is 34.9 MiB (33.8 MiB total object size).

## 7. First Firmware Call

* **Site**: `0x0880419C` (`jal 0x088CCD6C`).
* **Import Target**: `ThreadManForUser:0x446D8DE6` (`psp_import_088CCD6C`).
* **Canonical Symbol**: `sceKernelCreateThread` in the public PSP SDK database.
* **Runtime Verification**: `redmp1 roms --lift-check` executes entry point `0x08804128` under `GuardedRun`, cleanly captures the first HLE call at `ThreadManForUser:0x446D8DE6`, and terminates with status `STOPPED_ON_FIRST_HLE_CALL`.
