# 설계: 화면 셰이더(screen shader) (issue #49, repsp #50)

## 1. 요구사항

* 사용자 요청(2026-11-18): re2DJ의 screen shader 기능을 가져온다.
* **참고 구현**: re2DJ `include/re2dj/graphics/post_shader_*.h`, `opengl_post_process.h`, `src/graphics/post_shader_*.cpp`, `opengl_post_process.cpp`, `src/graphics/post_shaders/`, `src/ui/osd.cpp`의 `DrawPostShaderMenu`, `tests/unit/post_shader*_test.cpp`, `tests/graphics/opengl_post_shader_probe.cpp`, 문서 `docs/guides/post-process-shaders.md`, `docs/kb/libretro-glsl-post-shaders.md`, 설계 455·i026·i027.
  * 리뷰어가 `%TEMP%\redmp1-build\borrow\re2DJ\`에 사본을 둔다. 저장소에는 넣지 않는다.
  * re2DJ는 BSD 3-Clause다. 가져온 코드와 셰이더에는 re2DJ의 저작권 고지를 남기고, `THIRD_PARTY_NOTICES.md`에 적는다. FSR 1.0, ScaleFX, Anime4K는 MIT이고 각 파일의 고지를 그대로 둔다.
* 가져오는 범위는 re2DJ v0.0.75 전체다.
  * 단일 pass `.glsl`과 다단계 `.glslp`(libretro 형식의 부분집합)
  * `#pragma parameter`와 슬라이더
  * 내장 `crt`, `scanline`, `fsr`, `scalefx`, `anime4k`
  * `shaders/` 디렉터리의 사용자 파일, Reload
  * 실패하면 `none`으로 그리고 이유를 보인다.

## 2. 나눔

* **repsp(플랫폼, 게임 무관)**:
  * 셰이더 목록·preset 해석·소스 조립(GL 없음, 단위 시험 가능)
  * GL 후처리 pass(표시 단계)
  * 앱이 쓰는 제어 계약
  * 내장 셰이더
* **reDMP1(앱)**:
  * Graphics 탭의 Screen shader 항목
  * `redmp1.ini`의 `post_shader`
  * `--post-shader`와 `REDMP1_POST_SHADER`

## 3. repsp

### 3.1 표시 경로
* 후처리는 **표시 단계에서만** 한다. 게스트가 읽을 수 있는 것(VRAM, 렌더 타깃, 텍스처 캐시)은 바뀌지 않는다.
* 끼워 넣는 곳은 `sdl_platform.cpp`의 GL 헬퍼 `DrawTexture` 하나다. GL 렌더러의 `present_tex`와, GL 문맥에서 CPU 화면을 올린 `cpu_tex`가 모두 여기를 지난다.
* 셰이더가 선택되면 마지막 quad를 셰이더로 그린다. 그 밖의 pass는 각자의 타깃에 그린다.
* `none`이면 지금과 GL 호출이 같다.
* 화면 비율(`ComputeOutputRect`)과 검은 바탕은 그대로다. 셰이더는 그 사각형 viewport 안에 그린다.
* `flip_y`(CPU 화면)는 꼭짓점의 TexCoord로 처리한다. 셰이더는 위아래를 신경 쓰지 않는다.
* SDL 렌더러 경로(GL 문맥이 없는 소프트웨어 렌더러)에는 셰이더가 없다. 이 경우 고른 셰이더는 적용되지 않고, 그 사실을 로그에 남긴다.
* 오버레이(ImGui)는 셰이더 뒤에 그린다. 메뉴에는 셰이더가 걸리지 않는다.

### 3.2 GL 3.3 core에서 libretro GLSL
re2DJ는 OpenGL 2.1 compatibility 문맥이라 GLSL 1.20을 그대로 컴파일한다. repsp는 3.3 core 문맥이다(GLES면 3.0). 그래서 소스를 조립할 때 다음과 같이 바꾼다.

