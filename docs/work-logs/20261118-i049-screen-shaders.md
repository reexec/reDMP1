# 작업 로그: 화면 셰이더(Screen Shader) 도입 (`#49` / `repsp #50`)

* 날짜: 2026-11-18
* 브랜치: `reDMP1` (`i049-screen-shaders`), `third_party/repsp` (`i50-screen-shaders`)
* 관련 설계: [20261118-i049-screen-shaders.md](../design/20261118-i049-screen-shaders.md)
* 관련 작업 지시: [20261118-i049-screen-shaders.md](../work-orders/20261118-i049-screen-shaders.md)

---

## 1. 구현 요약

### 단위 1·2 (`third_party/repsp`, 커밋 `9c593ed`, `e01d630`)
* **GL 비의존 카탈로그·파서·GLSL 변환기**: `post_shader_source`, `post_shader_preset`, `post_shader_catalog`를 추가하여 `#pragma parameter`, `.glslp` 다중 패스 프리셋, GL 3.3 Core / GLES 3.0 변환(선행 `#version` 제거, `#version 330 core` 삽입, `attribute`/`varying`/`texture2D` 호환 매크로, `gl_FragColor` 토큰 치환) 및 `ChoosePostShader` 우선순위 함수를 구현했습니다.
* **내장 셰이더 포함**: `crt`, `scanline`, `fsr`, `scalefx`, `anime4k`를 `src/platform/post_shaders/`에 두고 CMake 구성 시 `post_shader_builtins.inc`로 임베드했습니다. `crt.glsl`과 `scanline.glsl`은 내부 렌더링 배율(`TextureSize`)과 무관하게 PSP 논리 해상도(`272`줄)를 기준으로 동작하도록 `LogicalSize` uniform을 사용하도록 수정했습니다.
* **OpenGL 3.3 Core 후처리 패스**: `OpenGlPostProcess`를 구현하고 `SdlDisplayBackend`의 `DrawTexture` 및 `DrawCpuRgba8`에 연결했습니다. 다중 패스 FBO(`RGBA8`/`RGBA16F`), `OrigTexture`/`PassPrevNTexture`/`LogicalSize`/`TextureSize`/`InputSize`/`OutputSize` uniform, 실패 시 `none` 폴백 및 `last_error()`, `PostShaderControl` API를 구현했습니다.

### 단위 3 (`reDMP1`, 작업 트리)
* `src/app/settings.h`, `src/app/settings.cpp`: `Settings::post_shader` (`"none"` 기본값) 및 `redmp1.ini` `[Graphics]` `PostShader=` 읽기/저장 추가.
* `src/app/settings_ui.h`, `src/app/settings_ui.cpp`: `Graphics` 탭에 `Screen shader` 섹션(`Shader` 콤보, `Reload` 버튼, 오류 적색 표시, `#pragma parameter` 슬라이더, `Reset defaults` 버튼, 셰이더 폴더 경로 안내) 추가. GL 문맥이 없으면 비활성화 및 안내 문구를 표시합니다. 메뉴에서 고른 경우에만 `redmp1.ini`에 저장하며 파라미터 값은 저장하지 않습니다.
* `src/app/main.cpp`: `--post-shader <id>` / `--post-shader=<id>`(빈 값/인자 누락 시 종료 코드 1), `REDMP1_POST_SHADER` 환경 변수, `redmp1.ini` `PostShader` 우선순위를 `ChoosePostShader`로 결정하고 `Initialize()` 전에 `SetPostShaderDirectory`·`SetInitialPostShader`를 호출하며 시작 시 `[redmp1] Post shader: <id>` 로그를 출력합니다.
* `src/app/settings_probe.cpp`: `PostShader=` 왕복, 빈 `PostShader=` 기본값 유지, `ChoosePostShader` 우선순위 검증 추가.

### 단위 4 (문서)
* `third_party/repsp/docs/post-process-shaders.md`: GL 3.3 Core 변환 규칙, `LogicalSize` vs `TextureSize`, `.glslp`, `#pragma parameter` 안내 추가.
* `third_party/repsp/ARCHITECTURE.md`, `third_party/repsp/THIRD_PARTY_NOTICES.md`: `post_shader*.h` 계약 및 re2DJ(BSD 3-Clause), FSR 1.0 / ScaleFX / Anime4K(MIT) 라이선스 고지 추가.
* `README.md`: 화면 셰이더 메뉴, `--post-shader`, `REDMP1_POST_SHADER`, `redmp1.ini` 안내 반영.

---

## 2. 검증 및 측정 결과

### 단위 테스트 및 CTest
* **Win64 Ninja Release** (`%TEMP%\redmp1-build\redmp1-win64-i039-rel`): `37/37` 통과 (`6.81 sec`), `redmp1_settings_probe`, `post_shader_probe`, `gl_probe` 포함.
* **Win64 Ninja Debug** (`%TEMP%\redmp1-build\redmp1-win64-i039-dbg`): `37/37` 통과 (`7.98 sec`).
* **Win64 Ninja nogamedata** (`%TEMP%\redmp1-build\redmp1-win64-i039-nogamedata`): `34/34` 통과 (`5.53 sec`).
* **Win64 VS 18 2026 Release** (`%TEMP%\redmp1-build\redmp1-win64-i043-vs`): `37/37` 통과 (`7.32 sec`).
* **WSL / Linux**: 이 환경에서 `Wsl/E_ACCESSDENIED`로 WSL 접근이 차단되어 미실행(규칙에 따라 우회하지 않음).

