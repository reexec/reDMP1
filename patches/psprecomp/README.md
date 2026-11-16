# psprecomp 패치 모음

이 디렉터리는 `third_party/psprecomp`(커밋 `5ca18fa`, upstream `main`)에 적용하는 로컬 패치를 보관한다. 패치는 configure 때 빌드 디렉터리의 복사본에만 적용되고, submodule 자체는 바꾸지 않는다.

## upstream 상태

사용자의 허락(2026-11-16)으로 제출했다. upstream이 받아들이면 해당 로컬 패치를 지우고 submodule을 올린다.

| 로컬 패치 | upstream | 상태 |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | 제출함. 로컬 패치는 PR의 커밋과 같다(테스트 파일만 뺐다). |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31)(다른 기여자) | 같은 수정이 이미 제출돼 있어 우리는 내지 않았다. 로컬 패치는 #31의 조건(`jalr $zero`는 끝으로 본다)에 맞췄다. |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | 제출함. 로컬 패치는 PR의 커밋과 같다(테스트 파일만 뺐다). |

예전의 `0002-seed-harvest-segment-bases.patch`는 0001에 합쳤다.

## 패치

1. **`0001-relocate-prx-at-base.patch`** — PRX를 지정한 주소에 재배치해 옮긴다.
   * `prx_relocate()`: 모든 `PT_LOAD` 세그먼트로 적재된 이미지를 만들고(BSS는 0) `SHT_PRXRELOC`을 지정한 기준 주소로 적용한다(`R_MIPS_32`, `R_MIPS_26`, `HI16`/`LO16` 쌍).
   * `funcs`·`emit`의 `--base <addr>`: 재배치된 이미지에서 함수를 찾고 코드를 낸다. `--base`가 없으면(또는 0이면) upstream의 원래 경로 그대로다.
   * `relocate <file> <base> <out>`: 재배치된 이미지를 파일로 쓴다(런타임이 같은 바이트를 적재한다).
   * 포인터 seed가 재배치 항목의 `ofsbase`·`addrbase` 세그먼트 번호를 반영한다. 재배치된 경우에는 재배치되지 않은 `lui`+`addiu` 즉값 스캔을 건너뛴다.
   * 포인터 seed는 upstream의 `psp_collect_pointer_seeds`(`looks_like_code` 거름)를 그대로 쓴다. 그래서 이 게임에서는 `.text` 밖의 데이터 일부도 함수로 잡힌다(호출되지 않는다; `tools/lift_audit.py`가 따로 센다).
2. **`0003-jalr-fallthrough.patch`** — 본문이 연결하는 `jalr`로 끝나면 다음 명령으로 이어지는 흐름을 낸다. `jr`과 `jalr $zero`는 끝으로 본다.
3. **`0004-j-owned-target-goto.patch`** — 현재 본문이 가진 주소로 가는 `j`는 함수 진입점이라도 `goto`로 옮긴다(뒤로 가면 `PSP_LOOP`). 예: `0x088616C8: j 0x0886167C`(함수 자기 시작으로 돌아가는 루프)가 자기 호출로 옮겨져 호스트 스택이 쌓이던 결함([#32](https://github.com/reexec/reDMP1/issues/32)).

## 패치를 고칠 때

* upstream PR의 커밋을 고친 뒤 `git format-patch -1 <commit> --stdout -- tools/allegrexrecomp`로 로컬 패치를 다시 만든다(테스트 파일은 빌드 복사본에 없으므로 뺀다).
* 다시 생성할 때 `scripts/recomp/regenerate.py`는 가장 최근에 빌드된 `allegrexrecomp`를 쓰고 그 경로를 출력한다. 출력된 경로가 방금 빌드한 것인지 확인한다.

---

# psprecomp patches

This directory keeps the local patches applied to `third_party/psprecomp` (commit `5ca18fa`, upstream `main`). They are applied at configure time to a copy in the build directory; the submodule itself is not modified.

## Upstream status

Submitted with the user's permission (2026-11-16). When upstream accepts one, the matching local patch is removed and the submodule moved forward.

| Local patch | Upstream | Status |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | Submitted; the local patch equals the PR's commit (minus the test file). |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31) (another contributor) | The same fix was already proposed, so we did not submit ours; the local patch follows #31's condition (`jalr $zero` ends a body). |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | Submitted; the local patch equals the PR's commit (minus the test file). |

The former `0002-seed-harvest-segment-bases.patch` is folded into 0001.

## Patches

1. **`0001-relocate-prx-at-base.patch`** — lift a PRX relocated to a chosen base. `prx_relocate()` builds the loaded image from all `PT_LOAD` segments (BSS zeroed) and applies `SHT_PRXRELOC` at the base (`R_MIPS_32`, `R_MIPS_26`, `HI16`/`LO16` pairs). `--base <addr>` on `funcs` and `emit` discovers and emits from the relocated image; without it (or at 0) upstream's original path runs unchanged. `relocate <file> <base> <out>` writes the relocated image (the runtime loads the same bytes). Pointer seeds honour the relocation's `ofsbase` and `addrbase` segment indices, and with a relocated image the unrelocated `lui`+`addiu` immediate scan is skipped. Pointer seeds go through upstream's `psp_collect_pointer_seeds` (the `looks_like_code` filter) unchanged, so for this game some data after `.text` is taken for functions (never called; counted separately by `tools/lift_audit.py`).
2. **`0003-jalr-fallthrough.patch`** — a body ending in a linking `jalr` continues at the next instruction; `jr` and `jalr $zero` end the body.
3. **`0004-j-owned-target-goto.patch`** — a `j` to an address the current body owns becomes a `goto`, even at a function entry (with `PSP_LOOP` when backward). Example: `0x088616C8: j 0x0886167C`, a loop back to the function's own start that was lifted as a self call and grew the host stack ([#32](https://github.com/reexec/reDMP1/issues/32)).

## Changing a patch

Amend the upstream PR's commit, then regenerate the local patch with `git format-patch -1 <commit> --stdout -- tools/allegrexrecomp` (test files are left out, as the build copy has none). When regenerating code, `scripts/recomp/regenerate.py` uses the most recently built `allegrexrecomp` and prints its path; check that it is the one just built.
