# psprecomp 패치 모음

이 디렉터리는 `third_party/psprecomp`(커밋 `14749c1`, upstream `v0.6.0`)에 적용하는 로컬 패치를 보관한다. 패치는 configure 때 빌드 디렉터리의 복사본에만 적용되고, submodule 자체는 바꾸지 않는다.

## upstream 상태

사용자의 허락(2026-11-16)으로 제출했다. upstream이 받아들이면 해당 로컬 패치를 지우고 submodule을 올린다.

| 로컬 패치 | upstream | 상태 |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | v0.6.0에 들어감. 로컬 패치 삭제. |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31)(다른 기여자) | v0.6.0에 들어감. 로컬 패치 삭제. |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | v0.6.0에 들어감. 로컬 패치 삭제. |
| `0005-emit-trampoline.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | 제출함(2026-11-17), v0.6.0 위로 rebase(`d9278d2`). PR에는 upstream 런타임(`src/dispatch.c`)의 `psp_run`과 테스트가 함께 있으며, 이 저장소의 런타임은 `src/recomp_abi/`에 있다. |
| `0006-loop-hook.patch` | [sp00nznet/psprecomp#38](https://github.com/sp00nznet/psprecomp/pull/38) | v0.6.0에 들어감. 로컬 패치 삭제. |

## 패치

1. **`0005-emit-trampoline.patch`** — `emit`에 `--trampoline` 선택 사항을 추가한다(기본값은 꺼짐이며, 꺼진 상태의 출력은 패치 전과 바이트 단위로 같다). 켜면 비링크 외부 꼬리 호출(`j`, 비링크 `jalr`, 본문 끝 도달)을 호스트 호출 대신 `PSP_TAIL(...)`(간접 점프는 `psp_dispatch_tail`)로 내보내 런타임 트램펄린(`psp_run`)이 호스트 스택을 소비하지 않고 다음 대상으로 이어서 실행하게 한다([#33](https://github.com/reexec/reDMP1/issues/33)). 일반 게스트 호출은 켜도 upstream(#32)처럼 `psp_call_guest_fn`/`psp_call_guest_addr`로 나가고, 이 함수들이 호출 대상을 `psp_run`으로 실행한다.

## 패치를 고칠 때

* upstream PR의 커밋을 고친 뒤 `git format-patch -1 <commit> --stdout -- tools/allegrexrecomp`로 로컬 패치를 다시 만든다(테스트 파일은 빌드 복사본에 없으므로 뺀다).
* 다시 생성할 때 `scripts/recomp/regenerate.py`는 가장 최근에 빌드된 `allegrexrecomp`를 쓰고 그 경로를 출력한다. 출력된 경로가 방금 빌드한 것인지 확인한다.

---

# psprecomp patches

This directory keeps the local patches applied to `third_party/psprecomp` (commit `14749c1`, upstream `v0.6.0`). They are applied at configure time to a copy in the build directory; the submodule itself is not modified.

## Upstream status

Submitted with the user's permission (2026-11-16). When upstream accepts one, the matching local patch is removed and the submodule moved forward.

| Local patch | Upstream | Status |
|---|---|---|
| `0001-relocate-prx-at-base.patch` | [sp00nznet/psprecomp#36](https://github.com/sp00nznet/psprecomp/pull/36) | In v0.6.0. Local patch removed. |
| `0003-jalr-fallthrough.patch` | [sp00nznet/psprecomp#31](https://github.com/sp00nznet/psprecomp/pull/31) (another contributor) | In v0.6.0. Local patch removed. |
| `0004-j-owned-target-goto.patch` | [sp00nznet/psprecomp#35](https://github.com/sp00nznet/psprecomp/pull/35) | In v0.6.0. Local patch removed. |
| `0005-emit-trampoline.patch` | [sp00nznet/psprecomp#37](https://github.com/sp00nznet/psprecomp/pull/37) | Submitted (2026-11-17), rebased onto v0.6.0 (`d9278d2`). The PR also carries `psp_run` in upstream's runtime (`src/dispatch.c`) and tests; this repository's runtime is in `src/recomp_abi/`. |
| `0006-loop-hook.patch` | [sp00nznet/psprecomp#38](https://github.com/sp00nznet/psprecomp/pull/38) | In v0.6.0. Local patch removed. |

## Patches

1. **`0005-emit-trampoline.patch`** — adds an optional `--trampoline` flag to `emit` (off by default, with byte-identical output when off). When enabled, non-linking external tail transfers (`j`, non-linking `jalr`, and body-end fallthrough) are emitted as `PSP_TAIL(...)` (indirect jumps as `psp_dispatch_tail`) instead of host calls so the runtime trampoline (`psp_run`) continues to the next target without consuming host stack ([#33](https://github.com/reexec/reDMP1/issues/33)). With it on, ordinary guest calls still go through `psp_call_guest_fn`/`psp_call_guest_addr` as upstream (#32) emits them, and those run the callee through `psp_run`.

## Changing a patch

Amend the upstream PR's commit, then regenerate the local patch with `git format-patch -1 <commit> --stdout -- tools/allegrexrecomp` (test files are left out, as the build copy has none). When regenerating code, `scripts/recomp/regenerate.py` uses the most recently built `allegrexrecomp` and prints its path; check that it is the one just built.
