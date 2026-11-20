# 설계: 텍스처 필터링 — 게임 설정(TFLT)을 따르고, 하드웨어 샘플링과 밉맵을 쓴다 (issue #56, repsp #59)

## 1. 보고와 진단

* 사용자 보고(2026-11-21): 렌더링 배율 1x, 출력 필터 Linear에서 곡 재킷 같은 그림이 자글자글하고 네모 블록으로 보인다. 출력 필터가 적용된 큰 글자 윤곽은 부드럽지만, 작은 텍스처를 키워 그린 그림은 픽셀 블록이 그대로 보인다.
* 원인 세 가지(임시 계측, 커밋하지 않음):
  1. **GE 해석기가 TFLT(`0xC6`)를 읽지 않는다.** 이 게임이 쓰는 값은 다음과 같다.

     | TFLT | 축소 | 확대 |
     |---|---|---|
     | `0x000000` | 최근접 | 최근접 |
     | `0x000105` | 밉맵+선형(5) | **선형** |
     | `0x000604` | 밉맵+최근접(4) | 최근접 |

  2. **"Original" 옵션이 항상 최근접이다.** `ApplyTextureFiltering`이 게임 값과 상관없이 `GL_NEAREST`를 쓴다.
  3. **GL 셰이더가 `texelFetch`로 텍셀 하나만 읽는다.** 좌표를 직접 감싸서 읽기 때문에 텍스처의 필터 설정이 아무 효과가 없다. "Linear"와 이방성 2x~16x 옵션도 실제로는 모두 최근접이었다.
* **게임이 밉맵 레벨을 직접 준다.** TMODE `0x010000`(최대 레벨 1)이 나온다. 레벨 1 이상의 주소·폭·크기(TBP1~7, TBW1~7, TSIZE1~7)와 TMODE의 최대 레벨, 레벨 선택(TEXLEVEL `0xC8`)을 지금은 읽지 않는다.
* **사용자 결정(2026-11-21)**: 이방성 필터링도 실제로 동작하게 만든다.

## 2. 변경 (repsp)

### 2.1 GE 상태
* `DrawState`에 다음을 더한다.
  * **TFLT**: 축소 필터(bits 0-2), 확대 필터(bits 8-10). 선형은 bit 0, 밉맵 사용은 축소 값 4~7이다.
  * **TMODE의 최대 레벨**(bits 16-18). swizzle은 지금처럼 읽는다.
  * **레벨 1~7**: 주소(TBP/TBW `0xA1`~`0xA7`, `0xA9`~`0xAF`), 크기(TSIZE `0xB9`~`0xBF`)
  * **TEXLEVEL**(`0xC8`): 모드(bits 0-1: 자동/고정/기울기)와 bias(bits 16-23, 부호 있음, /16)
* GE 명령의 비트 배치는 공개된 PSP GE 레지스터 문서를 따른다. 확실하지 않은 비트는 작업 로그에 적는다.

### 2.2 GL 백엔드: 하드웨어 샘플링
* 셰이더의 텍스처 읽기를 `texelFetch`에서 `texture()`(정규화 좌표 `uv / 텍스처 크기`)로 바꾼다. 감싸기와 필터는 샘플러 상태로 정한다.
  * 감싸기: TWRAP에 따라 `GL_REPEAT` 또는 `GL_CLAMP_TO_EDGE`
  * 필터:
    * **Original**: TFLT를 따른다. 확대는 bit 0으로 선형/최근접을 고른다. 축소는 값에 맞는 GL 상수다(밉맵이 없으면 밉맵 없는 형태).
    * **Linear**: 항상 선형이다. 축소는 밉맵이 있으면 `GL_LINEAR_MIPMAP_LINEAR`를 쓴다.
    * **이방성 2x~16x**: Linear에 `GL_TEXTURE_MAX_ANISOTROPY`(지원 범위로 제한)를 더한다.
  * 샘플러 객체(`glGenSamplers`/`glBindSampler`, GL 3.3)를 쓰면 텍스처마다 상태를 다시 쓰지 않아도 된다.
* 텍스처 크기는 GE의 레벨 0 크기(`tex_w × tex_h`, 2의 거듭제곱)로 올린다. 그래야 정규화 좌표와 `GL_REPEAT`이 PSP의 감싸기와 같다. 지금 업로드가 이미 그 크기인지 확인하고, 아니면 맞춘다.
* **밉맵**:
  * 게임이 레벨을 주면(최대 레벨 ≥ 1) 각 레벨을 디코드해 그 레벨로 올린다. `GL_TEXTURE_MAX_LEVEL`은 최대 레벨로 둔다. 텍스처 캐시의 해시와 크기 계산에 레벨을 포함한다.
  * 게임이 레벨을 주지 않고 이방성 옵션이면, 레벨 0에서 `glGenerateMipmap`으로 만든다. Original과 Linear에서 레벨이 없으면 밉맵 없이 그린다.
  * TEXLEVEL:
    * 자동 모드는 GL의 LOD 계산에 bias(`GL_TEXTURE_LOD_BIAS` 또는 셰이더 `texture(..., bias)`)를 더한다.
    * 고정 모드는 `textureLod`로 그 레벨을 쓴다.
    * 기울기 모드는 근사한다(작업 로그에 적음).
