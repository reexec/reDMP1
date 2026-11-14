# 아키텍처 (ARCHITECTURE)

이 문서는 reDMP1 프로젝트의 레이어 구조, 컴포넌트 간 의존성, 재컴파일 파이프라인 및 실행 흐름을 정의합니다.

---

## 1. 레이어 구조 및 의존성 규칙

reDMP1은 엄격한 단방향 의존성 계층 구조를 따릅니다. 상위 레이어는 오직 직속 하위 레이어의 공개 인터페이스(public contract)만 참조하며, 하위 레이어는 상위 레이어의 존재나 타이틀 고유 정보를 알지 못합니다.

```mermaid
flowchart TD
    APP["앱 계층 (src/app/)<br/>redmp1 실행 파일: CLI 인자 파싱, 식별, 오케스트레이션"]
    GAME["타이틀 계층 (src/game/dmp1/)<br/>redmp1_game 라이브러리: dmp1_profile, variants.json 식별"]
    RECOMP["재컴파일 계층 (src/game/dmp1/recomp/generated/)<br/>redmp1_recomp (로컬 생성 C 코드)"]
    ABI["ABI 어댑터 (src/recomp_abi/)<br/>redmp1_recomp_abi: psprecomp ABI ↔ repsp recompiled contract"]
    REPSP["공유 런타임 (third_party/repsp)<br/>repsp::runtime, repsp::module, repsp::hle_kernel, repsp::recomp"]
    PLAT["플랫폼 계층 (repsp 내부)<br/>repsp::platform: Win32/Linux, 가상 메모리, SDL3"]

    APP --> GAME
    APP --> REPSP
    GAME --> RECOMP
    RECOMP --> ABI
    ABI --> REPSP
    REPSP --> PLAT
```

### 레이어별 역할

1. **`app` (`src/app/`, 타깃 `redmp1`)**:
   * 애플리케이션 진입점(`main.cpp`).
   * 사용자 명령줄 인자(`--variants`, `--lift-check`, `--headless`, 게임 디렉터리 경로) 파싱 및 옵션 처리.
   * `redmp1_game`의 `IdentifyGameDirectory`를 호출하여 게임 디스크 유효성 검사.
   * 기본 실행(`REDMP1_HAVE_RECOMP=1`) 시 `LoadPrx`, `repsp::hle::kernel::Register(KernelConfig)`(`disc0:`/`umd0:`/`ms0:` 마운트), `repsp::hle::ge::Register`, `repsp::hle::display::Register`(`--headless` 지원), `Scheduler::CreateThread("module_start")` 및 `GuardedRun` 기반 부팅을 오케스트레이션하고, 정지 분류 `(a)`/`(b)`/`(c)`(SHA-1 공개 NID 이름 표기 포함), 디스플레이·GE 통계 및 `PrintKernelSummary()`를 출력한다.
2. **`game/dmp1` (`src/game/dmp1/`, 타깃 `redmp1_game`)**:
   * DJMAX Portable 타이틀 고유 메타데이터와 프로파일 정의(`include/redmp1/dmp1_profile.h`).
   * `variants.json` 파싱 및 디스크 식별 로직.
   * `repsp::module::ParamSfo`를 사용하여 `PSP_GAME/PARAM.SFO`로부터 `DISC_ID`, `DISC_VERSION`, `TITLE` 추출.
   * `repsp::runtime::ComputeSha1`을 사용하여 후보 실행 파일의 SHA-1 해시 검증.
