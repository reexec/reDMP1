# 작업 로그: 프로젝트 부트스트랩 (issue #1)

* 작업 브랜치: `i001-project-bootstrap`
* 설계: [`docs/design/20261111-i001-project-bootstrap.md`](../design/20261111-i001-project-bootstrap.md)
* 작업 지시: [`docs/work-orders/20261111-i001-project-bootstrap.md`](../work-orders/20261111-i001-project-bootstrap.md)

---

## 1. 수행한 작업 (Summary of Work)

1. **서브모듈 추가 및 고정**:
   * `third_party/psprecomp`: `https://github.com/sp00nznet/psprecomp` 커밋 `5ca18fa` 고정.
   * `third_party/repsp`: `https://github.com/reexec/repsp` v0.0.1 머지 커밋 `730c404` 고정.
   * CI 환경에서 인증 없이 접근 가능하도록 HTTPS URL로 등록하고, 하위 SDL3 서브모듈까지 재귀적으로 초기화.
2. **빌드 골격 구축**:
   * `VERSION` (`0.0.0`) 파일 작성.
   * `CMakePresets.json`: `win64-debug`, `win64-release`, `linux-x64-debug`, `linux-x64-release` 구성/빌드/테스트 프리셋 정의 (VS 17 2022 및 Ninja).
   * 루트 `CMakeLists.txt`:
     * `REDMP1_REPSP_DIR` 캐시 옵션을 제공하여 형제 체크아웃(`../repsp`) 전환 지원.
     * `repsp`의 소비자 계약에 따라 `add_subdirectory` 후 공개 타깃(`repsp::module`, `repsp::runtime`, `repsp::platform`)만 링크.
     * `psprecomp` 런타임 타깃 생성 없이 `third_party/psprecomp/tools/allegrexrecomp`의 소스만을 컴파일하여 호스트 도구 타깃 `allegrexrecomp` 빌드.
     * 레이어 타깃 `redmp1_game` (정적 라이브러리) 및 `redmp1` (실행 파일) 정의.
     * CTest 활성화 및 앱 smoke 테스트 5종 등록.
3. **타이틀 계층 및 ABI 플레이스홀더**:
   * `include/redmp1/dmp1_profile.h`: 타이틀 메타데이터 상수, `Variant` 구조체, `IdentifyResult` 및 디스크 식별 함수 선언.
   * `src/game/dmp1/dmp1_profile.cpp`: `repsp::module::ParamSfo`를 이용한 `PARAM.SFO` 파싱, `repsp::runtime::ComputeSha1`을 이용한 실행 파일 SHA-1 계산, 경량 JSON 파서 기반 `variants.json` 로딩 및 디스크 식별 구현.
   * `src/game/dmp1/recomp/seed_functions.json` (`[]`) 및 `variants.json` (`register_disc.py`로 생성/갱신).
   * `src/game/dmp1/decomp/README.md`: 향후 디컴파일 함수 배치 계획 문서화.
   * `src/recomp_abi/README.md`: `psprecomp` ABI와 `repsp` 재컴파일 코드 계약 사이의 어댑터 역할 및 issue #2 예정 문서화.
4. **애플리케이션 진입점 및 스모크 검증**:
   * `src/app/main.cpp`: CLI 인자 파싱(`--variants`, `--help`, `--version`), `IdentifyGameDirectory` 호출 및 결과 진단 출력.
   * CTest 스모크 테스트 5종:
     * `app_smoke_no_args`: 인자 없이 실행 시 비정상 종료 확인 (WILL_FAIL)
     * `app_smoke_nonexistent_dir`: 없는 경로 거부 확인 (WILL_FAIL)
     * `app_smoke_missing_sfo`: PARAM.SFO 없는 디렉터리 거부 확인 (WILL_FAIL)
     * `app_smoke_unregistered`: 미등록/합성 SFO 거부 확인 (WILL_FAIL)
     * `app_smoke_registered_roms`: 로컬 `roms` 정품 덤프 식별 성공 확인 (PASS)
