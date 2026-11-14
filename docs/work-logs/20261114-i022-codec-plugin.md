# Work Log: Optional FFmpeg Codec Plugin Integration (`i022-codec-plugin`)

## 작업 요약 (한국어)

### 1. 구현 내용
* **빌드 옵션 및 스테이징 ([`CMakeLists.txt`](../../CMakeLists.txt))**:
  * `REDMP1_CODEC_FFMPEG`(기본 `OFF`) 추가 → `REPSP_CODEC_FFMPEG` 전달.
  * `ON`일 때 `repsp_codec_ffmpeg`와 `REPSP_FFMPEG_SHARED_LIBS`(FFmpeg LGPL DLL)를 실행 파일 출력 디렉터리로 자동 복사(저장소 내부에는 복사하지 않음).
  * `app_boot` CTest는 무음/빈 프레임 계약(`AVC decode: blank`) 유지를 위해 `--no-codec-plugin`을 전달.
* **앱 연결 및 폴백 ([`src/app/main.cpp`](../../src/app/main.cpp))**:
  * 실행 파일 옆 `DefaultCodecPluginFilename()` 자동 탐색, `--codec-plugin <path>`, `--no-codec-plugin` 옵션 추가.
  * 로드 성공 시 `[redmp1] Codec plugin loaded: ffmpeg (FFmpeg libavcodec 62 (LGPL, dynamic))`을 1회 출력하고 `hle::atrac::SetDecoder`, `hle::mpeg::SetAtracDecoder`, `hle::mpeg::SetAvcDecoder`에 연결.
  * 누락·로드 실패 시 한 줄 사유를 출력하고 `SilentDecoder`/`BlankAvcDecoder`로 정상 폴백.
* **문서 갱신**:
  * [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md), [`ARCHITECTURE.md`](../../ARCHITECTURE.md), [`docs/guides/codec-plugin.md`](../guides/codec-plugin.md), [`docs/guides/README.md`](../guides/README.md), [`docs/analysis/audio.md`](../analysis/audio.md), [`docs/analysis/video.md`](../analysis/video.md).

### 2. 검증 결과
* **Win64 Release (`REDMP1_CODEC_FFMPEG=ON`)**: CTest **34/34 통과**.
* **Win64 Release (`REDMP1_CODEC_FFMPEG=OFF`)**: CTest **33/33 통과**.
* **Win64 Release 스텁 (`REDMP1_CODEC_FFMPEG=OFF`, `REDMP1_HAVE_RECOMP=0`)**: CTest **33/33 통과**.
* **오프닝 무비 (`--max-vblanks 450`, FFmpeg 활성)**: `avcDec=227/227`, `atracDec=171/171`, `REPSP_FRAME_DUMP` BMP 60프레임 중 38개 비흑색(고유 해시 26개), `REPSP_AUDIO_DUMP` WAV `331,264`프레임 RMS `841.2` · 피크 `8477` · 클리핑 `0`.
* **타이틀 → 곡 선택 → 플레이 (`--max-vblanks 2100`, 3회 실행)**: 3회 모두 종료 코드 `0` 및 VBlank 정지(`(e)`) 도달, `ATRAC decode: ffmpeg`(`736..741` 프레임), WAV `1,544,960`프레임 RMS `5147.6`. (실시간 오디오 풀 스레드 타이밍으로 프레임 수 ±2 내외 변동 존재.)
* **폴백 검증**: 플러그인 누락 및 `avcodec-62.dll` 누락 환경 모두 종료 코드 `0`으로 실행되며 `SilentDecoder`/`BlankAvcDecoder`로 정상 폴백.

---

## Work Summary (English)

### 1. Changes
* **Build & staging ([`CMakeLists.txt`](../../CMakeLists.txt))**: Added `REDMP1_CODEC_FFMPEG` (`OFF` by default) forwarding to `REPSP_CODEC_FFMPEG`, post-build copying of `repsp_codec_ffmpeg` and `REPSP_FFMPEG_SHARED_LIBS` next to `redmp1`, and `--no-codec-plugin` on `app_boot`.
* **App wiring & fallback ([`src/app/main.cpp`](../../src/app/main.cpp))**: Auto-discovers `repsp_codec_ffmpeg` next to the executable, adds `--codec-plugin <path>` and `--no-codec-plugin`, logs plugin metadata once, wires `hle::atrac`/`hle::mpeg`, and falls back cleanly to `SilentDecoder`/`BlankAvcDecoder`.
* **Docs**: Updated `THIRD_PARTY_NOTICES.md`, `ARCHITECTURE.md`, `docs/guides/codec-plugin.md`, `docs/guides/README.md`, `docs/analysis/audio.md`, and `docs/analysis/video.md`.

### 2. Verification
* Win64 Release (`REDMP1_CODEC_FFMPEG=ON`): CTest **34/34 passed**.
* Win64 Release (`REDMP1_CODEC_FFMPEG=OFF`): CTest **33/33 passed**.
* Win64 Release stub (`REDMP1_CODEC_FFMPEG=OFF`, `REDMP1_HAVE_RECOMP=0`): CTest **33/33 passed**.
* Opening movie (`--max-vblanks 450`): `avcDec=227/227`, `atracDec=171/171`, 38/60 non-black BMP frames (26 unique hashes), WAV RMS `841.2` with `0` clipped samples.
* Title to gameplay (`--max-vblanks 2100`, 3 runs): all 3 exited `0` at VBlank limit with `ATRAC decode: ffmpeg` (`736..741` frames) and WAV RMS `5147.6`.
* Fallback: verified clean fallback to `silent`/`blank` with exit code `0` when plugin or `avcodec-62.dll` is absent.