3. **`recomp_abi` (`src/recomp_abi/`, 타깃 `redmp1_recomp_abi`)**:
   * `allegrexrecomp`가 기계적으로 생성한 C 코드가 호출하는 `psprecomp` ABI 인터페이스(`psp_cpu`, `psp_read*`, `psp_write*`, `psp_hle_call`, `psp_syscall`, VFPU 행렬 연산자 등)를 완벽하게 구현하여 `repsp`의 재컴파일 코드 계약(`include/repsp/recomp/guest_cpu.h`) 및 메모리 계층에 바인딩.
   * `cpu.c`: `psp_cpu_t psp_cpu` 전역 레지스터 상태 정의 및 `repsp_guest_cpu_ops` 콜백(`save`, `restore`, `get_gpr`, `set_gpr`, `get_fpr`, `set_fpr`, `call_guest`) 구현과 등록.
   * `mem.c`: `psp_read8/16/32/64`, `psp_write8/16/32/64` 인라인 메모리 접근자를 `repsp_guest_read*`, `repsp_guest_write*`에 매핑.
   * `dispatch.c`: 재컴파일된 함수 디스패치 테이블 및 `psp_dispatch_call` 라우팅. 초기 설계에서는 게스트 주소 기반 평면 배열(flat array) 색인을 검토했으나, `psprecomp` ABI 구현과의 정합성과 메모리 효율·등록 단순성을 위해 `psprecomp`의 개방 주소 지정(open-addressed) 해시 테이블을 유지한다. 향후 프로파일링에서 간접 호출 디스패치 오버헤드가 병목으로 확인되는 경우에만 평면 배열로 전환한다.
   * `trap.c`: `psp_hle_call`, `psp_syscall`, `psp_unimplemented` 처리. 디스패치 미스 및 `psp_unimplemented` 발생 시 `repsp_raise_stop`을 통해 `GuardedRun`으로 제어를 반환한다.
   * `vfpu.c`: psprecomp가 생성하는 VFPU 전치, 행렬 연산 매크로 도우미.
   * **`repsp`는 `psprecomp`에 의존하지 않으며**, 어댑터는 reDMP1 내부에 위치함.
4. **`recomp` (`src/game/dmp1/recomp/generated/`, 타깃 `redmp1_recomp`)**:
   * `allegrexrecomp emit`으로 기계적으로 생성된 8,784개 함수의 C 구현체(138개 번들 번역 단위 `dmp1_funcs_000..137.c` + `dmp1_imports.c` + `dmp1_register.c`).
   * `redmp1_recomp_abi`를 통해 런타임과 연결되며, `redmp1` 실행 파일에 정적 라이브러리로 링크.
5. **`repsp` (`third_party/repsp`, 공유 서브모듈)**:
   * 타이틀 중립적이며 독립적으로 재사용 가능한 PSP 런타임 라이브러리.
   * `GuestMemory`: 512 MiB 가상 주소 공간 예약 및 `0x1FFFFFFF` 미러링 접기 주소 변환.
   * `NidRegistry`: NID 기반 시스템 콜 디스패치 및 통계 수집.
   * `ParamSfo`: PSP `PARAM.SFO` 바이너리 파서.
   * `GuardedRun`: 하드웨어 세그폴트 및 접근 위반 방어 실행.
   * `DisplayBackend`, `InputBackend`, `AudioBackend`: SDL3 기반 하드웨어 백엔드.

---

## 2. 재컴파일 파이프라인

재컴파일 파이프라인은 원본 게임 미디어로부터 네이티브 실행 코드를 빌드하기까지의 흐름을 정의합니다.

```mermaid
flowchart LR
    ROM["정품 UMD 덤프<br/>(roms/PSP_GAME/)"] --> REG["register_disc.py<br/>PARAM.SFO + SHA-1"]
    REG --> VAR["variants.json"]
    ROM --> LIFT["regenerate.py<br/>allegrexrecomp emit"]
    LIFT --> GEN["src/game/dmp1/recomp/generated/<br/>(dmp1_funcs_*.c, dmp1_funcs.h)"]
    GEN --> BUILD["CMake 타깃 빌드<br/>redmp1_recomp + redmp1_recomp_abi"]
```

1. **`register_disc.py`**:
   * 게임 폴더 내 `PARAM.SFO`에서 디스크 ID(`ULKS46050`)와 버전(`1.02`)을 추출.
   * 평문 실행 파일(`BOOT.BIN`)의 SHA-1 다이제스트를 계산하여 `variants.json`에 등록.
2. **`allegrexrecomp` (호스트 도구)**:
   * `third_party/psprecomp/tools/allegrexrecomp`의 핵심 소스를 psprecomp 런타임 라이브러리 의존성 없이 빌드하여 호스트 실행 파일로 제공.
   * `info`: ELF/PRX 컨테이너 헤더 및 세그먼트 정보 출력.
   * `funcs`: 제어 흐름 분석, 함수 발견, 임포트 NID 통계 출력.
   * `emit`: 발견된 9,159개 함수를 144개 C 소스 파일(`dmp1_funcs_*.c`)로 분할 생성.