* 첫 줄의 `#version`은 버리고, 문맥에 맞는 `#version 330 core`(GLES는 `#version 300 es`와 `precision` 선언)를 넣는다.
* 그 뒤에 stage 정의(`VERTEX`/`FRAGMENT`), `PARAMETER_UNIFORM`, 호환 매크로를 넣는다.
  * vertex: `attribute`→`in`, `varying`→`out`
  * fragment: `varying`→`in`, `texture2D`→`texture`, 출력 변수 하나를 선언하고 `gl_FragColor`를 그 이름으로 바꾼다.
  * `gl_FragColor`는 `#define`으로 바꾸지 않는다. `gl_`로 시작하는 매크로 이름을 거부하는 드라이버가 있으므로, 토큰 단위로 치환한다.
* libretro 셰이더의 `#if __VERSION__ >= 130` 분기(`COMPAT_VARYING` 등)는 `__VERSION__`이 330이므로 저절로 맞는 쪽을 탄다.
* core 문맥에는 client-side 꼭짓점 배열이 없으므로, 단위 사각형은 VAO와 VBO로 그린다. attribute 위치는 링크 전에 `VertexCoord`=0, `COLOR`=1, `TexCoord`=2로 묶는다.
* 조립은 GL 없는 함수다. 단위 시험에서 결과 문자열을 검사한다.

### 3.3 크기 uniform
* `TextureSize`, `InputSize`: 입력 텍스처의 실제 픽셀 크기다(libretro의 원래 의미). 렌더링 배율이 2면 960x544다. FSR, ScaleFX 같은 업스케일러는 이 격자를 그대로 쓴다.
* `OutputSize`: 화면 비율 사각형의 픽셀 크기다.
* `LogicalSize`: 게임의 논리 해상도(480x272)다. repsp가 더한 이름이다. 렌더링 배율과 상관없이 원래 주사선 하나에 빔 하나를 그리려면 이것을 쓴다.
  * 내장 `crt`와 `scanline`은 re2DJ에서 `TextureSize`를 논리 해상도로 썼다. repsp에서는 이 둘이 `LogicalSize`를 쓰도록 고친다.
* 다단계 pass는 re2DJ와 같다. `Texture`, `OrigTexture`, `PassPrevNTexture`와 각 `...TextureSize`, `...InputSize`(그 텍스처의 픽셀 크기)를 쓴다.
* `FrameCount`, `FrameDirection`(1), `MVPMatrix`도 re2DJ와 같다.

### 3.4 필터
* `.glsl` 한 pass(또는 preset의 첫 pass에 `filter_linear0`이 없을 때)는 입력을 지금의 출력 필터(최근접/선형)로 읽는다.
* preset이 `filter_linearN`을 정하면 그 값을 따른다.
* 마지막 pass에 배율이 있으면 그 결과를 선형으로 창에 그린다(re2DJ와 같음).

### 3.5 목록과 파일
* 셰이더 디렉터리는 앱이 정한다(`SetPostShaderDirectory`). repsp는 경로를 정하지 않는다.
* 목록: 내장(고정 순서) 다음에 디렉터리의 `.glsl`, `.glslp`를 이름순으로. 디렉터리가 없거나 읽을 수 없으면 내장만 보인다.
* id: `none`, 내장 이름(`crt`), 파일 이름(`my_crt.glsl`, `my_chain.glslp`).
* 내장 셰이더는 CMake가 `src/gpu/post_shaders/`(또는 그에 맞는 위치)의 파일을 실행 파일에 넣는다. 생성 파일은 빌드 디렉터리에만 둔다.

### 3.6 제어 계약(`include/repsp/platform/post_shader.h`)
* re2DJ의 `PostShaderControl`과 같은 기능이다. 이름과 네임스페이스는 repsp 것으로 바꾼다.
  * 목록, 셰이더 디렉터리, 지금 셰이더 id, 마지막 오류
  * `Select(id)`: 즉시 컴파일한다. 실패하면 `none`이 되고 오류를 남긴다.
  * `Reload()`: 목록을 다시 읽고 지금 셰이더를 다시 컴파일한다.
  * `parameters()`: `#pragma parameter` 값, 그 자리에서 바꿀 수 있다.