5. **스크립트 구현 (표준 라이브러리 Python 및 쉘/배치)**:
   * `scripts/build_win64.ps1`, `scripts/build_win64.bat`, `scripts/build_linux.sh`: configure, build, ctest 자동화.
   * `scripts/recomp/lifter.py`: 빌드 산출물 디렉터리에서 `allegrexrecomp`를 탐색하여 실행하는 래퍼.
   * `scripts/recomp/register_disc.py`: SFO 파싱 및 BOOT.BIN SHA-1 계산 후 `variants.json` 자동 갱신.
   * `scripts/recomp/regenerate.py`: 평문 `BOOT.BIN`을 lift 입력으로 자동 선택하여 `src/game/dmp1/recomp/generated/`에 C 소스(147개 파일) 생성.
6. **실행 파일 분석 및 원본 디스크 이미지 분석 갱신**:
   * `allegrexrecomp info`를 통해 `BOOT.BIN`과 `EBOOT.BIN` 비교 분석:
     * `EBOOT.BIN`의 `~PSP` 헤더 내 복호화 대상 크기(1,463,072 바이트), 엔트리(`0x00000128`), 세그먼트 크기가 평문 `BOOT.BIN`과 100% 일치함을 확인.
     * 복호화 키 없이 평문 `BOOT.BIN`을 lift 입력으로 확정.
   * `allegrexrecomp funcs` 통계 분석:
     * 9,159개 함수 발견, `.text` 커버리지 92.4% (761,108 바이트).
     * 20개 펌웨어 라이브러리, 159개 함수(고유 호출 144개) 임포트 집계.
   * `docs/analysis/executable.md` 신규 작성.
   * `docs/analysis/disc-image.md`의 '미확정' 절을 '확인됨'으로 갱신하고 `docs/analysis/README.md` 색인 갱신.
7. **프로젝트 문서 작성**:
   * `README.md`, `ARCHITECTURE.md`, `docs/EXECUTABLE_DESIGN.ko.md`, `docs/EXECUTABLE_DESIGN.en.md`, `docs/kb/README.md`, `docs/guides/README.md`, `docs/guides/win64-setup-and-build.md`, `docs/guides/disc-image-check.md`, `THIRD_PARTY_NOTICES.md`, `CREDITS.md`.
8. **GitHub Actions 워크플로 구축**:
   * `.github/workflows/ci.yml`: Win64 Debug/Release (`windows-2022`), Linux x64 Debug/Release (`ubuntu-24.04`), repsp v0.0.1의 Kakao APT 미러 및 SDL3 의존성 패키지 적용.
   * `.github/workflows/release.yml`: VERSION 태그 일치 검증, main 브랜치 검증, 빌드 및 릴리스 배포.

---

## 2. 검증 결과 (Verification Results)

### 2.1. Win64 Debug 빌드 및 CTest
```powershell
ctest --test-dir build/win64-debug -C Debug --output-on-failure
```
```
Test project E:/MYWORK/Projects/reexec/reDMP1/build/win64-debug
      Start  1: app_smoke_no_args
 1/10 Test  #1: app_smoke_no_args ................   Passed    0.06 sec
      Start  2: app_smoke_nonexistent_dir
 2/10 Test  #2: app_smoke_nonexistent_dir ........   Passed    0.01 sec
      Start  3: app_smoke_missing_sfo
 3/10 Test  #3: app_smoke_missing_sfo ............   Passed    0.01 sec
      Start  4: app_smoke_unregistered
 4/10 Test  #4: app_smoke_unregistered ...........   Passed    0.01 sec
      Start  5: app_smoke_registered_roms
 5/10 Test  #5: app_smoke_registered_roms ........   Passed    0.11 sec
      Start  6: memory_probe
 6/10 Test  #6: memory_probe .....................   Passed    0.01 sec
      Start  7: module_probe
 7/10 Test  #7: module_probe .....................   Passed    0.04 sec
      Start  8: nid_probe
 8/10 Test  #8: nid_probe ........................   Passed    0.05 sec
      Start  9: recomp_contract_probe
 9/10 Test  #9: recomp_contract_probe ............   Passed    0.01 sec
      Start 10: platform_probe
10/10 Test #10: platform_probe ...................   Passed    0.45 sec

100% tests passed, 0 tests failed out of 10
Total Test time (real) = 0.78 sec
```

