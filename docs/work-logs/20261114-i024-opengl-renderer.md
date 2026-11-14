# 작업 로그: OpenGL 렌더 백엔드 연결 및 검증 (reDMP1 #24)

* 날짜: 2026-11-14
* 브랜치: `i024-opengl-renderer` (선행: `third_party/repsp` `i024-opengl-backend` #24)
* 설계: [20261114-i024-opengl-renderer.md](../design/20261114-i024-opengl-renderer.md)
* 작업 지시: [20261114-i024-opengl-renderer.md](../work-orders/20261114-i024-opengl-renderer.md)

## 1. 구현 내역

* **앱 CLI 옵션 및 백엔드 연결 ([src/app/main.cpp](../../src/app/main.cpp))**:
  * `--renderer <sw|gl>` (기본값 `sw`): 소프트웨어 백엔드를 기본값 겸 기준 구현으로 유지하고, `gl` 지정 시 `DisplayConfig::use_opengl = true` 및 `DisplayHle::Config::use_opengl = true` 로 OpenGL 3.3 Core 백엔드를 활성화했다.
  * `--scale <n>` (1..8, 기본값 `2`): OpenGL 내부 렌더 타깃 배율(`480*n x 272*n`)을 설정한다. `--renderer sw` 와 함께 쓰면 경고를 출력하고 무시한다.
  * `--headless --renderer gl`: 숨겨진 OpenGL 창 문맥(`DisplayConfig::hidden = true`)을 생성해 헤드리스 실행에서도 OpenGL 렌더링과 `REPSP_FRAME_DUMP` 리드백을 검증할 수 있게 했다.
  * **폴백**: OpenGL 창/문맥 생성 또는 백엔드 초기화에 실패하면 오류 이유를 로그로 남기고 소프트웨어 백엔드(`sw`)로 자동 폴백한다.
  * **종료 보고**: `Renderer:` (`opengl (scale=N)` / `software`), `OpenGL:` (`draws`, `fbo_switches`, `tex_uploads`, `tex_hits`, `approx_blends`, `cpu_overlaps`, `unsupported`), `Present:` (`N frames, interval mean=... ms, max=... ms`) 항목을 추가했다.
* **프레임 BMP 비교 도구 ([tools/compare_bmp.py](../../tools/compare_bmp.py))**:
  * Python 표준 라이브러리만으로 24/32비트 BMP 파일의 픽셀 차이를 계산하는 도구를 추가했다(`--tol`, `--diff-out`).
* **문서 갱신**:
  * [docs/guides/running-and-tracing.md](../guides/running-and-tracing.md): `--renderer gl`, `--scale`, `--headless --renderer gl`, `REPSP_FRAME_DUMP` + `tools/compare_bmp.py` 비교 절차를 추가했다.
  * [docs/guides/win64-setup-and-build.md](../guides/win64-setup-and-build.md): 리뷰어가 반영한 `scripts/build_win64.bat` (`[Debug|Release] [ffmpeg] [notest]`) 사용법과 OpenGL 런타임 요구 사항을 반영했다.
  * [docs/analysis/first-frame.md](../analysis/first-frame.md): §9에 실게임 OpenGL 백엔드 GE 통계, `psm=1`(`ABGR1555`) 대 `RGBA8` 양자화 차이, `--scale 3` 검증 결과를 기록했다.

## 2. 검증 결과

* **Win64 Release + FFmpeg CTest** (`%TEMP%\redmp1-build\redmp1-win64-rel-ff`): **35/35 통과** (`app_boot`, `gl_probe`, `codec_ffmpeg_probe` 포함).
* **Win64 Debug CTest** (`%TEMP%\redmp1-build\redmp1-win64-dbg`): **34/34 통과**.
* **실게임 비교 (`--max-vblanks 700`, `--scale 1`, `play-script.txt`)**:
  * `sw`: `321 frames, interval mean=36.39 ms, max=1050.67 ms`, `187 GE lists`.
  * `gl`: `457 frames, interval mean=25.57 ms, max=1150.51 ms`, `324 GE lists`, `OpenGL: draws=2362 fbo_switches=319 tex_uploads=11 tex_hits=4946 approx_blends=0 cpu_overlaps=0 unsupported=0`.
  * `--renderer gl --scale 3 --max-vblanks 700`: `454 frames, interval mean=25.74 ms, max=1218.51 ms`, `approx_blends=0 cpu_overlaps=0 unsupported=0` 으로 정상 동작 확인.
* **실게임 픽셀 비교 (`--max-vblanks 2100`, `REPSP_FRAME_DUMP`, `--scale 1`)**:
  * `sw` 1002장, `gl` 1473장 덤프.
  * 오프닝 동영상 구간(`frame_0020.bmp`, `frame_0060.bmp`, `frame_0120.bmp`): `max_diff=0`, `exceed(>1)=0` 으로 소프트웨어와 완전 일치.
  * 이후 UI/플레이 구간(`Warning/Intro sw[350] vs gl[550]`, `Title sw[450] vs gl[715]`, `Mode/Song sw[750] vs gl[1030]`, `Gameplay sw[1000] vs gl[1470]`): `gl`이 더 빠르게 프레임를 출력해 프레임 번호 위상차가 발생하며, 게임 프레임버퍼 `psm=1`(`ABGR1555`)을 `sw`는 5비트로 패킹하고 `gl`은 `RGBA8` FBO로 렌더링해 최대 5비트 양자화 차이(`Gameplay` 노트 레인 `(146,50)..(377,270)` 에서 `1916/130560` 픽셀 `>8`)가 관찰되었다.
  * `gl` 덤프(`frame_0060.bmp`, `frame_0715.bmp`, `frame_1470.bmp`) 육안 확인 결과 깨짐 없이 선명하게 출력됨을 확인했다.

## 3. 미실행 항목 (환경 제약)

* `git commit` 및 `third_party/repsp` 서브모듈 커밋: 리뷰어가 수행한다.
* WSL / Linux 빌드: 샌드박스 제약으로 미실행(리뷰어가 수행).
* `scripts/build_win64.bat`(Visual Studio 생성기 경로) 직접 실행: 샌드박스에서는 `vcvars64.bat` + `Ninja` 경로로 검증했다.

---

# Work Log: OpenGL Render Backend Integration & Verification (reDMP1 #24)

* Date: 2026-11-14
* Branch: `i024-opengl-renderer` (prerequisite: `third_party/repsp` `i024-opengl-backend` #24)
* Design: [20261114-i024-opengl-renderer.md](../design/20261114-i024-opengl-renderer.md)
* Work Order: [20261114-i024-opengl-renderer.md](../work-orders/20261114-i024-opengl-renderer.md)

## 1. Implementation Summary

* **App CLI options & backend wiring ([src/app/main.cpp](../../src/app/main.cpp))**:
  * `--renderer <sw|gl>` (default `sw`): keeps the software backend as default and reference; selecting `gl` enables the OpenGL 3.3 Core backend via `DisplayConfig::use_opengl = true` and `DisplayHle::Config::use_opengl = true`.
  * `--scale <n>` (1..8, default `2`): sets the OpenGL internal render-target scale (`480*n x 272*n`); warns and is ignored when `--renderer sw` is active.
  * `--headless --renderer gl`: creates a hidden OpenGL context (`DisplayConfig::hidden = true`) so OpenGL rendering and `REPSP_FRAME_DUMP` readback work in headless runs.
  * **Fallback**: if OpenGL window/context creation or backend initialization fails, logs the reason and falls back to `sw`.
  * **Stop report**: added `Renderer:` (`opengl (scale=N)` / `software`), `OpenGL:` (`draws`, `fbo_switches`, `tex_uploads`, `tex_hits`, `approx_blends`, `cpu_overlaps`, `unsupported`), and `Present:` (`N frames, interval mean=... ms, max=... ms`).
* **Frame BMP comparison tool ([tools/compare_bmp.py](../../tools/compare_bmp.py))**:
  * Added a Python stdlib-only 24/32-bit BMP pixel comparison script (`--tol`, `--diff-out`).
* **Documentation updates**:
  * [docs/guides/running-and-tracing.md](../guides/running-and-tracing.md): documented `--renderer gl`, `--scale`, `--headless --renderer gl`, and `REPSP_FRAME_DUMP` + `tools/compare_bmp.py`.
  * [docs/guides/win64-setup-and-build.md](../guides/win64-setup-and-build.md): documented the reviewer's `scripts/build_win64.bat` (`[Debug|Release] [ffmpeg] [notest]`) options and OpenGL runtime requirements.
  * [docs/analysis/first-frame.md](../analysis/first-frame.md): added §9 recording real-game OpenGL GE stats, `psm=1` (`ABGR1555`) vs `RGBA8` quantization behavior, and `--scale 3` results.

## 2. Verification Results

* **Win64 Release + FFmpeg CTest** (`%TEMP%\redmp1-build\redmp1-win64-rel-ff`): **35/35 passed** (including `app_boot`, `gl_probe`, `codec_ffmpeg_probe`).
* **Win64 Debug CTest** (`%TEMP%\redmp1-build\redmp1-win64-dbg`): **34/34 passed**.
* **Real-game comparison (`--max-vblanks 700`, `--scale 1`, `play-script.txt`)**:
  * `sw`: `321 frames, interval mean=36.39 ms, max=1050.67 ms`, `187 GE lists`.
  * `gl`: `457 frames, interval mean=25.57 ms, max=1150.51 ms`, `324 GE lists`, `OpenGL: draws=2362 fbo_switches=319 tex_uploads=11 tex_hits=4946 approx_blends=0 cpu_overlaps=0 unsupported=0`.
  * `--renderer gl --scale 3 --max-vblanks 700`: `454 frames, interval mean=25.74 ms, max=1218.51 ms`, `approx_blends=0 cpu_overlaps=0 unsupported=0`.
* **Real-game pixel comparison (`--max-vblanks 2100`, `REPSP_FRAME_DUMP`, `--scale 1`)**:
  * `sw` dumped 1002 BMPs, `gl` dumped 1473 BMPs.
  * Opening movie frames (`frame_0020.bmp`, `frame_0060.bmp`, `frame_0120.bmp`): `max_diff=0`, `exceed(>1)=0`.
  * Later UI/gameplay screens (`Warning/Intro sw[350] vs gl[550]`, `Title sw[450] vs gl[715]`, `Mode/Song sw[750] vs gl[1030]`, `Gameplay sw[1000] vs gl[1470]`): `gl` presents more frames within the same VBlank budget (phase offset), and `psm=1` (`ABGR1555`) 5-bit packing in `sw` vs `RGBA8` FBO rendering in `gl` accounts for the remaining differences (`1916/130560` pixels >8 in the gameplay note lane `(146,50)..(377,270)`).
  * Visual inspection of `gl` `frame_0060.bmp`, `frame_0715.bmp`, and `frame_1470.bmp` confirmed clean output with no artifacts.

## 3. Not Run (Environment Constraints)

* `git commit` and `third_party/repsp` submodule commit: left to the reviewer.
* WSL / Linux build: not run in sandbox (left to the reviewer).
* `scripts/build_win64.bat` (Visual Studio generator path): not run directly in sandbox; verified via `vcvars64.bat` + `Ninja`.