* 이 메서드들은 GL 문맥이 있는 UI 스레드에서만 부른다. 오버레이 빌더가 그 스레드에서 돌므로 메뉴에서 바로 부를 수 있다.
* 창이 열리기 전에 앱이 고르는 시작 셰이더는 `SetInitialPostShader(id)`로 넘긴다. 창이 열릴 때 UI 스레드가 적용한다. 적용하지 못하면 로그에 `post shader '<id>' not applied: <이유>`를 남긴다.
* 지금 제어 객체는 GL 문맥이 없으면 없다(null). 앱은 이때 메뉴 항목을 숨기거나 비활성으로 둔다.

### 3.7 실패
* 컴파일, 링크, 타깃 생성 실패는 화면을 검게 만들지 않는다. `none`으로 그리고 이유를 로그와 `last_error`에 남긴다.
* 실행 중 고른 셰이더가 실패해도 게임은 계속 돈다.

## 4. reDMP1

* **Graphics 탭**에 Screen shader 항목을 넣는다(re2DJ `DrawPostShaderMenu`와 같은 구성).
  * 콤보: `none`과 목록
  * Reload 버튼, 셰이더 디렉터리 경로 표시
  * 오류가 있으면 빨간 글씨
  * 매개변수 슬라이더(값, 최소, 최대, 단계)와 기본값으로 되돌리기
  * GL 문맥이 없으면(소프트웨어 렌더러에서 SDL 렌더러 경로) 항목을 비활성으로 두고 이유를 한 줄 보인다.
* **셰이더 디렉터리**: 작업 디렉터리의 `shaders/`가 있으면 그것, 없으면 실행 파일 옆의 `shaders/`.
* **`redmp1.ini`**: `post_shader = none`. 메뉴에서 고르면 저장한다. 매개변수 값은 저장하지 않는다(re2DJ와 같음, 셰이더를 다시 고르면 기본값).
* **우선순위**: `--post-shader <id>`(또는 `--post-shader=<id>`), 그다음 `REDMP1_POST_SHADER`, 그다음 `redmp1.ini`, 없으면 `none`. 명령줄이나 환경 변수로 정한 값은 파일에 쓰지 않는다. 값이 없거나 비어 있는 `--post-shader`는 사용법 오류다.
* 시작 로그에 고른 셰이더를 적는다.

## 5. 측정과 검증

1. 창 모드에서 확인한다.
   * 각 내장 셰이더가 적용되고, 메뉴에서 바꾸면 바로 바뀐다. `none`은 이전과 같은 화면이다.
   * 매개변수 슬라이더가 바로 반영된다.
   * `shaders/`에 넣은 `.glsl`과 `.glslp`가 Reload 후 목록에 나오고, 일부러 깨뜨리면 오류가 보이고 화면은 `none`이다.
   * 화면 비율 세 가지, 출력 필터 두 가지, 렌더링 배율 1·2·4, MSAA를 켠 상태에서 그림이 맞다(위아래, 위치, 크기).
   * 다시 실행하면 고른 셰이더가 유지되고, `--post-shader`와 환경 변수가 우선한다.
2. 성능: 셰이더 `none`에서 표시 간격이 이전과 같은 범위다(플레이 화면까지 입력 스크립트 3회). 각 내장 셰이더의 비용을 3배 창 이상에서 적는다(vsync 끔이 가능하면 처리량).
3. probe(창 없이, 숨은 창의 GL 문맥은 허용):
   * 내장 셰이더 다섯 개가 3.3 core에서 모두 컴파일·링크된다.
   * `none`은 셰이더 없이 그린 것과 픽셀이 같다.
   * `scanline`이 논리 주사선마다 어두운 줄을 만든다(렌더링 배율 2에서도 272줄 기준).
   * 2 pass preset이 이어진다.
   * 깨진 셰이더는 `none`으로 돌아가고 오류가 남는다.
   * `flip_y` 입력도 위아래가 맞다.
