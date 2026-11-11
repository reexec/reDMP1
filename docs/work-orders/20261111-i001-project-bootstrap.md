# 작업 지시: 프로젝트 부트스트랩 (issue #1)

설계: [20261111-i001](../design/20261111-i001-project-bootstrap.md)
선행: [repsp v0.0.1](https://github.com/reexec/repsp/pull/2)(머지 커밋 `730c404`, issue #1 완료). 소비자 계약은 `third_party/repsp/ARCHITECTURE.md`, 재컴파일 코드 계약은 `include/repsp/recomp/guest_cpu.h`, `PARAM.SFO` 파서는 `include/repsp/module/param_sfo.h`에 있다.

이미 된 것(main `c601d7f`, 작업 브랜치 `i001-project-bootstrap`): `LICENSE`, `.gitignore`, `.gitattributes`, `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, `docs/CODING_STYLE.md`, `docs/PROJECT_CHARTER.md`, 이 설계와 작업 지시, `docs/analysis/{README,disc-image}.md`.

1. submodule: `third_party/psprecomp`(https://github.com/sp00nznet/psprecomp, 커밋 `5ca18fa`), `third_party/repsp`(https://github.com/reexec/repsp, v0.0.1 머지 커밋 `730c404`). URL은 HTTPS로 적는다(CI가 SSH 키 없이 받도록). 로컬 개발 중에는 형제 체크아웃 `../repsp`를 쓸 수 있게 CMake 옵션(예: `REDMP1_REPSP_DIR`)을 둔다.
2. 빌드 골격: 루트 `CMakeLists.txt`(repsp 소비자 계약: `add_subdirectory` 후 repsp 공개 타깃만 링크; 레이어 타깃 `redmp1_game`, `redmp1` 앱), `CMakePresets.json`(`win64-debug`, `win64-release`, `linux-x64-debug`, `linux-x64-release`), `VERSION`(`0.0.0`).
3. lifter: `allegrexrecomp`를 psprecomp 런타임 타깃 없이 호스트 도구 타깃으로 빌드하고(설계 §3), 그 경로를 `regenerate.py`가 찾을 수 있게 한다.
4. 타이틀 계층: `include/redmp1/dmp1_profile.h`, `src/game/dmp1/`(프로파일, `variants.json` 읽기와 변형 조회; `PARAM.SFO`는 repsp module 계층의 파서를 쓴다), `src/game/dmp1/decomp/README.md`, `src/game/dmp1/recomp/{variants.json,seed_functions.json}`, `src/recomp_abi/README.md`(어댑터의 목적과 issue #2 예정; 코드는 아직 없음).
5. 스크립트: `scripts/build_win64.ps1`/`.bat`(configure + build + ctest), `scripts/build_linux.sh`, `scripts/recomp/register_disc.py`(게임 디렉터리 → `variants.json`), `scripts/recomp/lifter.py`(도구 실행 wrapper), `scripts/recomp/regenerate.py`(lift 입력 선택 → `generated/`). Python은 표준 라이브러리만.
6. 분석: `allegrexrecomp info`로 `BOOT.BIN`과 `EBOOT.BIN`을 비교하고(키가 없으면 `BOOT.BIN`만), `funcs`/`emit` 결과로 `docs/analysis/executable.md`를 새로 쓰고 `docs/analysis/disc-image.md`의 미확정 항목과 색인을 갱신한다. 이 문서들에는 원본 게임에 대한 사실만 적는다(사용자 덤프의 파일 이름·해시·차이는 적지 않는다).
7. 앱: `src/app/main.cpp`(인자 파싱, 식별, 결과 출력; 아직 실행은 없음), 설계 §8-2의 smoke를 CTest에 등록.
8. 문서: `README.md`(reCT3 README 구성: 게임 데이터 불포함, 구조, Win64·Linux 빌드, 플랫폼 상태, 라이선스와 상표 고지), `ARCHITECTURE.md`(구현된 대로), `docs/EXECUTABLE_DESIGN.ko.md`/`.en.md`(자리), `docs/kb/README.md`(색인, 일반 PSP 지식은 repsp를 가리킴), `docs/guides/{README,win64-setup-and-build,disc-image-check}.md`(`disc-image-check`: 사용자가 자기 덤프를 redump #33114 해시와 비교하는 일반 절차), `THIRD_PARTY_NOTICES.md`(psprecomp MIT, repsp MIT과 그 하위 의존성), `CREDITS.md`.
9. CI: `.github/workflows/ci.yml`(Win64 Debug/Release on `windows-2022`, Linux x64 Debug/Release on `ubuntu-24.04`, submodules recursive, 빌드 + CTest; 트리거는 `main` push, `main`으로의 PR, `workflow_dispatch`), `release.yml`(reCT3와 같은 규칙: 태그 = `VERSION`, `main` 위, 릴리스 노트 본문). Linux job은 repsp v0.0.1의 `ci.yml`과 같게 한다: 'Use the Kakao APT mirror' 단계(runner의 `/etc/apt/apt-mirrors.txt`와 `ubuntu.sources`의 URI를 `https://mirror.kakao.com/ubuntu/`로 바꿈; 기본 Azure 미러는 패키지마다 20초씩 멈춘다), SDL3의 X11·Wayland 개발 패키지 목록, `timeout-minutes: 30`. 그 단계와 목록은 repsp의 것을 그대로 옮긴다.
10. 작업 로그: `docs/work-logs/20261111-i001-project-bootstrap.md`(한 일, 검증 결과, 측정 수치, 정한 것과 그 근거, 질문).

커밋: 작업 단위마다 커밋하고 제목 끝에 `(#1)`. 커밋 전 `git status`로 `roms/`, ISO, `generated/`가 없는지 확인한다.

완료 기준: 설계 §8의 1~6.

---

# Work order: project bootstrap (issue #1)

Design: [20261111-i001](../design/20261111-i001-project-bootstrap.md)
Depends on: [repsp v0.0.1](https://github.com/reexec/repsp/pull/2) (merge commit `730c404`, issue #1 done). The consumer contract is in `third_party/repsp/ARCHITECTURE.md`, the recompiled-code contract in `include/repsp/recomp/guest_cpu.h`, the `PARAM.SFO` parser in `include/repsp/module/param_sfo.h`.

Already done (main `c601d7f`, task branch `i001-project-bootstrap`): `LICENSE`, `.gitignore`, `.gitattributes`, `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, `docs/CODING_STYLE.md`, `docs/PROJECT_CHARTER.md`, this design and work order, `docs/analysis/{README,disc-image}.md`.

(1) Submodules: `third_party/psprecomp` (https://github.com/sp00nznet/psprecomp, commit `5ca18fa`) and `third_party/repsp` (https://github.com/reexec/repsp, the v0.0.1 merge commit `730c404`), with HTTPS URLs so CI fetches without SSH keys; a CMake option (for example `REDMP1_REPSP_DIR`) lets local development use the sibling checkout `../repsp`. (2) Build skeleton: the root `CMakeLists.txt` (repsp consumer contract: `add_subdirectory`, then link only repsp's public targets; layer targets `redmp1_game` and the `redmp1` app), `CMakePresets.json` (`win64-debug`, `win64-release`, `linux-x64-debug`, `linux-x64-release`), `VERSION` (`0.0.0`). (3) Lifter: build `allegrexrecomp` as a host tool target without psprecomp's runtime target (design §3) and make its path findable by `regenerate.py`. (4) Title layer: `include/redmp1/dmp1_profile.h`, `src/game/dmp1/` (profile, reading `variants.json` and looking up the variant; `PARAM.SFO` through repsp's module-layer parser), `src/game/dmp1/decomp/README.md`, `src/game/dmp1/recomp/{variants.json,seed_functions.json}`, `src/recomp_abi/README.md` (the adapter's purpose, planned for issue #2; no code yet). (5) Scripts: `scripts/build_win64.ps1`/`.bat` (configure + build + ctest), `scripts/build_linux.sh`, `scripts/recomp/register_disc.py` (game directory → `variants.json`), `scripts/recomp/lifter.py` (tool wrapper), `scripts/recomp/regenerate.py` (choose the lift input → `generated/`); standard-library Python only. (6) Analysis: compare `BOOT.BIN` and `EBOOT.BIN` with `allegrexrecomp info` (only `BOOT.BIN` without keys), write `docs/analysis/executable.md` from the `funcs`/`emit` results, and update the unresolved item of `docs/analysis/disc-image.md` and the index. These documents state facts about the original game only (never the user's dump file names, hashes or differences). (7) App: `src/app/main.cpp` (argument parsing, identification, printing the result; no execution yet), with the design §8-2 smoke checks registered in CTest. (8) Documents: `README.md` (reCT3's README outline: no game data, structure, Win64 and Linux builds, platform status, license and trademark notice), `ARCHITECTURE.md` (as implemented), `docs/EXECUTABLE_DESIGN.ko.md`/`.en.md` (placeholders), `docs/kb/README.md` (index pointing at repsp for general PSP knowledge), `docs/guides/{README,win64-setup-and-build,disc-image-check}.md` (`disc-image-check`: the general procedure for comparing one's own dump with the redump #33114 hashes), `THIRD_PARTY_NOTICES.md` (psprecomp MIT, repsp MIT and its dependencies), `CREDITS.md`. (9) CI: `.github/workflows/ci.yml` (Win64 Debug/Release on `windows-2022`, Linux x64 Debug/Release on `ubuntu-24.04`, recursive submodules, build + CTest; triggered by pushes to `main`, pull requests into `main` and `workflow_dispatch`) and `release.yml` (reCT3's rules: tag = `VERSION`, on `main`, release note as the body). The Linux jobs match repsp v0.0.1's `ci.yml`: the 'Use the Kakao APT mirror' step (rewriting the runner's `/etc/apt/apt-mirrors.txt` and the `ubuntu.sources` URIs to `https://mirror.kakao.com/ubuntu/`; the default Azure mirror stalls 20 s per package), SDL3's X11 and Wayland development package list, and `timeout-minutes: 30`, carried over unchanged from repsp. (10) Work log: `docs/work-logs/20261111-i001-project-bootstrap.md` (what was done, verification results, measurements, decisions and their grounds, questions).

Commits: one per unit of work, titles ending in `(#1)`. Before each commit check with `git status` that `roms/`, ISOs and `generated/` are absent.

Done when: design §8, items 1-6.

---

## 리뷰 1 (2026-11-12, `57cb3df` 기준)

검증: 리뷰어가 깨끗한 빌드 디렉터리에서 Win64 Debug/Release 빌드와 CTest 10/10, Linux x64 Release(WSL Ubuntu 24.04) 빌드와 CTest 10/10, `redmp1 roms` 식별 성공(`ULKS46050` 1.02, `BOOT.BIN` SHA-1 일치)을 다시 확인했다. 게임 데이터와 `generated/`(147개 파일)는 추적되지 않는다. 분석 문서에 사용자 덤프 정보는 없다. 아래를 고친다.

| # | 위치 | 문제 | 요청 |
|---|---|---|---|
| R1 | `docs/analysis/disc-image.md`, `docs/analysis/executable.md` | `~PSP` 헤더 값(ELF 크기, 진입점, 세그먼트, BSS)이 `BOOT.BIN`과 같다는 것에서 "코드와 데이터가 완전히 동일"을 **확인됨**으로 적었다. 바이트 비교는 하지 않았다(psprecomp는 복호화를 아직 하지 않는다). | 헤더 값 일치는 **확인됨**, 내용이 동일하다는 결론은 **추정**으로 적는다. lift 입력이 `BOOT.BIN`인 근거(키가 필요 없고 헤더가 일치)는 그대로 둔다. "남은 확인"에 "복호화가 가능해지면(psprecomp의 복호화 또는 사용자 키) 바이트 단위로 비교"를 남긴다. `disc-image.md`의 미확정 절도 "(없음)"이 아니라 이 항목으로 바꾼다. |
| R2 | `CMakeLists.txt`의 `app_smoke_unregistered` | 내용이 `INVALID_SFO_DATA`인 파일이라 "등록되지 않은 `DISC_ID`"가 아니라 "깨진 SFO"를 검사한다(설계 §8-2). | 이 검사는 `app_smoke_invalid_sfo`로 이름을 바꿔 둔다. 그리고 `DISC_ID`가 다른 올바른 합성 `PARAM.SFO`(예: `TEST00000`, 1.00)로 `app_smoke_unregistered`를 새로 만든다. 합성 파일은 configure 때 작은 생성기(CMake 또는 표준 라이브러리 Python)로 만들고, 종료 코드가 "미등록" 상태 값인지까지 확인한다. |
| R3 | `CMakeLists.txt` | `set(REPSP_BUILD_PROBES ON CACHE BOOL ... FORCE)`가 사용자 설정을 덮어쓴다. | `FORCE`를 뺀다(기본 ON, 사용자가 끌 수 있게). |
| R4 | `src/app/main.cpp` | `--version`이 `0.0.0`을 하드코딩한다. | `VERSION` 파일에서 온 값을 CMake compile definition으로 넘긴다. |
| R5 | `CMakeLists.txt`의 `allegrexrecomp` | 소스 목록이 손으로 적혀 있어 psprecomp 고정을 옮길 때 파일이 늘면 깨진다. | `tools/allegrexrecomp/*.c`와 `crypto/*.c`를 `file(GLOB ... CONFIGURE_DEPENDS)`로 모은다(psprecomp 런타임 `src/`는 여전히 넣지 않는다). |

완료 기준: R1~R5 반영, Win64 Debug/Release와 Linux x64 빌드·CTest 통과, 작업 로그에 "리뷰 1 반영" 절 추가.

## Review 1 (2026-11-12, at `57cb3df`)

Verification: from clean build directories the reviewer re-ran the Win64 Debug/Release builds with CTest 10/10, the Linux x64 Release build (WSL Ubuntu 24.04) with CTest 10/10, and `redmp1 roms` identification (`ULKS46050` 1.02, `BOOT.BIN` SHA-1 matches). Game data and `generated/` (147 files) are untracked, and the analysis documents carry no user-dump information. Fix the items in the table above: R1 the matching `~PSP` header values are **confirmed** but "identical code and data" is **inferred** (no byte comparison was made; psprecomp does not decrypt yet), keeping the grounds for lifting `BOOT.BIN`, adding "compare byte by byte once decryption is possible" to the open checks, and turning the unresolved section of `disc-image.md` into that item; R2 `app_smoke_unregistered` feeds a broken SFO, so rename it `app_smoke_invalid_sfo` and add a real `app_smoke_unregistered` with a valid synthetic `PARAM.SFO` of another `DISC_ID` (for example `TEST00000`, 1.00) generated at configure time, checking the exit code is the "unregistered" status; R3 drop `FORCE` from `REPSP_BUILD_PROBES`; R4 pass the version from `VERSION` as a compile definition instead of hard-coding `0.0.0`; R5 collect the `allegrexrecomp` sources with `file(GLOB ... CONFIGURE_DEPENDS)` over `tools/allegrexrecomp/*.c` and `crypto/*.c` (still never psprecomp's runtime `src/`).

Done when: R1-R5 are in, the Win64 Debug/Release and Linux x64 builds and CTest pass, and the work log has a "Review 1" section.
