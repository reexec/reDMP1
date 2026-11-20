# 작업 로그: 텍스처 필터링 (issue #56, repsp #59)

* 날짜: 2026-11-21
* 브랜치: `reDMP1` `i056-texture-filter`, `repsp` `i59-texture-filter`
* 설계: [20261121-i056-texture-filter.md](../design/20261121-i056-texture-filter.md)
* 작업 지시: [20261121-i056-texture-filter.md](../work-orders/20261121-i056-texture-filter.md)

## 1. 단위별 변경 요약

### 단위 1: `repsp` GE 상태 및 `ge_probe`
* `third_party/repsp/include/repsp/gpu/render_backend.h`: `DrawState`에 `kMaxTexLevels = 8`, `tex_level_addr[8]`, `tex_level_bw[8]`, `tex_level_w[8]`, `tex_level_h[8]`, `tex_max_level`, `tex_min_filter`, `tex_mag_filter`, `tex_level_mode`, `tex_lod_bias`를 추가했다.
* `third_party/repsp/include/repsp/gpu/ge_interpreter.h`, `third_party/repsp/src/gpu/ge_interpreter.cpp`:
  * `TBP0~7`(`0xA0~0xA7`), `TBW0~7`(`0xA8~0xAF`), `TSIZE0~7`(`0xB8~0xBF`)를 배열로 디코드하고 레벨 0은 기존 `tex_addr`/`tex_bw`/`tex_w`/`tex_h`와 동기화했다.
  * `TMODE`(`0xC2`)에서 최대 밉맵 레벨(bits 16-18)을, `TFLT`(`0xC6`)에서 축소(bits 0-2)·확대(bits 8-10) 필터를, `TEXLEVEL`(`0xC8`)에서 모드(bits 0-1)와 부호 있는 4.4 bias(bits 16-23, `/16.0f`)를 디코드했다.
  * `GeInterpreter::GetDrawState()` const 조회 함수를 추가해 `ge_probe`가 `DrawState`를 직접 검사할 수 있게 했다.

### 단위 2: `repsp` GL 하드웨어 샘플링
* `third_party/repsp/src/gpu/gl_functions.h`: GL 3.3 샘플러 함수(`glGenSamplers`, `glDeleteSamplers`, `glBindSampler`, `glSamplerParameteri`, `glSamplerParameterf`, `glGetSamplerParameterfv`), `glGenerateMipmap`, `glUniform1f`, 밉맵/LOD 상수(`GL_NEAREST_MIPMAP_NEAREST` 등, `GL_TEXTURE_BASE_LEVEL`, `GL_TEXTURE_MAX_LEVEL`, `GL_TEXTURE_LOD_BIAS`)를 추가했다.
* `third_party/repsp/src/gpu/gl_backend.cpp`:
  * 일반 텍스처 읽기를 `texelFetch`에서 정규화 좌표 `vUv / vec2(uTexSize)` 기반 `texture()`/`textureLod()`로 전환했다(업로드 크기는 이미 GE 레벨 0의 2의 거듭제곱 크기 `tex_w × tex_h`).
  * `SamplerKey` 기반 샘플러 객체 캐시(`g_gl.ui_samplers`)를 도입해 `TWRAP`(`GL_REPEAT`/`GL_CLAMP_TO_EDGE`), `TextureFilterMode`(`kOriginal`/`kLinear`/`kAniso2x..16x`), `TFLT`, 이방성(`GL_TEXTURE_MAX_ANISOTROPY`)을 바인딩하도록 했다. flush 종료 시 `glBindSampler(0, 0)`으로 해제하고 종료 시 정리한다.
  * 렌더 타깃 텍스처(FBO, `uTexIsFbo != 0`)는 밉맵·이방성 없이 내부 배율에 맞춰 좌표를 계산하되, 선형 요청 시 셰이더에서 2×2 텍셀 중심 bilinear로 보간하도록 했다.
  * `PipelineState::operator==`의 `std::memcmp`를 명시적 멤버 비교로 바꿔 패딩·부동소수점 비교 안전성을 확보했다.

