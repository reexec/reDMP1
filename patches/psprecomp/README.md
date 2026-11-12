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
     - 지정된 기준 주소로 PRX(ELF32 e_type 0xFFA0)의 `PT_LOAD` 세그먼트 및 `SHT_PRXRELOC` 재배치를 적용하여 인메모리 레이아웃을 생성한 후 함수 발견 및 C 코드 생성을 수행합니다.
     - 독립 서브커맨드 `allegrexrecomp relocate <file> <base> <out>`를 추가하여 재배치된 바이너리 바이트를 추출할 수 있게 합니다.
     - Adds the `--base <addr>` option to `allegrexrecomp funcs` and `emit`.
     - Relocates PRX (`e_type 0xFFA0`) segments using `SHT_PRXRELOC` to the given base address in memory before running function discovery and code emission.
     - Adds `allegrexrecomp relocate <file> <base> <out>` subcommand to output relocated binary bytes.

2. **`0002-seed-harvest-segment-bases.patch`**
   - **설명 / Description**:
     - `container.c`의 `psp_collect_pointer_seeds`에서 `ofsbase` 및 `addrbase` 세그먼트 인덱스를 처리하도록 수정합니다.
     - 기존 코드는 모든 재배치를 세그먼트 0 기준으로 계산하여 세그먼트 1(`addrbase == 1`)을 가리키는 포인터를 잘못된 주소로 해석하거나 누락하는 문제가 있었습니다.
     - Fixes handling of `ofsbase` and `addrbase` segment indices in `psp_collect_pointer_seeds` (`container.c`).
     - Previously, all relocations were assumed relative to segment 0, causing addresses referencing segment 1 (`addrbase == 1`) to be calculated incorrectly or missed.

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
