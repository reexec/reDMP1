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