### 단위 3: `repsp` 밉맵·`TEXLEVEL`·텍스처 캐시
* `third_party/repsp/src/gpu/texture_decode.h`, `third_party/repsp/src/gpu/texture_decode.cpp`: `DecodeTextureLevelRgba8`, `ComputeTextureLevelDataSizeBytes`를 추가했다(레벨 0은 기존 `tex_addr/bw/w/h` 경로를 유지해 직접 `DrawState`를 채우는 호출자와 호환).
* `third_party/repsp/src/gpu/gl_backend.cpp`:
  * `ResolveTextureLocked`에서 유효한 게임 제공 밉맵 레벨(`1..tex_max_level`, 각 레벨이 `max(1, tex_w >> lvl) × max(1, tex_h >> lvl)`와 일치)을 계산해 `TextureKey`, 콘텐츠 해시, `texture_bytes_hashed`, `TextureUpload::levels`에 포함했다.
  * 게임이 밉맵을 주면 각 레벨을 업로드하고 `GL_TEXTURE_MAX_LEVEL`을 최대 레벨로 설정했다. 게임이 밉맵을 주지 않은 텍스처는 `kAniso2x..16x` 사용 시 `glGenerateMipmap`으로 1회 생성하고, `kOriginal`/`kLinear`에서는 밉맵 없이 그린다. `GetGpuMemoryInfo().hle_textures_bytes`에도 밉맵 체인 바이트를 반영했다.
  * 셰이더에서 `TEXLEVEL` 모드 0(자동)은 bias가 0이 아니면 `texture(uTex, uv, uTexLodBias)`, 모드 1(고정)은 `textureLod(uTex, uv, max(uTexLodBias, 0.0))`를 적용했다.

### 단위 4: `repsp` 소프트웨어 렌더러 bilinear
* `third_party/repsp/src/gpu/render_backend.cpp`: `SwSprite`와 `SwTri`에서 화면 대비 텍셀 변화율로 확대/축소를 판별해 `TFLT`의 선형 여부를 선택하고, 선형이면 `TexelSampleBilinear`(텍셀 중심 `u - 0.5, v - 0.5` 기준 2×2 보간, `TexelFetch`의 감싸기·클램프 규칙 유지)로 샘플링하도록 했다.

### 단위 5: probe
* `third_party/repsp/tools/ge_probe/main.cpp`: `TFLT`, `TMODE` 최대 레벨·swizzle, `TBP/TBW/TSIZE 0~7`, `TEXLEVEL`(음수 `-1.5`·양수 `+2.25` bias) 디코드 검증과 소프트웨어 렌더러 2×2 체커 4배 bilinear 검증을 추가했다.
* `third_party/repsp/tools/gl_probe/main.cpp`: 2×2 체커 4배 최근접/선형, `Original`의 `TFLT` 추종 및 `Linear`의 강제 선형, 이방성 옵션 시 GL 샘플러 `GL_TEXTURE_MAX_ANISOTROPY` readback(`GetLastAppliedAnisotropyForTest`), 2레벨 밉맵 축소 시 레벨 1 색상 선택, render-to-texture 선형 보간, `repeat`/`clamp` 경계 픽셀 검증을 추가했다.

### 단위 6: `reDMP1` 및 문서
* `src/app/settings_ui.cpp`: `Texture Filtering` 콤보 첫 항목 문구를 `"Original"`에서 `"Original (as the game sets it)"`로 변경했다.
* `README.md`: 한국어·영어 `Graphics` 메뉴 설명에 텍스처 필터링 각 항목의 뜻을 추가했다.
* `third_party/repsp/ARCHITECTURE.md`: §2.9에 `TFLT`, `TMODE`, 밉맵, `TEXLEVEL`, 샘플러 객체, `TextureFilterMode` 및 소프트웨어 bilinear 동작을 기록했다.