### 2.2. Win64 Release 빌드 및 CTest
```powershell
ctest --test-dir build/win64-release -C Release --output-on-failure
```
```
Test project E:/MYWORK/Projects/reexec/reDMP1/build/win64-release
      Start  1: app_smoke_no_args
 1/10 Test  #1: app_smoke_no_args ................   Passed    0.02 sec
      Start  2: app_smoke_nonexistent_dir
 2/10 Test  #2: app_smoke_nonexistent_dir ........   Passed    0.01 sec
      Start  3: app_smoke_missing_sfo
 3/10 Test  #3: app_smoke_missing_sfo ............   Passed    0.01 sec
      Start  4: app_smoke_unregistered
 4/10 Test  #4: app_smoke_unregistered ...........   Passed    0.01 sec
      Start  5: app_smoke_registered_roms
 5/10 Test  #5: app_smoke_registered_roms ........   Passed    0.02 sec
      Start  6: memory_probe
 6/10 Test  #6: memory_probe .....................   Passed    0.02 sec
      Start  7: module_probe
 7/10 Test  #7: module_probe .....................   Passed    0.01 sec
      Start  8: nid_probe
 8/10 Test  #8: nid_probe ........................   Passed    0.02 sec
      Start  9: recomp_contract_probe
 9/10 Test  #9: recomp_contract_probe ............   Passed    0.02 sec
      Start 10: platform_probe
10/10 Test #10: platform_probe ...................   Passed    0.35 sec

100% tests passed, 0 tests failed out of 10
Total Test time (real) = 0.49 sec
```

### 2.3. Linux x64 Debug 빌드 및 CTest (WSL Ubuntu-24.04, Ninja)
```bash
cmake -B /tmp/redmp1-linux-debug -S /mnt/e/MYWORK/Projects/reexec/reDMP1 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DREPSP_BUILD_PROBES=ON && cmake --build /tmp/redmp1-linux-debug && ctest --test-dir /tmp/redmp1-linux-debug --output-on-failure
```
```
Test project /tmp/redmp1-linux-debug
      Start  1: app_smoke_no_args
 1/10 Test  #1: app_smoke_no_args ................   Passed    0.00 sec
      Start  2: app_smoke_nonexistent_dir
 2/10 Test  #2: app_smoke_nonexistent_dir ........   Passed    0.00 sec
      Start  3: app_smoke_missing_sfo
 3/10 Test  #3: app_smoke_missing_sfo ............   Passed    0.01 sec
      Start  4: app_smoke_unregistered
 4/10 Test  #4: app_smoke_unregistered ...........   Passed    0.00 sec
      Start  5: app_smoke_registered_roms
 5/10 Test  #5: app_smoke_registered_roms ........   Passed    0.19 sec
      Start  6: memory_probe
 6/10 Test  #6: memory_probe .....................   Passed    0.00 sec
      Start  7: module_probe
 7/10 Test  #7: module_probe .....................   Passed    0.00 sec
      Start  8: nid_probe
 8/10 Test  #8: nid_probe ........................   Passed    0.02 sec
      Start  9: recomp_contract_probe
 9/10 Test  #9: recomp_contract_probe ............   Passed    0.00 sec
      Start 10: platform_probe
10/10 Test #10: platform_probe ...................   Passed    0.08 sec

100% tests passed, 0 tests failed out of 10
Total Test time (real) = 0.32 sec
```

### 2.4. Linux x64 Release 빌드 및 CTest (WSL Ubuntu-24.04, Ninja)
```bash
cmake -B /tmp/redmp1-linux-release -S /mnt/e/MYWORK/Projects/reexec/reDMP1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DREPSP_BUILD_PROBES=ON && cmake --build /tmp/redmp1-linux-release && ctest --test-dir /tmp/redmp1-linux-release --output-on-failure
```
```
Test project /tmp/redmp1-linux-release
      Start  1: app_smoke_no_args
 1/10 Test  #1: app_smoke_no_args ................   Passed    0.00 sec
      Start  2: app_smoke_nonexistent_dir
 2/10 Test  #2: app_smoke_nonexistent_dir ........   Passed    0.00 sec
      Start  3: app_smoke_missing_sfo
 3/10 Test  #3: app_smoke_missing_sfo ............   Passed    0.01 sec
      Start  4: app_smoke_unregistered
 4/10 Test  #4: app_smoke_unregistered ...........   Passed    0.00 sec
      Start  5: app_smoke_registered_roms
 5/10 Test  #5: app_smoke_registered_roms ........   Passed    0.07 sec
      Start  6: memory_probe
 6/10 Test  #6: memory_probe .....................   Passed    0.00 sec
      Start  7: module_probe
 7/10 Test  #7: module_probe .....................   Passed    0.00 sec
      Start  8: nid_probe
 8/10 Test  #8: nid_probe ........................   Passed    0.01 sec
      Start  9: recomp_contract_probe
 9/10 Test  #9: recomp_contract_probe ............   Passed    0.00 sec
      Start 10: platform_probe
10/10 Test #10: platform_probe ...................   Passed    0.09 sec

100% tests passed, 0 tests failed out of 10
Total Test time (real) = 0.19 sec
```