4. 단위 시험(GL 없음): `#pragma parameter` 해석, 소스 조립(호환 매크로, `gl_FragColor` 치환, `#version` 교체), `.glslp` 해석, 목록, 우선순위(명령줄·환경 변수·파일).
5. 소프트웨어 렌더러, `--headless`, 게임 데이터 없는 구성이 이전처럼 동작하고 빌드된다.
6. Win64 Debug/Release(Visual Studio 생성기 포함)와 Linux x64 빌드·CTest(repsp, reDMP1). GCC 경고 없음.
7. 사용자가 창 모드에서 셰이더를 확인한다(사용자 확인).

## 6. 기록

* repsp: `ARCHITECTURE.md`에 표시 단계 후처리와 제어 계약을, `docs/`에 셰이더 작성 안내(re2DJ guide와 kb를 repsp에 맞게: GL 3.3 core 변환, `LogicalSize`)를, `THIRD_PARTY_NOTICES.md`에 re2DJ(BSD 3-Clause), FSR 1.0, ScaleFX, Anime4K를 적는다.
* reDMP1: `README.md`(또는 사용 안내)에 Screen shader 메뉴, `--post-shader`, `REDMP1_POST_SHADER`, `shaders/`, `redmp1.ini`의 `post_shader`를 적는다. 작업 로그 `docs/work-logs/20261118-i049-screen-shaders.md`에 측정과 질문을 적는다.
* 스크린샷은 커밋하지 않는다.

---

# Design: screen shaders (issue #49, repsp #50)

## 1. Requirements

* **User request (2026-11-18)**: bring over re2DJ's screen shader feature.
* **Reference implementation**:
  * re2DJ headers: `include/re2dj/graphics/post_shader_*.h` and `opengl_post_process.h`.
  * re2DJ sources: `src/graphics/post_shader_*.cpp`, `opengl_post_process.cpp` and `src/graphics/post_shaders/`.
  * The OSD menu: `DrawPostShaderMenu` in `src/ui/osd.cpp`.
  * Tests: `tests/unit/post_shader*_test.cpp` and `tests/graphics/opengl_post_shader_probe.cpp`.
  * Documents: `docs/guides/post-process-shaders.md`, `docs/kb/libretro-glsl-post-shaders.md`, and designs 455, i026 and i027.
  * The reviewer puts copies in `%TEMP%\redmp1-build\borrow\re2DJ\`; they are not committed.
  * re2DJ is BSD 3-Clause. Code and shaders taken from it keep re2DJ's copyright notice and are listed in `THIRD_PARTY_NOTICES.md`. FSR 1.0, ScaleFX and Anime4K are MIT; each file's notice stays as it is.
* **Scope**: all of re2DJ v0.0.75's feature:
  * single-pass `.glsl` and multi-pass `.glslp` (a subset of the libretro format);
  * `#pragma parameter` with sliders;
  * the built-ins `crt`, `scanline`, `fsr`, `scalefx` and `anime4k`;
  * user files in a `shaders/` directory, and Reload;
  * on failure, drawing with `none` and showing the reason.

## 2. Split

* **repsp (platform, game-independent)**:
  * shader listing, preset parsing and source assembly (no GL, unit-testable);
  * the GL post-processing pass at presentation;
  * the control contract the app uses;
  * the built-in shaders.
* **reDMP1 (app)**:
  * the Screen shader item in the Graphics tab;
  * `post_shader` in `redmp1.ini`;
  * `--post-shader` and `REDMP1_POST_SHADER`.

## 3. repsp