## 2. 근사 및 미확인 비트

* **`TEXLEVEL` 기울기 모드(mode 2, `GU_TEXTURE_SLOPE`)**: 별도 `TSLOPE`(`0xD0`) 레지스터를 모델링하지 않고 자동 LOD + bias(mode 0과 동일)로 근사했다.
* **`TFLT` 미사용 값(2, 3)**: 공개 GU 문서에 정의되지 않은 축소 필터 값 2, 3은 bit 0(0=최근접, 1=선형)으로 폴백한다.
* **소프트웨어 렌더러 밉맵**: 레벨 0만 샘플링하는 근사를 사용한다(설계 §2.3).
* **Render-to-texture(FBO)**: 밉맵과 이방성 필터링을 적용하지 않으며, 셰이더에서 2×2 bilinear로 읽는다(설계 §2.2).

## 3. 검증 결과

### 3.1 빌드 및 CTest
* Win64 Ninja Release: 빌드 성공, CTest **37/37 통과** (7.39 s)
* Win64 Ninja Debug: 빌드 성공, CTest **37/37 통과** (10.09 s)
* Win64 Visual Studio 18 2026 Release: 빌드 성공, CTest **37/37 통과** (6.64 s)
* 소프트웨어 렌더러·헤드리스(`redmp1.exe roms --renderer software --headless --max-vblanks 120`): `Dispatches: 0`, `Present: 57 frames`, `Exit=0`.
* 게임 데이터 없는 구성: CTest `app_smoke_*` 전부 통과.
* Linux x64 (WSL): 이 실행 환경에서 WSL 호출이 `Wsl/E_ACCESSDENIED`로 차단되어 실행하지 못했다.

### 3.2 게임 창 모드 측정 (3회) 및 프레임 덤프 확인
* 실행 조건: `redmp1.exe roms --renderer gl --max-vblanks 3600 --input-script <keysound-script>` (Release)

| 회차 | Dispatches | Present 프레임 / 간격 평균 | OpenGL draw / tex_uploads / tex_hits | `ch1` underrun | 종료 코드 |
|---|---:|---:|---|---:|---:|
| 1 | 0 | 3177 / 18.90 ms | 41257 / 80 / 95372 | 136 (꼬리만) | 0 |
| 2 | 0 | 3178 / 18.90 ms | 41265 / 80 / 95402 | 136 (꼬리만) | 0 |
| 3 | 0 | 3179 / 18.89 ms | 41249 / 80 / 95364 | 136 (꼬리만) | 0 |

* `REPSP_FRAME_DUMP`로 캡처한 모드 선택·곡 선택·플레이 화면(확인 후 임시 덤프 삭제)에서 곡 재킷, 글자, BGA가 블록 없이 부드럽게 출력됨을 확인했다.
* **관찰 메모**: 플레이 화면에서 마젠타(`#FF00FF`) 컬러키(`CTEST`)를 쓰는 기어 테두리에 `TFLT` 선형 필터가 함께 켜진 구간에서 얇은 마젠타 경계선이 관찰됐다(선형 보간이 컬러키와 인접 텍셀을 섞어 `CTEST` 비교값을 벗어나는 현상). 사용자 화면 확인(설계 §3.5) 시 함께 검토가 필요하다.

---

# Work Log: Texture Filtering (issue #56, repsp #59)

* Date: 2026-11-21
* Branches: `reDMP1` `i056-texture-filter`, `repsp` `i59-texture-filter`
* Design: [20261121-i056-texture-filter.md](../design/20261121-i056-texture-filter.md)
* Work Order: [20261121-i056-texture-filter.md](../work-orders/20261121-i056-texture-filter.md)

## 1. Summary of Changes by Unit

