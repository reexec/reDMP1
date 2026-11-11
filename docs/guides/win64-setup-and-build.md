# Win64 개발 환경 구축 및 빌드 가이드 (Win64 Setup and Build Guide)

이 문서는 Windows 10/11 64비트 환경에서 reDMP1 프로젝트를 클론하고, 빌드 및 CTest 검증을 수행하는 절차를 설명합니다.

---

## 1. 사전 필수 소프트웨어

1. **Visual Studio 2022** (버전 17.0 이상):
   * 설치 시 워크로드: **C++를 사용한 데스크톱 개발(Desktop development with C++)** 선택
   * 포함 구성요소: MSVC v143 C++ x64/x86 빌드 도구, Windows 10/11 SDK, C++용 CMake 도구
2. **Git**: Git for Windows
3. **Python**: Python 3.10 이상 (표준 라이브러리만 사용)

---

## 2. 저장소 클론 및 서브모듈 초기화

```powershell
git clone https://github.com/reexec/reDMP1.git
cd reDMP1
git submodule update --init --recursive
```

---

## 3. CMakePresets를 이용한 빌드

### Debug 구성 빌드

```powershell
cmake --preset win64-debug
cmake --build build/win64-debug --config Debug
```

### Release 구성 빌드

```powershell
cmake --preset win64-release
cmake --build build/win64-release --config Release
```

또는 제공되는 PowerShell 스크립트를 사용하여 한 번에 빌드 및 테스트를 실행할 수 있습니다:

```powershell
.\scripts\build_win64.ps1 -Config Debug
```

---

## 4. CTest 검증 실행

```powershell
ctest --test-dir build/win64-debug -C Debug --output-on-failure
```

다음 검증 항목들이 통과해야 합니다:
* `app_smoke_no_args`: 인자 없이 실행 시 비정상 종료 확인
* `app_smoke_nonexistent_dir`: 존재하지 않는 경로 거부 확인
* `app_smoke_missing_sfo`: PARAM.SFO 없는 디렉터리 거부 확인
* `app_smoke_unregistered`: 등록되지 않은 디스크 거부 확인
* `repsp` 공유 런타임 검증 프로브 5종 (`memory_probe`, `module_probe`, `nid_probe`, `recomp_contract_probe`, `platform_probe`)

---

## 5. 게임 데이터 등록 및 실행

정품 UMD에서 추출한 파일들을 `roms/` 디렉터리에 배치한 후:

```powershell
# 1. variants.json에 디스크 정보 등록
python scripts/recomp/register_disc.py roms

# 2. C 코드 재생성
python scripts/recomp/regenerate.py roms

# 3. 디스크 식별 실행
.\build\win64-debug\Debug\redmp1.exe roms
```

---

# Win64 Setup and Build Guide (English)

This guide walks through cloning, building, testing, and running reDMP1 on 64-bit Windows.

## 1. Prerequisites

* Visual Studio 2022 with Desktop development with C++
* Git for Windows
* Python 3.10+

## 2. Setup & Build

```powershell
git clone https://github.com/reexec/reDMP1.git
cd reDMP1
git submodule update --init --recursive

cmake --preset win64-debug
cmake --build build/win64-debug --config Debug
ctest --test-dir build/win64-debug -C Debug --output-on-failure
```

## 3. Disc Registration & Running

Place extracted UMD files into `roms/`:
```powershell
python scripts/recomp/register_disc.py roms
python scripts/recomp/regenerate.py roms
.\build\win64-debug\Debug\redmp1.exe roms
```
