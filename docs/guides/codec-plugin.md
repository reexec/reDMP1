# 선택적 FFmpeg 코덱 플러그인 빌드·실행·배포 가이드

> 근거 설계: [20261114-i022-codec-plugin.md](../design/20261114-i022-codec-plugin.md), 작업 로그: [20261114-i022-codec-plugin.md](../work-logs/20261114-i022-codec-plugin.md)

## 1. 기본 원칙
* 기본 빌드(`REDMP1_CODEC_FFMPEG=OFF`)는 FFmpeg에 의존하지 않으며 무음(`SilentDecoder`)과 빈 프레임(`BlankAvcDecoder`)으로 동작합니다.
* `REDMP1_CODEC_FFMPEG=ON`은 **LGPL 공유 빌드**의 FFmpeg(`libavcodec >= 60`)에만 동적으로 링크하는 선택적 플러그인(`repsp_codec_ffmpeg.dll` / `librepsp_codec_ffmpeg.so`)을 빌드합니다. GPL 빌드는 금지됩니다.

## 2. 개발용 FFmpeg 준비 및 빌드
1. 사용자가 직접 `third_party/repsp/scripts/fetch_ffmpeg.py`를 실행해 LGPL 공유 빌드를 내려받습니다(CMake는 자동으로 내려받지 않음).
2. `cmake -S . -B <build-dir> -DREDMP1_CODEC_FFMPEG=ON -DFFMPEG_ROOT=<ffmpeg-dir>`로 구성하고 빌드하면 실행 파일 옆에 `repsp_codec_ffmpeg`와 FFmpeg 공유 라이브러리가 복사됩니다.

## 3. 실행 옵션
* 기본 동작: 실행 파일 옆 `repsp_codec_ffmpeg`를 자동 탐색하여 로드합니다. 없거나 로드에 실패하면 한 줄 로그를 남기고 무음/빈 프레임으로 폴백합니다.
* `--codec-plugin <path>`: 지정한 경로의 플러그인을 로드합니다.
* `--no-codec-plugin`: 플러그인을 로드하지 않고 무음/빈 프레임 모드로 실행합니다.

## 4. 배포(릴리즈) 시 LGPL 준수 조건
FFmpeg 공유 라이브러리를 배포물에 함께 포함할 때는 다음을 반드시 지킵니다:
1. `THIRD_PARTY_NOTICES.md` 및 LGPL 라이선스 사본(`LICENSE.txt` / `COPYING.LGPL*`) 동봉
2. 사용한 BtbN `lgpl-shared` 빌드 태그와 대응 소스 안내(또는 소스 제공)
3. 사용자가 `avcodec-*.dll` / `avutil-*.dll` / `swresample-*.dll` (또는 `.so`)을 교체할 수 있는 동적 링크 유지

---

# Optional FFmpeg Codec Plugin Build, Run, and Shipping Guide

## 1. Overview
* The default build (`REDMP1_CODEC_FFMPEG=OFF`) has zero FFmpeg dependency and runs with `SilentDecoder` and `BlankAvcDecoder`.
* `REDMP1_CODEC_FFMPEG=ON` builds the optional `repsp_codec_ffmpeg` shared plugin dynamically linked against an **LGPL shared build** of FFmpeg (`libavcodec >= 60`). GPL builds are strictly prohibited.

## 2. Preparing FFmpeg and Building
1. Manually run `third_party/repsp/scripts/fetch_ffmpeg.py` (CMake never downloads FFmpeg automatically).
2. Configure with `-DREDMP1_CODEC_FFMPEG=ON -DFFMPEG_ROOT=<ffmpeg-dir>` and build; CMake stages the plugin and FFmpeg shared libraries next to `redmp1`.

## 3. Runtime CLI Options
* Default: auto-loads `repsp_codec_ffmpeg` next to the executable, falling back cleanly to silent/blank with a one-line log if missing or invalid.
* `--codec-plugin <path>`: load a plugin from an explicit path.
* `--no-codec-plugin`: disable plugin loading.

## 4. Release Distribution Requirements (LGPL)
When bundling FFmpeg LGPL shared libraries in a release, always include `THIRD_PARTY_NOTICES.md` + LGPL license text, corresponding source info for the exact FFmpeg build tag, and keep the shared libraries replaceable via dynamic linking.