### Unit 1: `repsp` GE State and `ge_probe`
* `third_party/repsp/include/repsp/gpu/render_backend.h`: Added `kMaxTexLevels = 8`, `tex_level_addr[8]`, `tex_level_bw[8]`, `tex_level_w[8]`, `tex_level_h[8]`, `tex_max_level`, `tex_min_filter`, `tex_mag_filter`, `tex_level_mode`, and `tex_lod_bias` to `DrawState`.
* `third_party/repsp/include/repsp/gpu/ge_interpreter.h`, `third_party/repsp/src/gpu/ge_interpreter.cpp`:
  * Decoded `TBP0..7` (`0xA0..0xA7`), `TBW0..7` (`0xA8..0xAF`), and `TSIZE0..7` (`0xB8..0xBF`) into per-level arrays while keeping level 0 synchronized with `tex_addr`/`tex_bw`/`tex_w`/`tex_h`.
  * Decoded `TMODE` (`0xC2`) max mip level (bits 16-18), `TFLT` (`0xC6`) minify (bits 0-2) and magnify (bits 8-10), and `TEXLEVEL` (`0xC8`) mode (bits 0-1) and signed 4.4 bias (bits 16-23, `/16.0f`).
  * Exposed `GeInterpreter::GetDrawState()` so `ge_probe` can inspect `DrawState` directly.

### Unit 2: `repsp` GL Hardware Sampling
* `third_party/repsp/src/gpu/gl_functions.h`: Loaded GL 3.3 sampler functions (`glGenSamplers`, `glDeleteSamplers`, `glBindSampler`, `glSamplerParameteri`, `glSamplerParameterf`, `glGetSamplerParameterfv`), `glGenerateMipmap`, `glUniform1f`, and mipmap/LOD constants.
* `third_party/repsp/src/gpu/gl_backend.cpp`:
  * Switched regular texture sampling from `texelFetch` to normalized `texture()`/`textureLod()` (`vUv / vec2(uTexSize)`), matching power-of-two level-0 uploads (`tex_w × tex_h`).
  * Added a `SamplerKey`-indexed sampler object cache (`g_gl.ui_samplers`) for `TWRAP`, `TextureFilterMode`, `TFLT`, and `GL_TEXTURE_MAX_ANISOTROPY`, unbinding unit 0 after each flush and deleting samplers on shutdown.
  * Updated render-to-texture FBO sampling (`uTexIsFbo != 0`) to perform 2×2 texel-centered shader bilinear filtering when linear is requested, without mipmaps or anisotropy.
  * Replaced `std::memcmp` in `PipelineState::operator==` with explicit field comparisons.

### Unit 3: `repsp` Mipmaps, `TEXLEVEL`, and Texture Cache
* `third_party/repsp/src/gpu/texture_decode.h`, `third_party/repsp/src/gpu/texture_decode.cpp`: Added `DecodeTextureLevelRgba8` and `ComputeTextureLevelDataSizeBytes` (preserving level-0 compatibility with direct `DrawState` callers).
* `third_party/repsp/src/gpu/gl_backend.cpp`:
  * Included valid game-supplied mip levels (`1..tex_max_level`, matching `max(1, tex_w >> lvl) × max(1, tex_h >> lvl)`) in `TextureKey`, content hashing, `texture_bytes_hashed`, `TextureUpload::levels`, and `GetGpuMemoryInfo().hle_textures_bytes`.
  * Uploaded game-supplied levels with `GL_TEXTURE_MAX_LEVEL = supplied_max`; generated mipmaps once via `glGenerateMipmap` for single-level textures when an anisotropic mode is active, while keeping `Original` and `Linear` mipmap-free when no levels are supplied.
  * Implemented `TEXLEVEL` auto bias (`texture(..., bias)`) and constant level (`textureLod`) in the fragment shader.

### Unit 4: `repsp` Software Renderer Bilinear
* `third_party/repsp/src/gpu/render_backend.cpp`: Added `TexelSampleBilinear` (texel-centered `u - 0.5, v - 0.5` 2×2 blend using `TexelFetch` wrap/clamp rules) selected from `TFLT` in `SwSprite` and `SwTri`.

