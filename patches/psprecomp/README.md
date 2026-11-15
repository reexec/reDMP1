# psprecomp Patches / psprecomp 패치 모음

이 디렉터리는 `third_party/psprecomp`(커밋 `5ca18fa`)에 적용하는 로컬 패치를 보관합니다.

This directory maintains local patches applied to `third_party/psprecomp` (target commit `5ca18fa`).

> [!IMPORTANT]
> **업스트림 제출 상태: 사용자 허락 대기 중**  
> **Upstream Submission Status: Awaiting user permission.**  
> psprecomp 원격 저장소에는 어떠한 내용도 직접 push하거나 PR/issue를 생성하지 않으며, 이 패치는 빌드 시점에 복사본에만 적용됩니다.  
> Nothing is pushed to the psprecomp remote repository and no PR/issue is created without explicit user approval. These patches are applied only to a local copy in the build directory at configure time.

---

## 패치 목록 / Patch List

1. **`0001-relocate-prx-at-base.patch`**
   - **설명 / Description**:
     - `allegrexrecomp funcs` 및 `emit`에 `--base <addr>` 옵션을 추가합니다.
     - 기준 주소가 0이든 지정된 주소든 PRX(`e_type 0xFFA0`)는 항상 `prx_relocate`를 통해 인메모리 재배치 이미지를 생성하고, 단일 경로로 시드 수집·함수 발견·C 코드 생성을 수행합니다.
     - 독립 서브커맨드 `allegrexrecomp relocate <file> <base> <out>`를 추가하여 재배치된 바이너리 바이트를 추출할 수 있게 합니다.
     - Adds the `--base <addr>` option to `allegrexrecomp funcs` and `emit`.
     - Always relocates PRX (`e_type 0xFFA0`) segments via `prx_relocate` (even when `base == 0`, with `S = seg[addrbase].vaddr`) and uses a single unified path for seed collection, function discovery, and code emission.
     - Adds `allegrexrecomp relocate <file> <base> <out>` subcommand to output relocated binary bytes.

2. **`0002-seed-harvest-segment-bases.patch`**
   - **설명 / Description**:
     - `container.c`의 `psp_collect_pointer_seeds`에서 `ofsbase` 및 `addrbase` 세그먼트 인덱스를 처리하도록 수정합니다.
     - `analyze.c`에서 재배치 이미지(`an->image != NULL`)가 있는 PRX 분석 시 비재배치 즉시값(`lui+addiu`/`ori`) 선형 스캔을 건너뛰어, 기준 주소에 따라 상수 값이 우연히 코드 범위와 겹쳐 거짓 분할을 일으키는 문제를 방지하고 평행이동 불변성(lift invariance)을 보장합니다.
     - Fixes handling of `ofsbase` and `addrbase` segment indices in `psp_collect_pointer_seeds` (`container.c`).
     - Skips the non-relocated `lui+addiu`/`ori` immediate scan in `analyze.c` when `an->image` is present on PRX modules, preventing un-relocated constants from colliding with `.text` bounds at base 0 and ensuring exact lift invariance across base addresses.

3. **`0003-jalr-fallthrough.patch`**
   - **설명 / Description**:
     - `emit.c`에서 `last_terminal` 판정 시 `in.is_indirect`를 `in.is_call` 여부와 무관하게 terminal로 간주하던 문제를 수정하여, `jalr`(`is_indirect && is_call`)로 끝난 뒤 다음 함수나 레이블로 이어지는 경우 fall-through 호출이 누락되지 않도록 합니다.
     - Fixes `last_terminal` in `emit.c` so `jalr` (`in.is_indirect && in.is_call`) is not treated as terminal like `jr`, preserving fall-through calls into the following function or label when a body ends at `jalr`.

4. **`0004-j-owned-target-goto.patch`**
   - **설명 / Description**:
     - 원본 명령열 예: `0x088616C8: j 0x0886167C` (함수 시작 주소로 되돌아가는 루프).
     - 잘못 옮겨진 방식: `emit.c`의 `mark_labels`와 `j` 출력이 `owned_by(an, in.target, owner) && !is_function(an, in.target)` 조건을 써서, 대상이 현재 본문이 소유한 함수 진입점이면 `goto L_<addr>` 대신 `psp_func_<addr>(); return;` 꼬리 호출을 생성해 MSVC 등에서 매 반복마다 호스트 스택이 쌓였습니다.
     - 고친 방식: `mark_labels`와 `j` 출력에서 `!is_function(an, in.target)` 제한을 제거하여 현재 본문이 소유한 주소로의 `j`는 항상 `goto L_<addr>`(본문 앞 `PSP_ENTER`/`PSP_SP_ENTER`/진입 `switch`를 건너뛴 본문 내부 레이블)로 옮기고, 역방향(`in.target <= a`)이면 `PSP_LOOP`를 함께 출력합니다.
     - Original instruction sequence example: `0x088616C8: j 0x0886167C` (a loop jumping back to the function's own start).
     - How it was mislifted: `mark_labels` and `j` emission in `emit.c` checked `owned_by(an, in.target, owner) && !is_function(an, in.target)`, so jumping to a function entry owned by the current body emitted `psp_func_<addr>(); return;` instead of `goto L_<addr>`, growing the host stack on every iteration under MSVC.
     - How it is fixed: remove `!is_function(an, in.target)` in both `mark_labels` and `j` emission so any `j` targeting an address owned by the current body emits `goto L_<addr>` (landing after `PSP_ENTER`/`PSP_SP_ENTER`/entry `switch`), and emits `PSP_LOOP` when `in.target <= a`.

---

## 업스트림 PR 초안 / Draft Upstream Pull Request

### Title
Add `--base` option to funcs/emit, `relocate` subcommand, and fix segment base handling in seed collection

### Description
This PR improves PRX module support in `allegrexrecomp`:

1. **Relocated lifting (`--base <addr>`)**:
   PSP games often rely on PRX modules relocated at runtime to a base address (e.g., `0x08804000` in user RAM). In these modules, addresses in segment 1 (.data/.bss) have offsets relative to segment 1 start, and pointers stored in data/code must be relocated before analysis. Placing at base 0 without relocation risks missing pointers or misinterpreting data offsets.
   - Added `--base <addr>` to `funcs` and `emit`.
   - Added `relocate <file> <base> <out>` subcommand.

2. **Fix `ofsbase` and `addrbase` handling in `psp_collect_pointer_seeds`**:
   `Elf32_Rel` entries on PRX modules encode `ofsbase` in bits 8-15 and `addrbase` in bits 16-23 of `r_info`. The seed collector now properly uses `seg[ofsbase].offset` and adds `seg[addrbase].addr` when inspecting candidate function pointers.
