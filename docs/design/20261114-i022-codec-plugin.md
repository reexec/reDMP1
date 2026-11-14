# 설계: 소리와 영상 — FFmpeg 코덱 플러그인을 앱에 연결 (issue #22)

## 1. 요구사항

* repsp의 선택적 FFmpeg 코덱 플러그인([repsp issue #21](https://github.com/reexec/repsp/issues/21))을 앱이 실행 시에 불러와, 배경 음악(ATRAC3plus)과 오프닝 동영상(H.264 Main, CABAC, 480×272)이 나오게 한다.
* **사용자 결정(2026-11-14)**: FFmpeg(LGPL 구성)에 동적으로 링크하는 선택적 플러그인을 허용한다(`AGENTS.md`의 예외). 플러그인이 없으면 지금처럼 무음·검은 프레임으로 동작한다.
* 기본 빌드와 CI는 FFmpeg 없이 그대로다.

## 2. 앱 쪽 동작

* **빌드**: `REDMP1_CODEC_FFMPEG`(기본 OFF)가 repsp의 `REPSP_CODEC_FFMPEG`를 켠다. ON이면 빌드 뒤 단계에서 플러그인과, repsp가 알려 준 FFmpeg 공유 라이브러리를 실행 파일의 출력 디렉터리로 복사한다(저장소 안으로는 복사하지 않는다).
* **불러오기**: 시작할 때 실행 파일 옆의 플러그인(`repsp_codec_ffmpeg` 공유 라이브러리)을 찾는다. `--codec-plugin <path>`로 위치를 지정할 수 있고, `--no-codec-plugin`으로 끌 수 있다. 불러오면 디코더를 `hle::atrac`·`hle::mpeg`에 넘긴다. 불러오지 못하면 한 줄로 이유를 알리고 기본 디코더로 계속한다(오류가 아니다).
* **보고**: 정지 보고의 `ATRAC decode:`·`AVC decode:` 줄이 실제 디코더 이름을 보인다. 시작 로그에 플러그인의 설명 문자열을 한 번 찍는다.
* CTest `app_boot`는 플러그인이 없는 구성을 기준으로 유지한다(정규식이 `AVC decode: blank`를 본다). 플러그인이 있는 구성에서는 `--no-codec-plugin`을 주어 같은 결과가 나오게 한다.

## 3. 받는 방법과 고지

* `docs/guides/`에 적는다: repsp의 `scripts/fetch_ffmpeg.py`로 고정한 LGPL 공유 빌드를 받는 방법, `FFMPEG_ROOT`와 `REDMP1_CODEC_FFMPEG=ON`으로 빌드하는 방법, Linux의 배포판 패키지를 쓰는 방법.
* `THIRD_PARTY_NOTICES.md`(또는 `CREDITS.md`)에 FFmpeg을 "선택적, 동적 링크, 저장소에 없음"으로 적는다. 릴리스 패키지에 FFmpeg을 함께 실을 때의 조건(고지, 라이선스 사본, 대응 소스 안내, 교체 가능성; GPL 구성 금지)을 릴리스 절차 문서에 적는다. 이번 task에서 릴리스 패키지를 만들지는 않는다.

## 4. 검증 수단

* **소리**: 오디오 출력을 WAV로 남기는 진단 기능을 쓴다(repsp에 없으면 `REPSP_AUDIO_DUMP=<file>`로 platform 오디오 계층에 더한다; 꺼져 있을 때 비용이 없어야 한다). 타이틀 화면과 플레이 화면 구간에서 무음이 아닌지(RMS), 클리핑이나 주기적 끊김이 없는지(프레임 경계의 불연속)를 수치로 확인한다. WAV 파일은 커밋하지 않는다.
* **영상**: 오프닝 구간의 프레임 덤프가 검은색이 아니고 프레임마다 내용이 바뀐다. 색이 뒤바뀌지 않았는지(R/B 교환, 색 범위)를 덤프로 확인한다.
* **맞물림**: 무음·빈 프레임일 때와 진행이 같다(같은 입력 스크립트로 같은 화면에 이른다). 실제 디코더로 달라지는 것(예: 곡 길이에 따른 진행)은 기록한다.
* 최종 확인(소리가 맞게 들리는지)은 사용자가 창 모드로 듣고 본다. 그 방법을 작업 로그에 적는다.

## 5. 기록

* `docs/analysis/audio.md`, `video.md`에 이어서: 실제 디코딩으로 드러난 사실(트랙의 채널·표본화 주파수·길이, 루프, 동영상의 프레임 종류와 재정렬 여부).
* `docs/guides/running-and-tracing.md`: 새 옵션과 진단 기능.
* 원본 게임의 사실만 적는다. 사용자 덤프의 위치나 이름, 사용자 계정 이름이 든 경로는 적지 않는다.

## 6. 이번 task의 검증

1. 플러그인 없는 빌드: 기존 CTest가 그대로 통과한다. 게임 데이터 없는 구성도 경고 없이 빌드된다.
2. 플러그인 있는 빌드(Win64): 시작 로그에 플러그인 이름이 나오고, 오프닝 프레임 덤프에 그림이 있고, 오디오 덤프가 무음이 아니다. 플레이 화면까지의 입력 스크립트 실행이 같은 화면에 이른다.
3. 플러그인을 지우거나 FFmpeg DLL을 지운 상태에서 실행하면 이유를 알리고 무음·검은 프레임으로 정상 진행한다.
4. 3회 실행을 비교하고 차이의 원인을 기록한다.
5. Win64 Debug/Release와 Linux x64 빌드·CTest(플러그인 없는 구성), CI 통과.

---

# Design: sound and video — wiring the FFmpeg codec plugin into the app (issue #22)

## 1. Requirements

The app loads repsp's optional FFmpeg codec plugin ([repsp issue #21](https://github.com/reexec/repsp/issues/21)) at run time so that background music (ATRAC3plus) and the opening movie (H.264 Main, CABAC, 480×272) play. **User decision (2026-11-14)**: an optional plugin that links dynamically against FFmpeg (an LGPL configuration) is allowed (the exception in `AGENTS.md`); without the plugin decoding stays silent and black. The default build and CI remain free of FFmpeg.

## 2. App behaviour

**Build**: `REDMP1_CODEC_FFMPEG` (default OFF) turns on repsp's `REPSP_CODEC_FFMPEG`; when ON, a post-build step copies the plugin and the FFmpeg shared libraries reported by repsp into the executable's output directory (never into the repository). **Loading**: at start-up the app looks for the plugin (the `repsp_codec_ffmpeg` shared library) next to the executable; `--codec-plugin <path>` names another location and `--no-codec-plugin` turns it off. On success the decoders are handed to `hle::atrac` and `hle::mpeg`; on failure one line states the reason and the run continues with the default decoders (not an error). **Report**: the `ATRAC decode:` and `AVC decode:` lines of the stop report show the real decoder names, and the plugin's description string is logged once at start-up. CTest `app_boot` stays based on the configuration without the plugin (its expression looks for `AVC decode: blank`); in a configuration with the plugin it passes `--no-codec-plugin` to get the same result.

## 3. Getting FFmpeg and notices

`docs/guides/` explains how to download the pinned LGPL shared build with repsp's `scripts/fetch_ffmpeg.py`, how to build with `FFMPEG_ROOT` and `REDMP1_CODEC_FFMPEG=ON`, and how to use distribution packages on Linux. `THIRD_PARTY_NOTICES.md` (or `CREDITS.md`) lists FFmpeg as optional, dynamically linked and absent from the repository. The conditions for shipping FFmpeg in a release package (notice, a copy of the licence, a pointer to the corresponding source, replaceability; no GPL configuration) go into the release procedure document. No release package is made in this task.

## 4. Means of verification

**Sound**: a diagnostic that writes the audio output to a WAV file is used (added to the platform audio layer as `REPSP_AUDIO_DUMP=<file>` if repsp lacks it; free when off). Over the title screen and gameplay stretches the output is checked numerically to be non-silent (RMS) and free of clipping and periodic glitches (discontinuities at frame boundaries). WAV files are never committed. **Video**: frame dumps of the opening are not black and change from frame to frame, and the dumps are checked for swapped colours (R/B exchange, colour range). **Interplay**: progress is the same as with silent and blank decoding (the same input script reaches the same screens); anything a real decoder changes (progress depending on song length, for instance) is recorded. The final check — whether it sounds right — is the user's, listening and watching in windowed mode; the work log says how.

## 5. Record

Continuing `docs/analysis/audio.md` and `video.md`: what real decoding reveals (channels, sample rate and length of tracks, loops, the movie's frame types and whether frames are reordered). `docs/guides/running-and-tracing.md`: the new options and diagnostics. Facts about the original game only — no location or name of the user's dump and no path containing a user account name.

## 6. Verification for this task

1. Build without the plugin: the existing CTest passes unchanged, and the configuration without game data builds without warnings.
2. Build with the plugin (Win64): the plugin's name appears in the start-up log, frame dumps of the opening show a picture, and the audio dump is not silent; the scripted run to the gameplay screen reaches the same screens.
3. With the plugin or the FFmpeg DLLs removed, the run states the reason and proceeds normally with silent and black decoding.
4. Three runs are compared and the cause of any difference recorded.
5. Win64 Debug/Release and Linux x64 builds and CTest (the configuration without the plugin), and CI, pass.