### Unit 5: Probes
* `third_party/repsp/tools/ge_probe/main.cpp`: Added deterministic verification of `TFLT`, `TMODE` max level/swizzle, `TBP/TBW/TSIZE 0..7`, `TEXLEVEL` signed bias (`-1.5`, `+2.25`), and software renderer 2×2 checker 4x bilinear filtering.
* `third_party/repsp/tools/gl_probe/main.cpp`: Added hidden-GL-context checks for 2×2 checker 4x nearest vs linear, `Original` following `TFLT`, `Linear` overriding nearest, `GL_TEXTURE_MAX_ANISOTROPY` readback via `GetLastAppliedAnisotropyForTest()`, two-level mipmap minification selecting level 1, bilinear render-to-texture, and `repeat`/`clamp` edge pixels.

### Unit 6: `reDMP1` and Documentation
* `src/app/settings_ui.cpp`: Updated the `"Original"` combo label to `"Original (as the game sets it)"`.
* `README.md`: Updated Korean and English `Graphics` menu descriptions for Texture Filtering.
* `third_party/repsp/ARCHITECTURE.md`: Documented `TFLT`, `TMODE`, mipmaps, `TEXLEVEL`, sampler objects, `TextureFilterMode`, and software bilinear sampling in §2.9.

## 2. Approximations and Unconfirmed Bits

* **`TEXLEVEL` slope mode (mode 2, `GU_TEXTURE_SLOPE`)**: Approximated as automatic LOD + bias (same as mode 0); `TSLOPE` (`0xD0`) is not modeled separately.
* **Unused `TFLT` values (2, 3)**: Undocumented minify values fall back to bit 0 (`0` = nearest, `1` = linear).
* **Software renderer mipmaps**: Uses level 0 only (per design §2.3).
* **Render-to-texture (FBO)**: Uses 2×2 shader bilinear when linear is requested, with no mipmaps or anisotropy (per design §2.2).

## 3. Verification Results

### 3.1 Builds and CTest
* Win64 Ninja Release: build succeeded, CTest **37/37 passed** (7.39 s)
* Win64 Ninja Debug: build succeeded, CTest **37/37 passed** (10.09 s)
* Win64 Visual Studio 18 2026 Release: build succeeded, CTest **37/37 passed** (6.64 s)
* Software renderer + `--headless` (`redmp1.exe roms --renderer software --headless --max-vblanks 120`): `Dispatches: 0`, `Present: 57 frames`, `Exit=0`.
* No-game-data configuration: all `app_smoke_*` CTest cases passed.
* Linux x64 (WSL): could not run in this environment because WSL returns `Wsl/E_ACCESSDENIED`.

### 3.2 Windowed Gameplay Measurements (3 Runs) and Frame Dumps
* Command: `redmp1.exe roms --renderer gl --max-vblanks 3600 --input-script <keysound-script>` (Release)

| Run | Dispatches | Present frames / mean interval | OpenGL draws / tex_uploads / tex_hits | `ch1` underruns | Exit code |
|---|---:|---:|---|---:|---:|
| 1 | 0 | 3177 / 18.90 ms | 41257 / 80 / 95372 | 136 (tails only) | 0 |
| 2 | 0 | 3178 / 18.90 ms | 41265 / 80 / 95402 | 136 (tails only) | 0 |
| 3 | 0 | 3179 / 18.89 ms | 41249 / 80 / 95364 | 136 (tails only) | 0 |

* Frame dumps captured via `REPSP_FRAME_DUMP` (deleted after inspection) confirmed that mode select, song select jackets/lettering, and gameplay BGAs render smoothly without blocky pixelation.
* **Observation**: During gameplay, a thin magenta (`#FF00FF`) fringe was observed on edges of the color-keyed (`CTEST`) gear sprite when `TFLT` linear filtering is active (linear interpolation blends the color-key border with adjacent opaque texels so the result no longer matches `#FF00FF` in `CTEST`). Flagged for user review during §3.5 visual verification.