* **렌더 타깃을 텍스처로 쓸 때(render-to-texture)**:
  * FBO 텍스처는 내부 배율 크기이고 밉맵이 없다.
  * 지금처럼 좌표를 맞춰 읽되, 선형이 요청되면 셰이더에서 2×2 텍셀을 섞는 bilinear로 읽는다. 감싸기도 지금 규칙을 따른다.
  * 이방성과 밉맵은 적용하지 않는다.
* **비용**: 밉맵 생성과 레벨 업로드는 텍스처를 올릴 때 한 번만 한다. 표시 간격으로 확인한다.

### 2.3 소프트웨어 렌더러
* TFLT를 따른다. 선형이면 2×2 텍셀 bilinear(텍셀 중심 기준, 감싸기 규칙 같음)로 읽는다.
* 밉맵은 레벨 0만 쓴다(근사, 작업 로그에 적음).

### 2.4 앱(reDMP1)
* 메뉴 문구를 고친다.
  * "Original" → "Original (as the game sets it)"
  * 이방성 항목은 그대로 둔다.
* README의 Texture Filtering 설명도 같이 고친다.

## 3. 검증

1. **probe**(창 없이, 결정적):
   * `ge_probe`: TFLT, TMODE 최대 레벨, TBP/TBW/TSIZE 1~7, TEXLEVEL을 `DrawState`로 옮긴다.
   * `gl_probe`(숨은 창의 GL 문맥):
     * 2×2 체커 텍스처를 4배로 그려, 최근접이면 경계가 날카롭고 선형이면 중간값이 나온다.
     * Original에서 TFLT를 따르고, Linear는 항상 선형이다.
     * 이방성 옵션에서 `GL_TEXTURE_MAX_ANISOTROPY`가 설정된다. 지원하지 않으면 건너뛴다.
     * 레벨 2개 텍스처를 축소해 그리면 레벨 1의 색이 나온다.
     * render-to-texture를 선형으로 읽는다.
     * 감싸기(repeat/clamp)는 경계 픽셀로 검사한다.
   * 소프트웨어 렌더러: 같은 체커를 bilinear로 그린다.
2. **게임**(창 모드, 같은 입력 스크립트 3회):
   * 미발견 주소 dispatch 0, 크래시 0
   * 표시 간격이 이전 범위(약 19 ms)
   * 곡 선택과 플레이 화면의 프레임 덤프에서 재킷과 글자가 이전보다 부드럽다(덤프는 커밋하지 않음).
3. 소프트웨어 렌더러, `--headless`, 게임 데이터 없는 구성이 빌드되고 동작한다.
4. Win64 Debug/Release(Visual Studio 생성기 포함)와 Linux x64 빌드·CTest(repsp, reDMP1). GCC 경고 없음.
5. 사용자가 1x·Linear 화면과 각 텍스처 필터 옵션을 확인한다(사용자 확인).

## 4. 기록
* repsp `ARCHITECTURE.md`: TFLT/TMODE/밉맵/TEXLEVEL과 텍스처 필터 옵션의 뜻.
* reDMP1 작업 로그 `docs/work-logs/20261121-i056-texture-filter.md`. 근사한 부분과 확인하지 못한 비트를 적는다.

---

# Design: texture filtering — follow the game (TFLT), with hardware sampling and mipmaps (issue #56, repsp #59)

## 1. Report and diagnosis

* **User report (2026-11-21)**: at render scale 1x with the Linear output filter, art such as the song jacket looks jagged and blocky. Large lettering, which the output filter smooths, is fine; small textures drawn larger show their pixel blocks.
* **Three causes** (temporary instrumentation, not committed):
  1. **The GE interpreter does not decode TFLT (`0xC6`).** This title uses:

     | TFLT | minify | magnify |
     |---|---|---|
     | `0x000000` | nearest | nearest |
     | `0x000105` | mipmap + linear (5) | **linear** |
     | `0x000604` | mipmap + nearest (4) | nearest |

  2. **The "Original" option is always nearest.** `ApplyTextureFiltering` uses `GL_NEAREST` whatever the game sets.
  3. **The GL shader reads one texel with `texelFetch`**, wrapping coordinates by hand, so the texture's filter settings have no effect. "Linear" and the anisotropic 2x-16x options were all nearest in practice.
* **The game supplies its own mip levels**: TMODE `0x010000` (maximum level 1) occurs. Today nothing reads the level 1+ addresses, widths and sizes (TBP1-7, TBW1-7, TSIZE1-7), TMODE's maximum level, or level selection (TEXLEVEL `0xC8`).
* **User decision (2026-11-21)**: make anisotropic filtering really work too.