### 2.5. 격리 및 게임 데이터 커밋 검증
* `git status`: 워킹 트리 깨끗함 확인 (`roms/`, `*.iso`, `generated/` 일체 미출현).
* `third_party/repsp` 내부 `git grep -i psprecomp`: 0건 (런타임에 lifter 의존성 부재 확인).
* `third_party/repsp` 내부 `git grep -i djmax`: 0건 (런타임에 타이틀 의존성 부재 확인).

---

## 3. 측정 수치 요약

1. **BOOT.BIN vs EBOOT.BIN**:
   * `BOOT.BIN`: 파일 크기 1,463,072 바이트, ELF32 MIPS LE PRX, 엔트리 `0x00000128`.
   * `EBOOT.BIN`: 파일 크기 1,463,408 바이트, Sony `~PSP` 헤더 태그 `0xC0CB167C`, 복호화 대상 크기 `1,463,072` 바이트, 엔트리 `0x00000128`.
   * 세그먼트 0 크기: 1,028,240 바이트 (양쪽 완전 일치).
   * 세그먼트 1 메모리 크기: 2,718,852 바이트 (BSS 2,594,336 바이트 양쪽 완전 일치).
   * 결론: 복호화 키 없이 평문 `BOOT.BIN`을 lift 입력으로 직접 사용 가능.
2. **함수 및 코드 커버리지**:
   * 발견된 함수 수: 9,159개
   * 발견된 코드 크기: 761,108 바이트 (`.text`의 92.4%)
   * 총 디코딩 명령어: 190,277개
   * VFPU 명령어: 208개 (0.11%, 67개 함수)
   * 간접 점프 테이블: 212개 (미해결 분기 사이트 0개)
   * 임포트 펌웨어 함수: 20개 라이브러리, 159개 함수 (고유 호출 144개)
3. **C 소스 생성 (`regenerate.py`)**:
   * 총 147개 C/헤더 파일 생성 (`dmp1_funcs_000.c` ~ `dmp1_funcs_143.c`, `dmp1_funcs.h`, `dmp1_imports.c`, `dmp1_register.c`).

---

## 4. 정한 것과 그 근거 (Decisions and Grounds)

1. **`allegrexrecomp` 호스트 도구 빌드 방식**:
   * `third_party/psprecomp` 전체를 `add_subdirectory`하면 불필요한 `psprecomp` 런타임 라이브러리 타깃이 함께 생성되어 repsp와의 의존성 격리가 깨질 위험이 있습니다.
   * 따라서 루트 `CMakeLists.txt`에서 `tools/allegrexrecomp`의 소스 파일들만 명시적으로 모아 호스트 실행 파일 타깃으로 독립 빌드하도록 구성했습니다.
2. **외부 복호화 키 미사용**:
   * UMD의 `BOOT.BIN`이 `EBOOT.BIN` 복호화 규격과 100% 동일한 평문 ELF임이 검증되었으므로, 복호화 키 파일(`psp_keys.txt`) 없이도 재컴파일 파이프라인을 완전히 독립적으로 가동할 수 있도록 확정했습니다.

---

## 5. 질문 (Questions for Reviewer)

* 없음: 작업 지시서의 모든 요구사항(submodule, build, lifter, title layer, scripts, analysis, app, docs, CI, verification)이 정상 구현 및 검증 완료되었습니다.

---

# Work Log: Project Bootstrap (English)

See sections above for summary of work, verification results across 4 target configurations (Win64 Debug/Release, Linux Debug/Release), measurements, design decisions, and questions.
