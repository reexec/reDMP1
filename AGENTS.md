# AGENTS.md

이 문서의 작업 규칙과 코딩 규칙은 [reexec/reCT3](https://github.com/reexec/reCT3)의 규칙과 같다(그 규칙은 [nworkers/reJSRF](https://github.com/nworkers/reJSRF)와 [reexec/rePIU](https://github.com/reexec/rePIU)에서 왔다). 프로젝트의 성격(정적 재컴파일과 디컴파일)과 라이선스(MIT)도 같고, 대상 기기(Xbox → PSP), 타이틀, 공유 런타임과 lifter만 다르다.

The working and coding rules here are those of [reexec/reCT3](https://github.com/reexec/reCT3) (themselves from [nworkers/reJSRF](https://github.com/nworkers/reJSRF) and [reexec/rePIU](https://github.com/reexec/rePIU)). The project's nature (static recompilation and decompilation) and license (MIT) are the same; only the console (Xbox → PSP), the title, the shared runtime and the lifter differ.

## 프로젝트 개요

이 프로젝트는 PSP 게임 DJMAX Portable(한국판, `ULKS-46050`, 디스크 버전 1.02)을 에뮬레이터 없이 Win64, Linux x64, Win32, Linux x86, Web, Android에서 네이티브로 실행하는 것을 목표로 한다.

방법은 두 가지를 함께 쓴다.

* **재컴파일(Recompile)**: 원본 실행 파일(`PSP_GAME/SYSDIR/BOOT.BIN`/`EBOOT.BIN`)의 Allegrex(MIPS32r2 + VFPU) 코드를 기계적으로 C로 옮겨(lift) 호스트 컴파일러로 빌드한다. 이것이 게임 로직의 기본 실행 경로다.
* **디컴파일(Decompile)**: 옮겨진 함수를 사람이 읽을 수 있는 C/C++로 하나씩 대체한다. 대체한 함수는 원래 함수와 같은 동작임을 검증한다.

lift와 디컴파일 보조 도구는 [sp00nznet/psprecomp](https://github.com/sp00nznet/psprecomp)(`third_party/psprecomp`, submodule, MIT)의 `allegrexrecomp`와 그 ABI 헤더를 쓴다. 생성 코드가 쓰는 psprecomp ABI를 repsp의 lifter 중립 재컴파일 코드 계약에 잇는 어댑터(`src/recomp_abi/`)는 이 저장소에 있다(repsp는 psprecomp에 의존하지 않는다). PSP 펌웨어(커널, 사용자 모드 라이브러리), GE(그래픽), 오디오, 입력, 파일 시스템은 High Level Emulation(HLE) 계층으로 대체한다. HLE 계층과 플랫폼 백엔드는 별도 저장소 [reexec/repsp](https://github.com/reexec/repsp)(`third_party/repsp`, submodule)에 있으며 다른 PSP 타이틀 프로젝트와 공유한다.

## Project Overview

This project aims to run the PSP game DJMAX Portable (the Korea build, `ULKS-46050`, disc version 1.02) natively on Win64, Linux x64, Win32, Linux x86, the web and Android, without an emulator.

Two methods are used together.

* **Recompile**: mechanically lift the Allegrex (MIPS32r2 + VFPU) code of the original executable (`PSP_GAME/SYSDIR/BOOT.BIN`/`EBOOT.BIN`) to C and build it with a host compiler. This is the default execution path of the game logic.
* **Decompile**: replace lifted functions one at a time with human-readable C/C++. Each replacement is verified to behave like the function it replaces.

Lifting and the decompilation aids come from [sp00nznet/psprecomp](https://github.com/sp00nznet/psprecomp) (`third_party/psprecomp`, a submodule, MIT): its `allegrexrecomp` tool and its ABI headers. The adapter connecting the psprecomp ABI the generated code uses to repsp's lifter-neutral recompiled-code contract (`src/recomp_abi/`) lives in this repository (repsp does not depend on psprecomp). The PSP firmware (kernel and user-mode libraries), the GE (graphics), audio, input and the file system are replaced by High Level Emulation (HLE) layers. Those and the platform backends live in the separate repository [reexec/repsp](https://github.com/reexec/repsp) (`third_party/repsp`, a submodule), shared with other PSP title projects.

---

## 핵심 설계 목표

우선순위:

1. 원본 게임 로직을 보존한다. 재컴파일된 코드가 기준 구현이며, 디컴파일은 그 동작을 바꾸지 않는다.
2. HLE는 API 경계(펌웨어 라이브러리의 NID export, 시스템 콜)에서 한다. 하드웨어 수준 처리는 API 경계로 해결되지 않는 곳에만 둔다(GE 디스플레이 리스트 해석이 대표적이다).
3. 재사용 가능한 PSP 런타임(`repsp`)은 특정 타이틀에 의존하지 않는다. 타이틀 고유 사항은 타이틀 프로파일과 `src/game/dmp1/`에만 둔다. repsp에 필요한 변경은 repsp 저장소에서 그 규칙대로 하고, 이 저장소는 submodule을 올린다.
4. Win64를 먼저 완성하고, 이후 Linux x64, Win32, Linux x86, Web, Android로 넓힌다. 처음부터 멀티플랫폼 공용 구조로 설계한다.
5. 에뮬레이터 소스(PPSSPP 등)를 통합하지 않는다. 동작 비교의 기준(oracle)으로만 쓴다.
6. 게임 데이터(ISO, 실행 파일, PRX, 자산)와 그것에서 기계적으로 생성한 코드는 저장소에 넣지 않는다.

## Primary Design Goals

Priority order:

1. Preserve the original game logic. The recompiled code is the reference implementation, and decompilation does not change its behavior.
2. Do HLE at API boundaries (firmware library NID exports, syscalls). Hardware-level processing exists only where an API boundary cannot work (interpreting GE display lists is the main case).
3. The reusable PSP runtime (`repsp`) knows nothing about any title. Title specifics live only in the title profile and `src/game/dmp1/`. A change repsp needs is made in the repsp repository under its rules; this repository then moves the submodule.
4. Finish Win64 first, then widen to Linux x64, Win32, Linux x86, the web and Android. Design shared multiplatform structures from the start.
5. Do not integrate emulator source code (PPSSPP and others); use emulators only as behavior oracles.
6. Never put game data (ISO, executables, PRX modules, assets) or code mechanically generated from it in the repository.

---

## 참조 문서

아키텍처 변경 전에는 관련 문서를 먼저 읽고 갱신한다.

* `docs/PROJECT_CHARTER.md`
* `ARCHITECTURE.md`
* `docs/EXECUTABLE_DESIGN.ko.md`
* `docs/EXECUTABLE_DESIGN.en.md`
* `docs/CODING_STYLE.md`
* `THIRD_PARTY_NOTICES.md`
* `docs/design/`
* `docs/work-orders/`
* `docs/work-logs/`
* `docs/analysis/`
* `docs/kb/`
* `docs/guides/`
* `third_party/repsp/ARCHITECTURE.md`(공유 런타임의 구조와 소비자 계약)
* `third_party/psprecomp/docs/`(lifter, 컨테이너, VFPU, oracle 사용법)

## Reference Documents

Read and update the relevant documents before making architectural changes: the list above, plus `third_party/repsp/ARCHITECTURE.md` for the shared runtime's structure and consumer contract and `third_party/psprecomp/docs/` for the lifter, the containers, the VFPU and oracle use.

---

## 핵심 원칙

1. 사용자 요구사항을 받으면 바로 구현하지 말고 먼저 설계를 작성하거나 기존 설계를 갱신한다.
2. 설계가 정리되면 구현 계획서를 작성하거나 갱신한 뒤 구현을 시작한다.
3. 코드 변경이 있는 모든 작업은 작업 지시 문서와 작업 로그 문서를 남긴다.
4. 문서는 기본적으로 한국어를 먼저 쓰고 바로 아래에 영어 번역을 추가한다. 이 규칙은 `docs/` 아래 문서와 `README.md`, `ARCHITECTURE.md` 같은 저장소 문서에만 적용되며, 소스 코드 주석에는 적용하지 않는다.
5. 기존 코드, 자산, 문서의 의도를 확인한 뒤 수정하며, 큰 구조 변경은 근거를 문서에 남긴다.
6. 멀티플랫폼 공용 구조를 우선 설계하고, 플랫폼별 세부 사항은 분리한다.

## Core Principles

1. When a user requirement is received, do not implement immediately. First write a design or update the existing design.
2. After the design is clear, write or update the implementation work order before starting implementation.
3. Every task that changes code must leave a work-order document and a work-log document.
4. Documents should be written in Korean first, followed immediately by an English translation. This applies to documents under `docs/` and repository documents such as `README.md` and `ARCHITECTURE.md`; it does not apply to source-code comments.
5. Confirm the intent of existing code, assets, and documents before modifying them. Document the rationale for large structural changes.
6. Design shared multiplatform structures first, and keep platform-specific details separated.

---

## 응답 태도 규칙

* 모든 질문에 대한 답변은 존대말을 사용하고 예의 있는 태도를 유지한다.

## Response Tone Rules

* Use polite Korean speech and maintain a respectful attitude for all answers.

---

## 요구사항 처리 절차

1. 요구사항 접수
2. 관련 코드, 문서, 자산 맥락 확인
3. 설계 문서 작성 또는 갱신
4. 구현 계획서 작성 또는 갱신
5. 사용자 승인 또는 현재 진행 맥락 확인
6. 구현
7. 빌드 또는 범위에 맞는 검증
8. 작업 로그 작성

단, 요구사항이 단순 질문이거나 확인 요청이라면 해당 내용에 바로 응답하고 문서화 및 코드 구현은 하지 않는다.

## Requirement Handling Procedure

1. Receive the requirement.
2. Inspect the relevant code, documentation, and asset context.
3. Write or update the design document.
4. Write or update the implementation work order.
5. Confirm user approval or the current active context.
6. Implement.
7. Run a build or verification appropriate to the scope.
8. Write the work log.

If the requirement is a simple question or confirmation request, answer it directly without documentation or code implementation.

---

## 문서 작성 규칙

* 설계 문서는 `docs/design/` 아래에 둔다.
* 계획 문서는 `docs/work-orders/` 아래에 둔다.
* 작업 결과와 회고는 `docs/work-logs/` 아래에 둔다.
* 사용자가 직접 수행하는 검증·측정·운영 절차는 `docs/guides/` 아래에 둔다. 반복 수행 가능한 절차만 두고, 근거가 되는 작업 로그와 설계를 링크한다.
* 원본 실행 파일과 디스크 분석으로 확인한 사실, 가설, 미해결 질문은 `docs/analysis/` 아래에 주제별로 둔다.
* 일반 기술 정의와 배경 지식은 `docs/kb/` 아래에 주제별로 둔다. PSP 일반 지식(컨테이너 형식, Allegrex, 펌웨어 NID, GE)은 repsp의 `docs/kb/`에 있으므로 거기에 더하고, 여기에는 이 타이틀의 이해에만 필요한 것(예: 게임의 데이터 형식)을 둔다. 외부 자료에서 왔으면 원문 링크를 단다.
* 프로젝트의 큰 방향성은 `docs/PROJECT_CHARTER.md`에 반영한다.
* 현재 구현되는 코드의 설계와 구조는 `ARCHITECTURE.md`에 지속적으로 반영한다.
* 원본 실행 파일 분석으로 확인한 구조와 설계는 `docs/EXECUTABLE_DESIGN.ko.md`, `docs/EXECUTABLE_DESIGN.en.md`에 누적 반영한다.
* 규칙이 바뀌면 `AGENTS.md`와 관련 문서를 함께 갱신한다.
* 작업 단위 문서 파일명은 `YYYYMMDD-i###-slug.md` 형식을 사용한다. `###`은 task의 GitHub issue 번호를 세 자리로 맞춘 값이다(예: issue #6 → `20261111-i006-slug.md`).
* Markdown 문서에서 구조, 관계, 흐름, 호출 순서, 상태 전이, 주소 변환을 도식화할 수 있으면 Mermaid를 적극적으로 사용한다.
* 세 개 이상의 구성요소·단계·분기·계층이 있으면 Mermaid 적용 가능성을 검토하고, 내용에 맞는 형식(flowchart, sequenceDiagram, stateDiagram, classDiagram 등)을 고른다.
* 단순 사실 하나나 한 단계 설명에는 도식을 강제하지 않는다.
* Mermaid 도식은 본문과 같은 사실 및 확인 상태를 표현해야 하며, 본문 설명을 완전히 대체하지 않는다.

## Documentation Rules

* Put design documents under `docs/design/`.
* Put work-order documents under `docs/work-orders/`.
* Put work results and retrospectives under `docs/work-logs/`.
* Put procedures the user runs themselves (verification, measurement, operations) under `docs/guides/`. Keep only repeatable procedures there, and link the design and work log they rest on.
* Put findings from the original executable and disc (confirmed behavior, hypotheses, unresolved questions) under `docs/analysis/`, organized by topic.
* Put generally applicable technical definitions and background under `docs/kb/`, organized by topic. General PSP knowledge (container formats, the Allegrex, firmware NIDs, the GE) lives in repsp's `docs/kb/` and is extended there; here go only topics this title alone needs (its data formats, for example). Link external sources when derived from them.
* Reflect the project's broad direction in `docs/PROJECT_CHARTER.md`.
* Continuously reflect the design and structure of currently implemented code in `ARCHITECTURE.md`.
* Accumulate original-executable analysis findings and design notes in `docs/EXECUTABLE_DESIGN.ko.md` and `docs/EXECUTABLE_DESIGN.en.md`.
* When rules change, update `AGENTS.md` and the related documents together.
* Use the `YYYYMMDD-i###-slug.md` filename format for task documents, where `###` is the task's GitHub issue number padded to three digits (for example issue #6 → `20261111-i006-slug.md`).
* Use Mermaid whenever structure, relationships, flows, call sequences, state transitions, or address translation can be visualized.
* Evaluate Mermaid whenever content has three or more components, steps, branches, or layers, and choose a fitting form (flowchart, sequenceDiagram, stateDiagram, classDiagram, ...).
* Do not force diagrams into single-fact or one-step explanations.
* Mermaid diagrams must express the same facts and confirmation status as the prose and must not replace the written explanation.

## 분석 및 지식 기반 유지 규칙

* 바이너리 분석이나 구현 검증으로 새로운 사실이 확인되거나 기존 결론이 바뀌면 같은 작업에서 관련 `docs/analysis/` 문서를 갱신한다.
* 작업 중 프로젝트 이해에 중요한 새로운 용어, 표준, 실행 형식, CPU 동작, 펌웨어 API가 등장하면 관련 `docs/kb/` 문서(일반 PSP 지식은 repsp의 것)를 작성하거나 갱신한다.
* 외부 자료에서 얻은 기술 지식에는 가능한 한 권위 있는 출처 링크를 가까운 위치에 남긴다.
* `docs/analysis/`는 **확인됨**, **추정**, **미확정**을 구분한다.
* analysis 또는 kb 파일을 추가하거나 이름을 바꾸면 해당 디렉터리의 `README.md` 색인을 같은 작업에서 갱신한다.
* `docs/work-logs/`는 시간순 작업 증거로, `docs/analysis/`와 `docs/kb/`는 주제별 누적 문서로 유지한다.
* 다른 프로젝트(psprecomp, PPSSPP, uOFW 등)에서 얻은 주장은 우리 바이너리에서 다시 확인하기 전까지 **추정**으로 적고 출처를 단다.

## Analysis and Knowledge Base Maintenance Rules

* When binary analysis or implementation verification confirms a new fact or changes an earlier conclusion, update the relevant `docs/analysis/` topic in the same task.
* When important new terminology, standards, executable formats, CPU behavior or firmware APIs arise, create or update the relevant `docs/kb/` topic (repsp's for general PSP knowledge).
* Place authoritative external links near derived technical knowledge.
* Distinguish **confirmed**, **inferred**, and **unresolved** statements in `docs/analysis/`.
* When adding or renaming an analysis or knowledge file, update that directory's `README.md` index in the same task.
* Keep `docs/work-logs/` as chronological evidence and `docs/analysis/` and `docs/kb/` as cumulative topic references.
* Record claims taken from other projects (psprecomp, PPSSPP, uOFW, ...) as **inferred**, with their source, until they are re-checked on our binary.

---

## 레이어와 의존 규칙

* 레이어는 아래에서 위로 `platform` → `repsp`(module, runtime, recomp, hle, gpu) → `recomp_abi`(psprecomp ABI 어댑터) → `game/dmp1` → `app` 순서다. 각 레이어는 아래 레이어만 링크한다. CMake 타깃 그래프가 이 규칙을 강제한다.
* `repsp`(`third_party/repsp`)는 타이틀 이름, 타이틀 주소, `redmp1` 헤더를 쓰지 않는다. 이 저장소는 repsp의 공개 계약(`include/repsp/`)만 쓰고, repsp의 내부 헤더(`third_party/repsp/src/`)는 include하지 않는다.
* HLE 모듈은 런타임만 링크하고 다른 HLE 모듈을 링크하지 않는다(repsp 규칙).
* 타이틀 코드(재컴파일·디컴파일 코드)는 HLE 모듈을 직접 부르지 않는다. HLE는 repsp의 NID 레지스트리(모듈 import stub → HLE export)를 통해서만 연결된다.
* 플랫폼 계층은 런타임과 타이틀의 타입을 알지 못한다. 기본 값(주소, 크기, 버퍼, 콜백)으로만 호출된다.

## Layer and Dependency Rules

* Layers run bottom to top: `platform` → `repsp` (module, runtime, recomp, hle, gpu) → `recomp_abi` (the psprecomp ABI adapter) → `game/dmp1` → `app`. Each layer links only the layers below it; the CMake target graph enforces this.
* `repsp` (`third_party/repsp`) never names a title, a title's addresses, or `redmp1` headers. This repository uses only repsp's public contracts (`include/repsp/`) and never includes its internal headers (`third_party/repsp/src/`).
* An HLE module links the runtime only, never another HLE module (a repsp rule).
* Title code (recompiled and decompiled) never calls an HLE module directly. HLE is reached only through repsp's NID registry (module import stubs → HLE exports).
* The platform layer knows none of the runtime's or the title's types. It is called with plain values (addresses, sizes, buffers, callbacks).

---

## 구현 규칙

* 플랫폼(OS) 종속 코드는 repsp의 `src/platform/<os>/` 아래에 둔다. 이 저장소의 타이틀·앱 디렉터리에는 플랫폼·아키텍처 하위 디렉터리를 두지 않는다.
* 게스트 주소는 항상 32비트 `GuestAddress`이며 호스트 포인터로 바꿀 때는 repsp의 `GuestMemory::Translate`를 거친다(based 주소 모델). 게스트 주소를 호스트 포인터로 직접 쓰지 않는다.
* 새 기능을 추가할 때는 테스트 전략 또는 최소 검증 절차를 문서에 함께 남긴다.
* 코드 수정이 있는 작업은 영향 범위에 맞는 빌드 검증을 수행하고, 불가능하면 이유를 작업 로그에 남긴다.
* 코딩 스타일 세부 규칙은 `docs/CODING_STYLE.md`에서 관리한다.
* 소스 코드 주석은 영어로만 작성한다. 세부 규칙은 `docs/CODING_STYLE.md`의 주석 언어 항목을 따른다.
* 큰 기능은 하나의 거대 파일에 누적하지 않는다. 독립적으로 이름 붙일 수 있는 하위 시스템은 전용 header/source로 분리하고, 통합 지점에는 orchestration과 adapter 코드만 남긴다.

## Implementation Rules

* Platform (OS) specific code goes under repsp's `src/platform/<os>/`. The title and app directories of this repository have no platform or architecture subdirectories.
* A guest address is always a 32-bit `GuestAddress`, turned into a host pointer only through repsp's `GuestMemory::Translate` (the based address model). Guest addresses are never used directly as host pointers.
* When adding a feature, document the test strategy or minimum verification procedure.
* For tasks that modify code, run build verification appropriate to the impact. If verification is impossible, record the reason in the work log.
* Maintain detailed coding style rules in `docs/CODING_STYLE.md`.
* Write source-code comments in English only, following the comment-language section of `docs/CODING_STYLE.md`.
* Do not accumulate major features in one monolithic file. Give independently named subsystems their own header/source, leaving only orchestration or adapter code at the integration point.

---

## 게임 데이터와 생성 코드 규칙

* `roms/`, ISO, 실행 파일(`EBOOT.BIN`, `BOOT.BIN`), PRX, 게임 자산은 절대 커밋하지 않는다. 사용자가 자기 덤프를 제공한다.
* PSP 복호화 키 자료는 사용자가 제공하며 커밋하지 않는다.
* 재컴파일 파이프라인이 실행 파일에서 기계적으로 생성한 C 코드(`gen/`, `src/game/dmp1/recomp/generated/`)는 커밋하지 않고 로컬에서 다시 만든다.
* 사람이 작성한 디컴파일 코드(`src/game/dmp1/decomp/`)는 커밋한다. 원본 바이트를 그대로 옮긴 표나 자산 덤프는 디컴파일 코드에 넣지 않는다.
* 디스크와 실행 파일의 신원(디스크 ID, 디스크 버전, 실행 파일의 해시)은 코드 식별에 필요한 값이므로 `variants.json`과 프로파일에 둔다. 그 밖의 바이트는 두지 않는다.
* 커밋 전에 `git status`로 게임 데이터가 staging되지 않았는지 확인한다.

## Game Data and Generated Code Rules

* Never commit `roms/`, ISOs, executables (`EBOOT.BIN`, `BOOT.BIN`), PRX modules or game assets. Users supply their own dump.
* PSP decryption key material is supplied by the user and never committed.
* C code the recompilation pipeline generates mechanically from the executable (`gen/`, `src/game/dmp1/recomp/generated/`) is never committed; it is rebuilt locally.
* Human-written decompiled code (`src/game/dmp1/decomp/`) is committed. Do not paste raw original bytes, tables or asset dumps into decompiled code.
* The identity of the disc and the executable (disc ID, disc version, the executable's hashes) is needed to identify the build, so it goes into `variants.json` and the profile; no other bytes do.
* Before committing, check with `git status` that no game data is staged.

---

## 라이선스 규칙

* 프로젝트 라이선스는 `MIT License`다.
* 서드파티 오픈소스는 MIT, BSD, zlib, ISC, Apache-2.0, Boost 같은 허용적 라이선스만 빌드에 포함한다.
* GPL, LGPL, AGPL 같은 전염성 라이선스의 코드는 컴파일·링크하지 않고 저장소에 복사하지도 않는다. 특히 PPSSPP(GPL-2.0+)와 FFmpeg(LGPL/GPL)의 코드는 쓰지 않는다. ATRAC3plus, H.264(PMF 동영상) 같은 코덱은 허용적 라이선스의 구현만 쓰며, 없으면 설계 문서에 대안을 남기고 사용자 확인을 받는다.
* **예외(사용자 결정, 2026-11-14)**: ATRAC3plus와 H.264에는 허용적 라이선스의 구현이 없어, 선택적 코덱 플러그인에 한해 사용자가 따로 설치한 FFmpeg 공유 라이브러리(LGPL 빌드)에 **동적으로** 링크하는 것을 허용한다. 플러그인은 기본 빌드에 들어가지 않고 실행 시에 불러오며, 없으면 무음·빈 프레임으로 동작한다. FFmpeg의 소스와 바이너리는 저장소에 넣지 않으며, 빌드에는 공개 API 헤더만 쓰고 구현 소스는 읽거나 옮기지 않는다. 사용자가 명시적으로 실행하는 스크립트로 고정한 버전의 LGPL 공유 빌드를 받을 수 있게 한다(빌드나 실행 중에 자동으로 받지 않는다). 배포물에 FFmpeg을 함께 실을 때는 LGPL의 조건(고지, 라이선스 사본, 대응 소스 안내, 교체 가능한 동적 링크)을 지킨다. GPL 구성의 FFmpeg 빌드는 받거나 싣지 않는다. 정적 링크와 PPSSPP 코드는 여전히 금지다.
* 전염성 라이선스 프로젝트(PPSSPP 등)는 동작을 이해하는 지식 출처와 동작 비교 oracle로만 참고하고, 코드를 옮기지 않는다([psprecomp의 ORACLE.md](https://github.com/sp00nznet/psprecomp/blob/main/docs/ORACLE.md)와 같은 태도).
* 라이선스가 명시되지 않았거나 LLM 기여를 금지한 프로젝트의 코드는 가져오지 않는다. 공개 문서의 사실은 출처를 달아 참고할 수 있다.
* 서드파티 코드를 가져오면 같은 작업에서 `THIRD_PARTY_NOTICES.md`를 갱신한다. 예외가 필요하면 먼저 문서화하고 사용자 확인을 받는다.
* 가져온 서드파티 코드에서 문제(버그, 이식성)를 찾으면 우리 사본만 고치지 않고 업스트림에 기여한다. repsp의 문제는 [reexec/repsp](https://github.com/reexec/repsp)에, psprecomp의 문제는 [sp00nznet/psprecomp](https://github.com/sp00nznet/psprecomp)에 낸다.

## License Rules

* The project license is the `MIT License`.
* Only permissively licensed third-party code (MIT, BSD, zlib, ISC, Apache-2.0, Boost, ...) is built into the project.
* Copyleft code (GPL, LGPL, AGPL) is neither compiled, linked, nor copied into the repository. In particular no code from PPSSPP (GPL-2.0+) or FFmpeg (LGPL/GPL) is used. Codecs such as ATRAC3plus and H.264 (PMF movies) come only from permissively licensed implementations; when none exists, the design document records the alternatives and the user decides.
* **Exception (user decision, 2026-11-14)**: no permissively licensed implementation of ATRAC3plus or H.264 exists, so an optional codec plugin, and only that, may link **dynamically** against FFmpeg shared libraries (an LGPL build) that the user installs separately. The plugin is not part of the default build and is loaded at run time; without it decoding is silent and blank. FFmpeg's source and binaries are not added to the repository; the build uses only the public API headers, and the implementation source is not read or carried over. A script the user runs explicitly may download a pinned LGPL shared build (never automatically during a build or a run). When FFmpeg is shipped alongside a release, the LGPL's conditions are met (notice, a copy of the licence, a pointer to the corresponding source, replaceable dynamic linking). FFmpeg builds configured as GPL are neither downloaded nor shipped. Static linking and PPSSPP code remain forbidden.
* Copyleft projects (PPSSPP, ...) are consulted only as sources of knowledge about behavior and as behavior oracles; their code is not carried over (the stance of [psprecomp's ORACLE.md](https://github.com/sp00nznet/psprecomp/blob/main/docs/ORACLE.md)).
* Do not take code from projects with no stated license or that forbid LLM contributions. Facts from their public documents may be used with attribution.
* When bringing in third-party code, update `THIRD_PARTY_NOTICES.md` in the same task. If an exception is needed, document it and confirm with the user first.
* When a problem (a bug, a portability issue) is found in third-party code we took, contribute the fix upstream instead of fixing only our copy: repsp problems go to [reexec/repsp](https://github.com/reexec/repsp), psprecomp problems to [sp00nznet/psprecomp](https://github.com/sp00nznet/psprecomp).

---

## 작업 단위 규칙

* task는 GitHub issue로 만든다. task 번호는 GitHub issue 번호를 쓴다.
* 기존 task의 후속 작업(남은 검증, 후속 수정)은 새 issue를 만들지 않고 그 task의 issue에 붙인다.
* 커밋 제목 끝에 관련 issue 번호를 `(#6)`처럼 붙인다.
* 의미 있는 작업마다 하나의 작업 지시 문서를 만든다.
* 작업이 끝나면 대응되는 작업 로그를 남긴다.
* 설계 없이 바로 코드만 추가하지 않는다.
* 초기 구조 작업이라도 디렉터리 목적과 향후 확장 방향을 문서로 남긴다.
* repsp를 바꿔야 하는 작업은 repsp에 issue를 만들어 그쪽 규칙으로 진행하고, 이 저장소의 작업 로그에 그 issue와 올린 submodule 커밋을 적는다.

## Task Unit Rules

* A task is created as a GitHub issue, and the task number is the GitHub issue number.
* Follow-up work on an existing task (verification left open, a follow-up fix) goes to that task's issue rather than a new one.
* End commit titles with the related issue number, as in `(#6)`.
* Create one work-order document for each meaningful task.
* When the task is complete, leave the corresponding work log.
* Do not add code directly without a design.
* Even for initial structure work, document the directory purpose and future extension direction.
* Work that must change repsp opens a repsp issue and proceeds under repsp's rules; this repository's work log records that issue and the submodule commit it moved to.

---

## Git 작업 규칙

* 사용자가 작업을 요청하면 먼저 현재 Git 브랜치명을 확인한다.
* 현재 브랜치가 `main`이면 사용자가 요청한 작업 내용을 바탕으로 작업용 브랜치를 새로 만든 뒤 작업한다.
* 현재 브랜치가 `main`이 아니면 새 브랜치를 만들지 않고 그 브랜치에서 이어서 작업한다. 다른 issue의 작업이어도 같다. 커밋마다 제목 끝에 해당 issue 번호를 붙여 구분한다.
* 작업 단위가 하나 끝날 때마다 관련 변경을 Git 커밋으로 남긴다.
* 프로젝트 버전은 저장소 루트의 `VERSION` 파일에서 `major.minor.patch` 형식으로 관리한다.
* 사용자가 머지를 요청하면 `main`에 머지하기 전에 patch 버전을 1 증가시킨다.
* 사용자가 minor 버전 증가를 요청하면 minor 버전을 1 증가시키고 patch 버전은 0으로 리셋한다.
* 사용자가 major 버전 증가를 요청하면 major 버전을 1 증가시키고 minor와 patch 버전은 0으로 리셋한다.
* 사용자가 머지를 요청하면 작업 브랜치를 원격에 push하고 GitHub pull request를 만든 뒤(머지 요청이 이 push와 PR 생성의 승인이다), 그 PR을 squash merge해 `main`에 넣는다. 머지한 뒤 `git pull`로 로컬 `main`을 원격과 같게 갱신한다.
* PR 본문에는 해결하는 issue를 `Closes #6`처럼 적는다. 여러 issue를 해결하면 issue마다 적는다.
* squash merge의 커밋 제목은 작업 브랜치 안의 커밋 제목들을 확인하고, 전체 변경 내용을 잘 표현하는 제목으로 만든다.
* `main`에 머지한 뒤에는 그 머지 커밋에 `VERSION`과 같은 값의 annotated tag를 `vmajor.minor.patch` 형식으로 붙인다.
* tag 메시지에는 해당 버전의 핵심 변경을 한 줄로 남긴다.
* tag는 로컬까지만 만들고 원격 push는 사용자가 직접 수행한다.
* 머지가 완료되면 현재 작업 브랜치를 삭제한다.
* 릴리스 노트(`docs/release-notes/v<version>.md`)에는 해결된 issue 번호와 그 커밋 ID를 함께 적는다.
* submodule(`third_party/repsp`, `third_party/psprecomp`)은 머지된 커밋 또는 태그에 고정한다. psprecomp 고정을 옮기면 `src/recomp_abi/`를 같은 작업에서 맞춘다. repsp의 작업 브랜치 커밋에 고정하는 것은 그 브랜치가 머지되기 전까지의 임시 상태이며, 릴리스 노트에 적는다.
* `main`에 push되면 Actions(`ci.yml`)가 빌드와 단위 테스트(CTest)를 검증한다. 태그를 push하면 Actions(`release.yml`)가 태그가 `VERSION`과 같고 `main` 위에 있는지 확인한 뒤 같은 빌드·테스트를 하고, `docs/release-notes/v<version>.md`가 있으면 그 내용으로 GitHub Release를 만든다(소스만; 빌드 산출물은 붙이지 않는다). 그러므로 릴리스 노트는 태그를 push하기 전에 `main`에 머지되어 있어야 한다.

## Git Workflow Rules

* When the user requests work, first check the current Git branch name.
* If the current branch is `main`, create a task branch based on the requested work before making changes.
* If the current branch is not `main`, keep working on it without creating a new branch, even for a different issue. Each commit title ends with its own issue number.
* Leave a Git commit for the related changes whenever one task unit is complete.
* Manage the project version in the repository-root `VERSION` file using `major.minor.patch`.
* When the user requests a merge, increment the patch version by 1 before merging into `main`.
* When the user requests a minor version bump, increment the minor version by 1 and reset the patch version to 0.
* When the user requests a major version bump, increment the major version by 1 and reset the minor and patch versions to 0.
* When the user requests a merge, push the task branch, open a GitHub pull request from it (the merge request approves this push and the pull request), and squash-merge it into `main`. Afterwards update local `main` with `git pull`.
* Name the issues the pull request resolves in its description, as in `Closes #6`, one per issue.
* For the squash merge's commit title, inspect the branch's commit titles and write one that describes the complete change.
* After merging into `main`, tag that merge commit with an annotated tag matching `VERSION`, in the form `vmajor.minor.patch`.
* Put a one-line summary of the version's key change in the tag message.
* Create tags locally only; the user pushes them to the remote.
* Delete the task branch after the merge is complete.
* Release notes (`docs/release-notes/v<version>.md`) record the resolved issue numbers together with their commit IDs.
* The submodules (`third_party/repsp`, `third_party/psprecomp`) are pinned to merged commits or tags. Moving the psprecomp pin updates `src/recomp_abi/` in the same task. A pin on a repsp task-branch commit is a temporary state until that branch is merged, and is noted in the release notes.
* A push to `main` is verified by Actions (`ci.yml`): the build and the unit tests (CTest). A pushed tag is verified by Actions (`release.yml`): the tag must equal `VERSION` and sit on `main`, the same build and tests run, and when `docs/release-notes/v<version>.md` exists a GitHub Release is created with it as the body (source only; no built artifacts). The release note must therefore be merged into `main` before the tag is pushed.

---

## 아키텍처 규칙

* 재컴파일된 원본 코드가 항상 기본 실행 경로다. 디컴파일된 함수는 함수 단위로 그 경로를 대체하며, 언제든 재컴파일 버전으로 되돌릴 수 있어야 한다.
* 운영체제(펌웨어), 시스템 라이브러리, 하드웨어 인터페이스만 HLE로 대체한다. 게임이 디스크에서 직접 읽어 올리는 Sony 사용자 모드 PRX(`USRDIR/modules/*.prx`)도 펌웨어 라이브러리로 보고 NID 경계에서 HLE한다.
* 절대적으로 필요하지 않으면 게임플레이 로직을 새로 설계해 다시 쓰지 않는다. 디컴파일은 재작성이 아니라 같은 동작의 읽을 수 있는 표현이다.
* 모든 하위 시스템은 독립적으로 교체 가능해야 한다.

## Architecture Rules

* The recompiled original code is always the default execution path. Decompiled functions replace it function by function, and each can always be switched back to its recompiled version.
* Replace only the operating system (firmware), system libraries and hardware interfaces with HLE. The Sony user-mode PRX modules the game loads from its own disc (`USRDIR/modules/*.prx`) count as firmware libraries and are HLE'd at the NID boundary.
* Do not redesign and rewrite gameplay logic unless absolutely unavoidable. Decompilation is a readable expression of the same behavior, not a rewrite.
* Every subsystem should be replaceable independently.

---

## 개발 철학

여러 구현이 가능할 때는 원본 실행 파일과의 동작 일치를 보존하면서 리버스 엔지니어링 부담을 최소화하는 방법을 선택한다. 런타임 동작은 생성된 코드를 읽어서 추론하지 말고 실행해서 측정한다.

최적화보다 정확성을 우선한다.

## Development Philosophy

When multiple implementations are possible, choose the one that preserves behavioral parity with the original executable while minimizing reverse engineering effort. Measure runtime behavior by running it rather than inferring it from reading generated code.

Accuracy is preferred over optimization.