### 3.1 Presentation path
* Post-processing happens **at presentation only**. Nothing the guest can read changes: VRAM, render targets and the texture cache are untouched.
* It goes into one place, the GL helper's `DrawTexture` in `sdl_platform.cpp`. Both the GL renderer's `present_tex` and the CPU picture uploaded in a GL context (`cpu_tex`) pass through it.
* With a shader selected, the last quad is drawn through it. Any other passes draw into targets of their own.
* With `none`, the GL calls are the same as today.
* The aspect rectangle (`ComputeOutputRect`) and the black background stay. The shader draws inside that rectangle's viewport.
* `flip_y` (the CPU picture) is handled in the vertices' TexCoord, so shaders need not care which way is up.
* The SDL renderer path (the software renderer with no GL context) has no shaders. A chosen shader is not applied there, and the log says so.
* The overlay (ImGui) is drawn after the shader, so the menu is never shaded.

### 3.2 libretro GLSL on GL 3.3 core
re2DJ has an OpenGL 2.1 compatibility context and compiles GLSL 1.20 as it is. repsp has a 3.3 core context (3.0 on GLES), so source assembly translates:

* A first-line `#version` is dropped. The context's own version goes in its place: `#version 330 core`, or `#version 300 es` with `precision` declarations on GLES.
* After it come the stage define (`VERTEX`/`FRAGMENT`), `PARAMETER_UNIFORM`, and compatibility macros:
  * vertex: `attribute` becomes `in`, `varying` becomes `out`;
  * fragment: `varying` becomes `in` and `texture2D` becomes `texture`. One output variable is declared, and `gl_FragColor` is renamed to it.
  * `gl_FragColor` is not renamed with `#define`, because some drivers reject macro names starting with `gl_`. It is replaced token by token.
* libretro shaders' `#if __VERSION__ >= 130` branches (`COMPAT_VARYING` and the like) take the right side by themselves, since `__VERSION__` is 330.
* A core context has no client-side vertex arrays, so the unit square is drawn with a VAO and VBO. Attribute locations are bound before linking: `VertexCoord`=0, `COLOR`=1, `TexCoord`=2.
* Assembly is a GL-free function, so unit tests check its output string.

### 3.3 Size uniforms
* `TextureSize` and `InputSize` are the input texture's actual pixel size, libretro's own meaning: 960x544 at rendering scale 2. Upscalers such as FSR and ScaleFX use that grid as it is.
* `OutputSize` is the pixel size of the aspect rectangle.
* `LogicalSize` is the game's logical resolution (480x272), a name repsp adds. A shader uses it to draw one beam per original scanline whatever the rendering scale.
  * re2DJ's built-in `crt` and `scanline` used `TextureSize` as the logical resolution. In repsp both are changed to use `LogicalSize`.
