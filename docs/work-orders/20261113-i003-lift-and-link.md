# 작업 지시: Lift & link (issue #3)

설계: [20261113-i003](../design/20261113-i003-lift-and-link.md)
브랜치: `i003-lift-and-link`(main `f44d480`에서)
선행: [repsp issue #3](https://github.com/reexec/repsp/issues/3)(PRX 로더). 4·7단계는 그것이 끝난 뒤. 로컬 개발 중에는 `REDMP1_REPSP_DIR=../repsp`로 형제 체크아웃의 `i003-prx-loader` 브랜치를 쓴다. submodule 고정은 repsp가 머지된 뒤 리뷰어가 옮긴다.

1. allegrexrecomp 패치(설계 §3): `patches/psprecomp/0001-relocate-prx-at-base.patch`(`--base`, `relocate` 부명령), `0002-seed-harvest-segment-bases.patch`, `patches/psprecomp/README.md`. 패치는 `third_party/psprecomp`를 임시 작업 사본(빌드 디렉터리 밖의 임시 폴더 또는 `git worktree`)으로 꺼내 만들고 `git format-patch`로 뽑는다. submodule 작업 트리와 고정 커밋은 바꾸지 않는다. **psprecomp 원격에는 아무것도 push하지 않고, GitHub에 fork·PR·issue를 만들지 않는다.**
2. CMake: configure 때 도구 소스를 빌드 디렉터리로 복사하고 패치를 적용해 `allegrexrecomp`를 빌드한다(실패하면 configure 실패).
3. `src/game/dmp1/recomp/lift.json`(`base` `0x08804000`, `prefix` `dmp1`)과 `regenerate.py`·`lifter.py`의 `--base` 전달. 재생성 후 함수 수·커버리지를 이전(issue #1)과 비교해 기록.
4. `reloc_check`(설계 §4; `roms/`가 있을 때만 CTest에 등록).
5. 어댑터 `src/recomp_abi/`(설계 §5)와 `recomp_abi_probe`. psprecomp에서 옮긴 코드는 출처 표기와 `THIRD_PARTY_NOTICES.md`.
6. `redmp1_recomp` 타깃(설계 §6)과 MSVC 측정.
7. `redmp1 roms --lift-check`(설계 §7).
8. 문서: `docs/analysis/executable.md`(재배치 측정, 재배치 후 수치, 첫 NID), `ARCHITECTURE.md`, `src/recomp_abi/README.md`, `docs/guides/`(재생성과 lift-check 절차), 작업 로그 `docs/work-logs/20261113-i003-lift-and-link.md`.
9. 검증: 설계 §8.

지킬 것: 게임 데이터, 생성 코드, `relocate` 출력은 커밋하지 않는다(커밋 전마다 `git status`). 분석 문서에는 원본 게임의 사실만. 작업 단위마다 커밋, 제목 끝에 `(#3)`. push·PR·merge·tag는 하지 않는다. 작업 지시에 없는 설계 결정은 작업 로그의 "질문"에 적고 멈춘다.

완료 기준: 설계 §8의 1~4와 6.

---

# Work order: lift & link (issue #3)

Design: [20261113-i003](../design/20261113-i003-lift-and-link.md)
Branch: `i003-lift-and-link` (from main `f44d480`)
Depends on: [repsp issue #3](https://github.com/reexec/repsp/issues/3) (the PRX loader) for steps 4 and 7. During local development `REDMP1_REPSP_DIR=../repsp` uses the sibling checkout's `i003-prx-loader` branch; the reviewer moves the submodule pin once repsp is merged.

(1) allegrexrecomp patches (design §3): `patches/psprecomp/0001-relocate-prx-at-base.patch` (`--base`, the `relocate` subcommand), `0002-seed-harvest-segment-bases.patch`, `patches/psprecomp/README.md`, made in a temporary working copy of `third_party/psprecomp` (a temporary folder outside the build tree or a `git worktree`) and extracted with `git format-patch`, leaving the submodule's working tree and pinned commit unchanged. **Nothing is pushed to psprecomp's remote, and no fork, pull request or issue is created on GitHub.** (2) CMake copies the tool sources into the build directory at configure time, applies the patches and builds `allegrexrecomp` (a failure fails the configure). (3) `src/game/dmp1/recomp/lift.json` (`base` `0x08804000`, `prefix` `dmp1`) and passing `--base` through `regenerate.py`/`lifter.py`; after regenerating, compare the function count and coverage with issue #1's and record them. (4) `reloc_check` (design §4; registered in CTest only when `roms/` exists). (5) The adapter `src/recomp_abi/` (design §5) and `recomp_abi_probe`, with code carried from psprecomp attributed and listed in `THIRD_PARTY_NOTICES.md`. (6) The `redmp1_recomp` target (design §6) and the MSVC measurements. (7) `redmp1 roms --lift-check` (design §7). (8) Documents: `docs/analysis/executable.md` (relocation measurements, post-relocation figures, the first NID), `ARCHITECTURE.md`, `src/recomp_abi/README.md`, `docs/guides/` (the regeneration and lift-check procedures), and the work log `docs/work-logs/20261113-i003-lift-and-link.md`. (9) Verify: design §8.

Must hold: game data, generated code and `relocate` output are never committed (`git status` before every commit); analysis documents state facts about the original game only; commit per unit of work with titles ending in `(#3)`; no push, pull request, merge or tag; a design decision the work order does not make goes to the work log's "Questions" and work stops.

Done when: design §8, items 1-4 and 6.

---

## 리뷰 1 (2026-11-13, `ad0b297` 기준)

검증: 재배치 결과를 세 구현으로 비교했다. repsp, 패치된 allegrexrecomp, 그리고 리뷰어가 따로 쓴 참조 구현이다. 3,747,092바이트가 모두 같았다(재배치 37,010개). `--lift-check`는 진입점에서 첫 펌웨어 호출(`0x0880419C` → `ThreadManForUser:0x446D8DE6` `sceKernelCreateThread`)까지 닿았다. submodule 포인터는 커밋에 들어가지 않았다. 아래를 고친다.

| # | 위치 | 문제 | 요청 |
|---|---|---|---|
| R1 | `patches/psprecomp/0001`(재배치 모드 분석) | **재배치가 평행이동이 아니다.** 같은 패치된 lifter에서 기준 주소 0과 `0x08804000`의 함수 집합이 다르다(리뷰어 측정: 0에서 9,062개, `0x08804000`에서 8,784개; 시드 4,399와 4,211). 0에만 있는 289개 중 154개는 재배치 쪽에서 아무 함수에도 속하지 않는다. 원인: 시드 수집이 두 벌이다. 기준 주소 0은 기존 경로(0002로 고친 것, `looks_like_code` 필터)를 타고, 재배치 모드는 0001의 새 코드(필터 없이 `.text` 범위만)를 탄다. | 분석 경로를 하나로 만든다. 기준 주소가 0이어도 재배치를 적용한 이미지를 만들고(0이면 `S = seg[addrbase].vaddr`), 시드는 그 이미지에서 한 가지 코드로만 모은다. 0002의 수정은 그 단일 경로에 녹인다(두 패치가 남아도 되지만 경로는 하나). 로컬 CTest `lift_invariance`(roms가 있을 때)를 더한다. `funcs --list`를 0과 `0x08804000`에서 돌려 주소를 기준 주소만큼 빼면 함수 목록·크기·`reached`가 완전히 같아야 한다. 그 뒤 최종 수치와 issue #1(패치 전) 수치의 차이를 `docs/analysis/executable.md`에 적는다. 거짓 분할이 사라진 예(`0x15C`는 진입 함수 안의 `sw $ra, 8($sp)`이다)와 남은 차이의 원인도 적는다. |
| R2 | `src/recomp_abi/dispatch.c`, `trap.c` | 디스패치 미스와 `psp_unimplemented`가 메시지만 내고 실행을 계속한다(설계 §5는 정지). | repsp의 새 정지 계약(`repsp_raise_stop`, repsp 리뷰 1 R1)으로 멈추고 이유에 주소와 설명을 담는다. 횟수 집계는 남겨도 된다. |
| R3 | `src/recomp_abi/trap.c`, 앱 | "첫 HLE 호출에서 정지"를 어댑터의 `setjmp/longjmp`로 하고 있다. 이 점프는 C 생성 코드를 지나 앱의 C++ 프레임까지 건너뛸 수 있다(정의되지 않은 동작). | 이 전용 장치를 지우고 repsp 경로를 쓴다. `REPSP_STOP_ON_UNIMPLEMENTED`(또는 그와 같은 API 설정)로 NID 레지스트리가 `repsp_raise_stop`을 부르게 하고, 앱은 `GuardedRunResult`의 정지 이유로 첫 NID를 보고한다. |
| R4 | `src/recomp_abi/dispatch.c` | 설계는 코드 범위의 평면 배열이었는데 psprecomp의 해시 표를 옮겼다. 작업 로그에 이 차이가 없다. | 해시 표는 그대로 둬도 된다(검증된 구현). 설계와 다른 선택이라는 점과 이유를 작업 로그와 `ARCHITECTURE.md`에 적는다. 성능 문제가 측정되면 그때 평면 배열로 바꾼다. |
| R5 | `src/recomp_abi/*.c` 머리 주석 | 옮긴 파일의 원문 주석이 인코딩 때문에 깨졌다(예: `psprecomp ??the VFPU`). | 원문의 UTF-8 그대로 둔다(소스 파일 인코딩 UTF-8). |

순서: repsp 리뷰 1(R1, submodule 안 `i003-prx-loader`)을 먼저 하고, 이어서 이 R1~R5를 한다. reDMP1 커밋에는 여전히 `third_party/repsp` 포인터를 넣지 않는다.

완료 기준: R1~R5 반영, `lift_invariance`와 `reloc_check` 통과, `--lift-check`가 repsp 정지 경로로 첫 NID를 보고, Win64 Debug/Release와 Linux x64 빌드·CTest 통과, 작업 로그에 "리뷰 1 반영" 절.

## Review 1 (2026-11-13, at `ad0b297`)

Verification: the relocation matched in all 3,747,092 bytes across three implementations (repsp, the patched allegrexrecomp, and the reviewer's separate reference; 37,010 relocations); `--lift-check` reached the first firmware call from the entry (`0x0880419C` → `ThreadManForUser:0x446D8DE6` `sceKernelCreateThread`); the submodule pointer was kept out of the commits. Fix the items in the table above. R1: relocation is not a pure translation — the same patched lifter finds different function sets at base 0 and at `0x08804000` (reviewer's measurement: 9,062 and 8,784 functions; 4,399 and 4,211 seeds), and 154 of the 289 starts found only at 0 belong to no function in the relocated analysis, because seed harvesting exists twice (base 0 takes the old path fixed by 0002 with its `looks_like_code` filter, the relocated mode takes 0001's new code, which checks only the `.text` range); make one analysis path that always builds the relocated image (at 0, `S = seg[addrbase].vaddr`) and harvests seeds from it with one piece of code, folding 0002's fix into that path; add a local CTest `lift_invariance` requiring `funcs --list` at 0 and at `0x08804000`, shifted by the base, to agree exactly in functions, sizes and `reached`; then record the final figures and their difference from issue #1's (unpatched) in `docs/analysis/executable.md`, with the example of a vanished false split (`0x15C` is `sw $ra, 8($sp)` inside the entry function) and the causes of any remaining difference. R2: dispatch misses and `psp_unimplemented` stop through repsp's new stop contract (`repsp_raise_stop`, repsp review 1 R1) with the address and description as the reason (counters may stay). R3: remove the adapter's own `setjmp/longjmp` stop on the first HLE call; the NID registry stops through `repsp_raise_stop` under `REPSP_STOP_ON_UNIMPLEMENTED` (or the equivalent API setting), and the app reports the first NID from `GuardedRunResult`'s stop reason. R4: keeping psprecomp's hash table instead of the designed flat array is fine (a proven implementation), but record the deviation and its reason in the work log and `ARCHITECTURE.md`; switch to a flat array if performance is measured to need it. R5: keep the carried files' original UTF-8 header comments intact.

Order: repsp review 1 (R1, `i003-prx-loader` inside the submodule) first, then these R1-R5; reDMP1 commits still never include the `third_party/repsp` pointer.

Done when: R1-R5 are in, `lift_invariance` and `reloc_check` pass, `--lift-check` reports the first NID through repsp's stop path, the Win64 Debug/Release and Linux x64 builds and CTest pass, and the work log has a "Review 1" section.
