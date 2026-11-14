# 설계: OpenGL 렌더러 — 옵션, 확대, 소프트웨어 렌더러와의 비교 (issue #24)

## 1. 요구사항

* repsp의 OpenGL 렌더 백엔드([repsp issue #24](https://github.com/reexec/repsp/issues/24))를 앱에서 고를 수 있게 한다.
* 지금까지 이른 화면들(오프닝, 로딩, 타이틀, 모드 선택, 곡 선택, 플레이)에서 OpenGL의 결과가 소프트웨어 렌더러와 같은지 비교한다.
* 기본 렌더러는 소프트웨어로 둔다. 기본값을 바꿀지는 비교 결과를 보고 사용자가 정한다.
* 함께: `scripts/build_win64.bat`의 `ffmpeg`·`notest` 옵션(이미 작업 트리에 있음)을 커밋하고 빌드 가이드에 적는다.

## 2. 앱 옵션

* `--renderer <sw|gl>`(기본 `sw`). `gl`을 골랐는데 GL 문맥을 만들 수 없으면 이유를 한 줄로 알리고 `sw`로 계속한다.
* `--scale <n>`(1~8, 기본 2): 창 크기 배율. `gl`에서는 내부 해상도 배율도 같은 값으로 한다. `sw`에서는 창만 커진다.
* `--headless --renderer gl`: 숨긴 창으로 GL 문맥을 만들어 실행한다(덤프 비교용).
* 정지 보고에 렌더러 이름과 OpenGL 백엔드의 통계(그리기 호출, 텍스처 캐시, 근사·미지원 횟수)를 더한다.
* CTest `app_boot`는 `sw` 그대로다.

## 3. 비교 방법

* 같은 입력 스크립트로 `sw`와 `gl`(배율 1)을 각각 실행해 프레임 덤프를 남긴다.
* 호스트 시각에 따라 프레임이 어긋나므로 프레임 번호로 짝짓지 않는다. 입력을 기다리며 멈춰 있는 화면(타이틀, 모드 선택, 곡 선택)에서 양쪽의 프레임을 골라 비교한다. 움직이는 요소가 있으면(반짝임, 스크롤) 차이가 그 영역에 국한되는지 본다.
* 비교 도구(`tools/` 아래의 스크립트, 표준 라이브러리만): 두 BMP의 채널별 최대 차이, 차이가 허용 오차(±1)를 넘는 픽셀 수와 그 경계 상자, 차이 영상(로컬에만 저장).
* 플레이 화면처럼 계속 움직이는 화면은 사람이 양쪽 덤프를 보고 구조적 차이(빠진 요소, 색, 가장자리)를 확인한다.
* 차이가 나오면 원인을 가려 repsp submodule 브랜치에서 고치고 `gl_probe`에 그 경우를 더한다. 소프트웨어 쪽이 틀린 것으로 드러나면 그쪽을 고친다(GE의 실제 동작이 기준이다; 확인하지 못하면 **추정**으로 적고 두 백엔드를 같게 맞춘다).
* 덤프와 차이 영상은 저장소에 넣지 않는다.

## 4. 성능 기록

* 플레이 화면 구간에서 `sw`와 `gl`의 프레임 시간(표시 간격의 평균과 최댓값), CPU 사용을 재서 작업 로그에 적는다. 측정 수단이 없으면 정지 보고에 "표시한 프레임 수 / vblank 수"를 쓰고, 필요하면 간단한 프레임 시간 통계를 더한다.

## 5. 기록

* `docs/guides/running-and-tracing.md`: 새 옵션. `docs/guides/win64-setup-and-build.md`: bat 사용법(`ffmpeg`, `notest`).
* `docs/analysis/first-frame.md` 또는 새 절: 게임이 쓰는 GE 기능의 목록(primitive 종류, 텍스처 형식, 블렌드 조합, 테스트)을 OpenGL 백엔드의 통계로 확인한 대로 적는다. 원본 게임의 사실만.
* 작업 로그: 화면별 비교 수치, 고친 차이, 남은 차이와 그 이유, 성능 수치, 덤프를 남긴 위치.

## 6. 이번 task의 검증

1. `--renderer gl`(창 모드와 headless)로 플레이 화면까지의 입력 스크립트 실행이 `sw`와 같은 화면에 이른다. 디스패치 미스, 교착, GL 오류가 없다.
2. 멈춰 있는 화면 셋(타이틀, 모드 선택, 곡 선택)에서 배율 1의 차이가 허용 오차 안이거나, 남은 차이의 원인과 위치가 기록돼 있다.
3. 동영상 구간(CPU가 쓴 프레임버퍼)이 `gl`에서도 표시된다(플러그인이 있으면 그림, 없으면 검은 화면).
4. `--scale 3 --renderer gl`에서 창과 내부 해상도가 3배가 된다(읽어 온 대상의 크기로 확인).
5. `sw` 기본 구성의 기존 CTest가 그대로 통과한다. Win64 Debug/Release와 Linux x64 빌드·CTest, CI 통과. 게임 데이터 없는 구성도 경고 없이 빌드된다.
6. `scripts\build_win64.bat Release ffmpeg`가 끝까지 통과한다.

---

# Design: OpenGL renderer — option, scaling, comparison with the software renderer (issue #24)

## 1. Requirements

Make repsp's OpenGL render backend ([repsp issue #24](https://github.com/reexec/repsp/issues/24)) selectable in the app, and compare its output with the software renderer on the screens reached so far (opening, loading, title, mode select, song select, gameplay). The default renderer stays software; whether to change the default is the user's decision after seeing the comparison. Alongside: commit the `ffmpeg` and `notest` options of `scripts/build_win64.bat` (already in the working tree) and describe them in the build guide.

## 2. App options

`--renderer <sw|gl>` (default `sw`); if `gl` is chosen and no GL context can be created, one line gives the reason and the run continues with `sw`. `--scale <n>` (1 to 8, default 2) is the window size scale; with `gl` the internal resolution scale takes the same value, with `sw` only the window grows. `--headless --renderer gl` runs with a GL context on a hidden window (for dump comparisons). The stop report gains the renderer name and the OpenGL backend's statistics (draw calls, texture cache, approximated and unsupported counts). CTest `app_boot` stays on `sw`.

## 3. Comparison

The same input script is run with `sw` and with `gl` at scale 1, each leaving frame dumps. Frames drift with host time, so they are not paired by number: on screens that sit waiting for input (title, mode select, song select) a frame is picked from each side and compared, and where something moves (a glow, a scroll) the differences are checked to be confined to that area. The comparison tool (a script under `tools/`, standard library only) reports the largest difference per channel, the number of pixels beyond the tolerance (±1) with their bounding box, and a difference image (kept locally). Screens in constant motion, such as gameplay, are checked by eye on both dumps for structural differences (missing elements, colours, edges). A difference is traced to its cause and fixed on the repsp submodule branch, with the case added to `gl_probe`; if the software side turns out to be wrong, that side is fixed (the GE's real behaviour is the standard; where it cannot be confirmed it is recorded as **assumed** and the two backends are made to agree). Dumps and difference images are never committed.

## 4. Performance record

Over the gameplay stretch, frame time (mean and maximum interval between presentations) and CPU use are measured for `sw` and `gl` and recorded in the work log. Lacking a means of measurement, the stop report's "frames presented / vblanks" is used, with simple frame time statistics added if needed.

## 5. Record

`docs/guides/running-and-tracing.md`: the new options. `docs/guides/win64-setup-and-build.md`: how to use the batch file (`ffmpeg`, `notest`). `docs/analysis/first-frame.md` or a new section: the GE features the game uses (primitive kinds, texture formats, blend combinations, tests) as confirmed by the OpenGL backend's statistics — facts about the original game only. The work log: comparison figures per screen, differences fixed, differences remaining and why, performance figures, where dumps were kept.

## 6. Verification for this task

1. With `--renderer gl` (windowed and headless) the scripted run to the gameplay screen reaches the same screens as `sw`; no dispatch miss, deadlock or GL error.
2. On the three waiting screens (title, mode select, song select) the scale-1 difference is within tolerance, or the cause and location of what remains is recorded.
3. The movie stretch (CPU-written frame buffers) is shown with `gl` too (a picture with the plugin, black without).
4. With `--scale 3 --renderer gl` the window and the internal resolution are tripled (checked by the size of the target read back).
5. The existing CTest passes unchanged in the default `sw` configuration; Win64 Debug/Release and Linux x64 builds and CTest, and CI, pass; the configuration without game data builds without warnings.
6. `scripts\build_win64.bat Release ffmpeg` runs to the end.