* Multi-pass names are as in re2DJ: `Texture`, `OrigTexture`, `PassPrevNTexture`, each with `...TextureSize` and `...InputSize` (that texture's pixel size).
* `FrameCount`, `FrameDirection` (1) and `MVPMatrix` are as in re2DJ.

### 3.4 Filtering
* A single-pass `.glsl`, or a preset's first pass without `filter_linear0`, samples its input with the current output filter (nearest or linear).
* A preset's `filter_linearN` is followed where it is given.
* A scaled last pass is drawn to the window linearly, as in re2DJ.

### 3.5 Listing and files
* The app names the shader directory (`SetPostShaderDirectory`); repsp chooses no path.
* The list holds the built-ins in a fixed order, then the directory's `.glsl` and `.glslp` files by name. A missing or unreadable directory lists built-ins only.
* Ids: `none`, a built-in name (`crt`), or a file name (`my_crt.glsl`, `my_chain.glslp`).
* CMake embeds the built-in shaders from `src/gpu/post_shaders/` (or a fitting place). The generated file stays in the build directory.

### 3.6 Control contract (`include/repsp/platform/post_shader.h`)
* It does what re2DJ's `PostShaderControl` does, with repsp names and namespaces:
  * the list, the shader directory, the current id and the last error;
  * `Select(id)` compiles at once. On failure the shader becomes `none` and the error is kept;
  * `Reload()` lists again and recompiles the current shader;
  * `parameters()` gives the `#pragma parameter` values, adjustable in place.
* These methods are called only on the UI thread, which holds the GL context. The overlay builder runs on that thread, so the menu calls them directly.
* The shader the app chooses before the window opens is passed with `SetInitialPostShader(id)`, and the UI thread applies it when the window opens. If it cannot be applied, the log reads `post shader '<id>' not applied: <reason>`.
* With no GL context there is no control object (null). The app then hides or disables the menu item.

### 3.7 Failure
* A compile, link or target failure never blacks out the picture. It draws with `none` and records the reason in the log and `last_error`.
* The game keeps running when a shader chosen during play fails.

## 4. reDMP1

* **Graphics tab**: a Screen shader item, laid out like re2DJ's `DrawPostShaderMenu`:
  * a combo with `none` and the list;
  * a Reload button and the shader directory's path;
  * errors in red;
  * parameter sliders (value, minimum, maximum, step) and a reset to defaults;
  * with no GL context (the software renderer on the SDL renderer path), the item is disabled with a one-line reason.
* **Shader directory**: `shaders/` in the working directory when it exists, otherwise `shaders/` beside the executable.
* **`redmp1.ini`**: `post_shader = none`, saved when the menu changes it. Parameter values are not saved, as in re2DJ; choosing a shader again restores its defaults.
* **Precedence**:
  1. `--post-shader <id>` (or `--post-shader=<id>`);
  2. `REDMP1_POST_SHADER`;
  3. `redmp1.ini`;
  4. otherwise `none`.

  Values from the command line or the environment are not written to the file. `--post-shader` with a missing or empty value is a usage error.
* The startup log names the chosen shader.

## 5. Measurement and verification

1. Check in windowed mode:
   * Each built-in applies, and choosing another in the menu changes the picture at once. `none` looks as before.
   * Parameter sliders take effect at once.
   * A `.glsl` and a `.glslp` put in `shaders/` appear in the list after Reload. Broken on purpose, they show an error and the picture is `none`.
   * The picture is right (orientation, position and size) under all three aspect modes, both output filters, rendering scales 1, 2 and 4, and with MSAA on.
   * The chosen shader persists across runs, and `--post-shader` and the environment variable take precedence.
2. **Performance**: with `none`, the present interval stays in the earlier range (input script to gameplay, three runs). Each built-in's cost is recorded at a 3x window or larger (throughput with vsync off where possible).
3. **Probe** (no window; a hidden window's GL context is allowed):
   * All five built-ins compile and link on 3.3 core.
   * `none` gives the same pixels as drawing with no shader.
   * `scanline` darkens one row per logical scanline, counted against 272 lines even at rendering scale 2.
   * A two-pass preset chains.
   * A broken shader falls back to `none` with the error kept.
   * A `flip_y` input comes out the right way up.
4. **Unit tests** (no GL):
   * `#pragma parameter` parsing;
   * source assembly: compatibility macros, the `gl_FragColor` rename, the `#version` replacement;
   * `.glslp` parsing;
   * listing;
   * precedence of command line, environment and file.
5. The software renderer, `--headless`, and a configuration without game data behave and build as before.
6. Win64 Debug/Release (including the Visual Studio generator) and Linux x64, build and CTest, for repsp and reDMP1. No GCC warnings.
7. The user checks the shaders in windowed mode (the user's check).

## 6. Records

* **repsp**:
  * `ARCHITECTURE.md`: presentation-stage post-processing and the control contract.
  * `docs/`: a guide to writing shaders, adapted from re2DJ's guide and kb (the GL 3.3 core translation, `LogicalSize`).
  * `THIRD_PARTY_NOTICES.md`: re2DJ (BSD 3-Clause), FSR 1.0, ScaleFX and Anime4K.
* **reDMP1**:
  * `README.md` (or the usage guide): the Screen shader menu, `--post-shader`, `REDMP1_POST_SHADER`, `shaders/`, and `post_shader` in `redmp1.ini`.
  * Work log `docs/work-logs/20261118-i049-screen-shaders.md`: measurements and questions.
* Screenshots are not committed.