3. **`regenerate.py`**:
   * `BOOT.BIN`을 lift 입력으로 자동 선택하여 `allegrexrecomp emit`을 실행하고 `src/game/dmp1/recomp/generated/`에 C 코드를 갱신.

---

## 3. 실행 디렉터리 및 디스크 식별 흐름

```mermaid
sequenceDiagram
    participant User as 사용자
    participant App as redmp1 (main)
    participant Profile as dmp1_profile
    participant SFO as repsp::module::ParamSfo
    participant Sha1 as repsp::runtime::Sha1

    User->>App: redmp1 <game_dir>
    App->>Profile: IdentifyGameDirectory(game_dir, variants)
    Profile->>Profile: game_dir 존재 및 디렉터리 여부 확인
    Profile->>SFO: Parse(PSP_GAME/PARAM.SFO)
    SFO-->>Profile: DISC_ID, DISC_VERSION, TITLE
    Profile->>Profile: BOOT.BIN 또는 EBOOT.BIN 탐색
    Profile->>Sha1: ComputeSha1(BOOT.BIN bytes)
    Sha1-->>Profile: SHA-1 hex string
    Profile->>Profile: FindVariant(variants, ID, Version, SHA-1)
    alt 일치하는 변형 발견
        Profile-->>App: Success (Title, ID, Version, Size, SHA-1)
        App->>User: 식별 성공 정보 출력 (exit 0)
    else 불일치 또는 오류
        Profile-->>App: Failure status (오류 원인 메시지)
        App->>User: 오류 출력 및 비정상 종료 (exit > 0)
    end
```

---

# Architecture (English)

This document describes reDMP1's layer structure, component dependencies, recompilation pipeline, and runtime flow.

## 1. Layers and Dependency Rules

* **`app` (`src/app/`, target `redmp1`)**: Application entry point, CLI arguments (`--variants`, `--lift-check`), orchestration. When built with generated code (`REDMP1_HAVE_RECOMP=1`), default execution loads `BOOT.BIN` via `LoadPrx`, mounts `disc0:`/`umd0:` (read-only) and `ms0:` (read-write) via `repsp::hle::kernel::Register`, creates `module_start` (`priority=32`, `stack_size=262144`, `attr=0` per `lift.json`), runs `Scheduler::Run` under `GuardedRun`, classifies stops into `(a)`/`(b)`/`(c)`, and prints `PrintKernelSummary()`.
* **`game/dmp1` (`src/game/dmp1/`, target `redmp1_game`)**: Title profile, variant identification, SFO parsing via repsp, SHA-1 validation.
* **`recomp_abi` (`src/recomp_abi/`, target `redmp1_recomp_abi`)**: Adapter defining psprecomp ABI globals/functions and binding to repsp's recompiled-code contract. repsp does not depend on psprecomp. Note on `dispatch.c`: although the initial design considered a guest-address flat array, the implementation keeps `psprecomp`'s open-addressed hash table for ABI compatibility, memory efficiency, and simpler registration, and will switch to a flat array only if profiling shows dispatch overhead as a bottleneck.
* **`repsp` (`third_party/repsp`)**: Shared PSP runtime (GuestMemory, NidRegistry, HLE kernel, platform backends with SDL3).

## 2. Recompilation Pipeline

1. `register_disc.py`: Reads `PARAM.SFO` and computes `BOOT.BIN` SHA-1 to register the disc into `variants.json`.
2. `allegrexrecomp`: Host tool built without psprecomp runtime dependencies.
3. `regenerate.py`: Selects `BOOT.BIN` as plain ELF lift input and invokes `allegrexrecomp emit` into `src/game/dmp1/recomp/generated/`.

## 3. Disc Identification Flow

`redmp1` verifies `PSP_GAME/PARAM.SFO` and `BOOT.BIN` SHA-1 against `variants.json`. If matched, returns success and details; otherwise prints diagnostics and exits with a non-zero code.