### `none` 기준 3회 반복 측정 (`--headless --renderer gl --max-vblanks 2400 --input-script i043-two-songs.txt --post-shader none`)
| 실행 | 출력 프레임 | 평균 간격 (ms) | 최대 간격 (ms) | 디스패치 | 총 실행 시간 (ms) | 종료 코드 |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 1955 | 20.47 | 2201.79 | 0 | 40590 | 0 |
| 2 | 1956 | 20.46 | 2168.66 | 0 | 40522 | 0 |
| 3 | 1972 | 20.30 | 2035.32 | 0 | 40525 | 0 |

### 창 모드 GL 3배율 내장 셰이더 스모크 및 비용 (`--renderer gl --scale 3 --max-vblanks 300 --post-shader <id>`)
| 셰이더 | 출력 프레임 | 평균 간격 (ms) | 최대 간격 (ms) | 디스패치 | 총 실행 시간 (ms) | 종료 코드 |
|---|---:|---:|---:|---:|---:|---:|
| `none` | 145 | 34.40 | 99.13 | 0 | 5645 | 0 |
| `crt` | 145 | 34.52 | 116.97 | 0 | 5628 | 0 |
| `scanline` | 145 | 34.41 | 101.16 | 0 | 5587 | 0 |
| `fsr` | 145 | 34.52 | 116.77 | 0 | 5672 | 0 |
| `scalefx` | 145 | 34.40 | 99.05 | 0 | 5774 | 0 |
| `anime4k` | 145 | 34.41 | 100.25 | 0 | 5715 | 0 |

### 폴백 및 CLI 오류 확인
* `--renderer sw --max-vblanks 100 --post-shader crt`: `[display] post shader 'crt' not applied: software renderer has no OpenGL context` 출력, 정상 종료(종료 코드 0).
* `--post-shader=`: `Error: --post-shader requires a non-empty shader id.` 출력, 종료 코드 1.

### 커밋 상태 (샌드박스 제약)
* `third_party/repsp` (`i50-screen-shaders`)에는 단위 1(`9c593ed`), 단위 2(`e01d630`)가 커밋되어 있으며, 단위 4 문서 변경은 작업 트리에 남아 있습니다.
* `reDMP1` (`i049-screen-shaders`)은 샌드박스에서 `.git/index.lock` 생성 권한이 차단되어(`Permission denied`), 단위 3·4 변경을 작업 트리에 둔 상태로 보고합니다.

---

# Work Log: Screen Shaders (`#49` / `repsp #50`) (English)

* Date: 2026-11-18
* Branches: `reDMP1` (`i049-screen-shaders`), `third_party/repsp` (`i50-screen-shaders`)
* Design: [20261118-i049-screen-shaders.md](../design/20261118-i049-screen-shaders.md)
* Work Order: [20261118-i049-screen-shaders.md](../work-orders/20261118-i049-screen-shaders.md)

## 1. Implementation Summary
* **repsp Units 1 & 2 (`9c593ed`, `e01d630`)**: Implemented the GL-free shader catalog, `#pragma parameter` and `.glslp` preset parsers, GL 3.3 Core / GLES 3.0 source translator, embedded built-ins (`crt`, `scanline`, `fsr`, `scalefx`, `anime4k`) with `LogicalSize` for scanline/CRT consistency across render scales, `OpenGlPostProcess` multi-pass FBO pipeline, `PostShaderControl` runtime API, and `post_shader_probe` / `gl_probe` unit tests.
* **reDMP1 Unit 3**: Added `Settings::post_shader` (`PostShader=` in `redmp1.ini`), the `Screen shader` section in `SettingsUi::DrawGraphicsTab()`, `--post-shader <id>` / `--post-shader=<id>` CLI parsing, `REDMP1_POST_SHADER` env var precedence via `ChoosePostShader`, startup logging, and `redmp1_settings_probe` coverage.
* **Unit 4 Docs**: Added `third_party/repsp/docs/post-process-shaders.md`, updated `third_party/repsp/ARCHITECTURE.md`, `third_party/repsp/THIRD_PARTY_NOTICES.md`, and `README.md`.

## 2. Verification & Measurements
* **CTest**: Win64 Ninja Release (`37/37`, `6.81 s`), Win64 Ninja Debug (`37/37`, `7.98 s`), Win64 Ninja nogamedata (`34/34`, `5.53 s`), and Win64 VS 18 2026 Release (`37/37`, `7.32 s`) all passed. WSL/Linux could not run due to `Wsl/E_ACCESSDENIED`.
* **Headless GL `none` baseline (`--max-vblanks 2400`, 3 runs)**: 1955/1956/1972 frames, mean present interval 20.47/20.46/20.30 ms, 0 dispatches, wall time ~40.5 s, exit 0.
* **Windowed GL 3x built-ins (`--scale 3 --max-vblanks 300`)**: `none`, `crt`, `scanline`, `fsr`, `scalefx`, and `anime4k` all completed 145 frames with 0 dispatches, mean interval 34.40–34.52 ms, wall time 5.59–5.77 s, exit 0.
* **SW fallback & CLI validation**: `--renderer sw --post-shader crt` logged `[display] post shader 'crt' not applied: software renderer has no OpenGL context` and exited 0; `--post-shader=` failed with exit code 1.
* **Git state**: `third_party/repsp` has Units 1–2 committed (`9c593ed`, `e01d630`) with Unit 4 docs in working tree; `reDMP1` changes remain in working tree because `.git/index.lock` creation is blocked in the sandbox (`Permission denied`).