## 2. Changes (repsp)

### 2.1 GE state
* `DrawState` gains:
  * **TFLT**: minify filter (bits 0-2) and magnify filter (bits 8-10). Bit 0 selects linear; minify values 4-7 use mipmaps.
  * **TMODE's maximum level** (bits 16-18). Swizzle is read as today.
  * **Levels 1-7**: address (TBP/TBW `0xA1`-`0xA7`, `0xA9`-`0xAF`) and size (TSIZE `0xB9`-`0xBF`).
  * **TEXLEVEL** (`0xC8`): mode (bits 0-1: automatic, constant, slope) and bias (bits 16-23, signed, /16).
* Bit layouts follow public PSP GE register documentation. Any bit not certain is noted in the work log.

### 2.2 GL backend: hardware sampling
* Texture reads move from `texelFetch` to `texture()` with normalized coordinates (`uv / texture size`). Wrapping and filtering come from sampler state:
  * **Wrap**: `GL_REPEAT` or `GL_CLAMP_TO_EDGE`, from TWRAP.
  * **Filter**:
    * **Original** follows TFLT. Magnify takes linear or nearest from bit 0. Minify uses the matching GL constant, without the mipmap form when there are no mipmaps.
    * **Linear** is always linear. Minify uses `GL_LINEAR_MIPMAP_LINEAR` when mipmaps exist.
    * **Anisotropic 2x-16x** is Linear plus `GL_TEXTURE_MAX_ANISOTROPY`, limited to what is supported.
  * Sampler objects (`glGenSamplers`/`glBindSampler`, GL 3.3) avoid rewriting state on every texture.
* The texture is uploaded at the GE's level-0 size (`tex_w × tex_h`, powers of two), so normalized coordinates and `GL_REPEAT` match the PSP's wrapping. Check whether uploads already have that size, and make them so if not.
* **Mipmaps**:
  * When the game supplies levels (maximum level ≥ 1), each level is decoded and uploaded to that level, with `GL_TEXTURE_MAX_LEVEL` at the maximum. The texture cache's hash and size accounting include the levels.
  * When the game supplies none and an anisotropic option is set, they are generated from level 0 with `glGenerateMipmap`. Original and Linear draw without mipmaps when there are no levels.
  * **TEXLEVEL**:
    * Automatic mode adds the bias to GL's LOD (`GL_TEXTURE_LOD_BIAS`, or `texture(..., bias)` in the shader).
    * Constant mode uses that level with `textureLod`.
    * Slope mode is approximated (noted in the work log).
* **Render-to-texture**:
  * An FBO texture is at the internal scale and has no mipmaps.
  * It is read with coordinates mapped as today. When linear is requested, the shader reads it bilinearly, blending 2×2 texels, with today's wrap rules.
  * Anisotropic filtering and mipmaps do not apply.
* **Cost**: mipmap generation and level uploads happen once per texture upload, checked against the present interval.

### 2.3 Software renderer
* Follows TFLT. When linear, it reads 2×2 texels bilinearly (texel-centred, same wrap rules).
* Mipmaps: level 0 only (an approximation, noted in the work log).

### 2.4 App (reDMP1)
* Menu wording:
  * "Original" becomes "Original (as the game sets it)".
  * The anisotropic entries stay.
* The README's Texture Filtering description follows.

## 3. Verification

1. **Probes** (no window, deterministic):
   * `ge_probe`: TFLT, TMODE's maximum level, TBP/TBW/TSIZE 1-7 and TEXLEVEL reach `DrawState`.
   * `gl_probe` (hidden window's GL context):
     * A 2×2 checker texture drawn at 4x has sharp edges when nearest and intermediate values when linear.
     * Original follows TFLT; Linear is always linear.
     * Anisotropic options set `GL_TEXTURE_MAX_ANISOTROPY`; skipped where unsupported.
     * A two-level texture drawn minified shows level 1's colour.
     * Render-to-texture is read linearly.
     * Wrapping (repeat/clamp) is checked with edge pixels.
   * Software renderer: the same checker drawn bilinearly.
2. **Game** (windowed, the same input script, three runs):
   * no unresolved-address dispatches, no crashes;
   * present interval in the earlier range (about 19 ms);
   * frame dumps of song select and gameplay show the jacket and lettering smoother than before (dumps not committed).
3. The software renderer, `--headless`, and a configuration without game data build and run.
4. Win64 Debug/Release (including the Visual Studio generator) and Linux x64, build and CTest, for repsp and reDMP1. No GCC warnings.
5. The user checks the 1x/Linear picture and each texture filtering option (the user's check).

## 4. Records
* repsp `ARCHITECTURE.md`: TFLT, TMODE, mipmaps, TEXLEVEL, and what each texture filtering option means.
* reDMP1 work log: `docs/work-logs/20261121-i056-texture-filter.md`, noting approximations and any unconfirmed bits.
