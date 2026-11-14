# 2026-11-15 Issue #26 소리 결함 조사 및 효과음 반복 수정

관련 이슈: `reDMP1 #26`, `repsp #26`  
설계: [20261115-i026-sound-defects.md](../design/20261115-i026-sound-defects.md)  
작업 지시: [20261115-i026-sound-defects.md](../work-orders/20261115-i026-sound-defects.md)  
선행/자매 작업 로그: [third_party/repsp/docs/work-logs/20261115-i026-sound-defects.md](../../third_party/repsp/docs/work-logs/20261115-i026-sound-defects.md)

## 증상

* 오프닝 동영상(`PSP_GAME/USRDIR/movie/01.pmf`)의 소리는 정상적으로 들리지만, 그 이후 화면(타이틀/모드 선택/곡 선택/플레이 진입)에서 정상적인 소리와 비정상적인 소리가 섞여 들린다.
* 특히 모드 선택 화면을 포함한 **모든 효과음(단발 SFX)**이 한 번 울리고 끝나지 않고 계속 반복 재생된다.
* 느리게 재생되는 소리가 있다는 제보는 사용자 결정에 따라 이번 작업 범위에서 제외하고 후속 작업으로 미뤘다(조사 중 확인된 ATRAC 관련 이상 징후는 아래 미해결 항목에 기록).

## 원인(한 문장)

게임의 공통 효과음 재생 helper(`0x0885215C`)가 단발 효과음을 포함해 `__sceSasSetVoice` 호출마다 `loop=1`을 넘기고, 단발 VAG 음원은 `LoopRepeat(0x02)` 없이 `LoopEnd(0x01)` 블록과 `End+Mute(0x07)` 블록으로 끝나는데, `repsp`의 `DecodeNextVagBlock()`이 `LoopRepeat(0x02)` 또는 `v.loop` 중 하나만 참이어도(`||`) 처음으로 되돌리도록 구현되어 있어 `0x01` 블록에서 무한 반복이 발생했다.

## 진단 증거

진단 덤프 위치: `%TEMP%\redmp1-build\i026-diag\` (저장소에 커밋하지 않음)

1. `REPSP_SAS_TRACE=1` (`%TEMP%\redmp1-build\i026-diag\vag-flags.log`):
   * `v=1, 4, 5, 6, 7`(단발 SFX): 모두 `loop=1`로 설정되었으나 VAG 블록 플래그는 `00`(일반), `01`(`LoopEnd`, `0x02` 없음) 1개, 마지막 `07`(`End+Mute`) 1개였다(`flags(00=...,01=1,02=0,...,07=1)`).
   * `v=2, 3`(모드 선택 BGM 스테레오 페어): `loop=1`이며 VAG 블록 플래그는 `00`, `06`(`LoopStart|LoopRepeat`), `02`(`LoopRepeat`), 마지막 `03`(`LoopEnd|LoopRepeat`)으로 구성된다.
2. 게스트 역컴파일/재컴파일 코드(`src/game/dmp1/recomp/generated/dmp1_funcs_048.c:2448-2450`):
   * 공용 SAS 음성 설정 함수(`0x0885215C`)가 모든 호출에서 `a3 = 1`(`loop=1`)을 고정 전달한다.
3. `REPSP_ATRAC_TRACE=1` (`%TEMP%\redmp1-build\i026-diag\run.log`):
   * ATRAC3+ 스트림 3개(`Open id=0 mode=stream tag=0x0270 ch=2 rate=44100 blockAlign=384 spf=1024`)가 열리며 `smpl` 청크의 `loopEnd=1039283664`가 관측되었다.

## `repsp` 및 `reDMP1` 변경 내용

* `third_party/repsp` (`i026-sound-defects` 브랜치):
  * `REPSP_AUDIO_DUMP=<path>` 확장: 믹서 입력 채널별 WAV(`<stem>.ch<N>.wav`) 및 SAS 출력 WAV(`<stem>.sas.wav`) 덤프 지원 (`hle_audio.cpp`, `hle_sascore.cpp`).
  * `REPSP_SAS_TRACE=1` 및 `REPSP_ATRAC_TRACE=1` 진단 로그 추가 (`hle_sascore.cpp`, `hle_atrac.cpp`).
  * `DecodeNextVagBlock()` 루프 조건을 `(flags & kVagFlagLoopRepeat) != 0 || v.loop`에서 `&&`로 수정하여, VAG 블록 자체가 `LoopRepeat(0x02)`를 가진 경우에만 `v.loop`가 적용되도록 수정 (`hle_sascore.cpp`).
  * `sas_probe` 회귀 검증 2건(checks 5~6: `flags=0x01` + `loop=1` 정지, `flags=0x03` + `loop=0` 정지) 추가 (`tools/sas_probe/main.cpp`).
  * 지식 문서(`docs/kb/sascore-and-vag.md`) 갱신.
* `reDMP1` (`i026-sound-defects` 브랜치):
  * [docs/guides/running-and-tracing.md](../guides/running-and-tracing.md) §4에 `REPSP_AUDIO_DUMP`, `REPSP_SAS_TRACE=1`, `REPSP_ATRAC_TRACE=1` 사용법 추가.
  * [docs/analysis/audio.md](../analysis/audio.md) §8에 `--max-vblanks 2100` 구간 소리 목록(9항목) 및 `smpl` `loopEnd` 미해결 기록 추가.

## 검증 결과

### 1. 빌드 및 CTest

* `repsp` Win64 Release (`%TEMP%\redmp1-build\repsp-win64-rel`): **21/21 통과** (`sas_probe` 6/6 통과 포함)
* `repsp` Win64 Debug (`%TEMP%\redmp1-build\repsp-win64-dbg`): **21/21 통과** (`sas_probe` 6/6 통과 포함)
* `reDMP1` Win64 Release + FFmpeg (`%TEMP%\redmp1-build\redmp1-win64-rel-ff`, `FFMPEG_ROOT=third_party\repsp\third_party\_ffmpeg`): **35/35 통과** (`codec_ffmpeg_probe`, `sas_probe` 포함)
* `reDMP1` Win64 Debug (`%TEMP%\redmp1-build\redmp1-win64-dbg`): **34/34 통과**
* `reDMP1` Win64 Release without FFmpeg (`%TEMP%\redmp1-build\redmp1-win64-rel-noff`, `-DREDMP1_CODEC_FFMPEG=OFF`): **34/34 통과**

### 2. 수정 후 실게임 3회 실행 비교

실행 결과 위치: `%TEMP%\redmp1-build\i026-verify\` (`--max-vblanks 2100`, `play-script.txt`, 저장소에 커밋하지 않음)

| 회차 | Display / Presented | SAS 요약 | ATRAC 요약 | 비고 |
| --- | --- | --- | --- | --- |
| `run1` | `480x272 fmt=1 vblanks=2101 presented=926` | `instances=1 calls=2950 frames=755200 keyOn=7 keyOff=48 activeVoices=0` | `ffmpeg \| active=1 created=3 released=2 frames=696 samples=712704` | `REPSP_SAS_TRACE=1` 및 `REPSP_AUDIO_DUMP` 활성; `SetVoice` 7회 모두 `End` 7회로 종료 확인 |
| `run2` | `480x272 fmt=1 vblanks=2100 presented=1012` | `instances=1 calls=3463 frames=886528 keyOn=7 keyOff=48 activeVoices=0` | `ffmpeg \| active=1 created=3 released=2 frames=744 samples=761856` | 진단 비활성 결정성 확인 |
| `run3` | `480x272 fmt=1 vblanks=2100 presented=1237` | `instances=1 calls=3481 frames=891136 keyOn=7 keyOff=48 activeVoices=0` | `ffmpeg \| active=1 created=3 released=2 frames=721 samples=738304` | 진단 비활성 결정성 확인 |

`run1`의 `audio.sas.wav`는 `755200 frames (17.12s)`이며 1초 구간 RMS는 `[0, 0, 7354, 7257, 7473, 7699, 0, 0, 0, 4372, 0, 0, 0, 0, 0, 0, 0, 0]`으로, 단발 효과음 이후 무음(`0`)으로 정상 복귀함을 확인했다.

## 미해결 항목

* **느리게 재생되는 소리(후속 작업 이월)**: 사용자 결정에 따라 이번 작업에서 원인 분석 및 수정을 보류했다.
* **ATRAC `smpl` `loopEnd=1039283664` (`0x3DF236D0`)**: `REPSP_ATRAC_TRACE=1`에서 ATRAC3+ 스트림 3개 모두 `loopEnd=1039283664`로 관측되었다. `smpl` 청크 파싱 오프셋/엔디안 또는 원본 RIFF 데이터 특성 여부는 미확정이다.

## 사용자 창 모드 확인 안내

빌드된 실행 파일(`%TEMP%\redmp1-build\redmp1-win64-rel-ff\src\app\redmp1.exe`)을 창 모드로 실행하여 다음을 직접 들어 확인하는 것을 권장한다.
1. 타이틀 화면 `START` 입력 시 효과음이 1회만 울리고 멈추는지
2. 모드 선택 화면 이동/결정 효과음이 반복되지 않고 1회만 재생되는지, 배경음(`v=2,3`)만 자연스럽게 반복되는지
3. 곡 선택 및 플레이 진입 시 효과음과 키음이 꼬이지 않고 1회 재생 후 종료되는지

## 제약 및 후속 처리

* 샌드박스 환경 제약으로 `git commit` 및 WSL/Linux 빌드는 수행하지 않았으며, 변경 사항은 `reDMP1` 및 `third_party/repsp` 작업 트리에 그대로 두었다(커밋·push·Linux 빌드는 리뷰어가 진행).

---

# 2026-11-15 Issue #26 Sound Defects Investigation & Repeating SFX Fix

Related issues: `reDMP1 #26`, `repsp #26`  
Design: [20261115-i026-sound-defects.md](../design/20261115-i026-sound-defects.md)  
Work order: [20261115-i026-sound-defects.md](../work-orders/20261115-i026-sound-defects.md)  
Sibling work log: [third_party/repsp/docs/work-logs/20261115-i026-sound-defects.md](../../third_party/repsp/docs/work-logs/20261115-i026-sound-defects.md)

## Symptoms

* The opening movie (`PSP_GAME/USRDIR/movie/01.pmf`) audio plays normally, whereas subsequent screens (Title / Mode Select / Song Select / Gameplay entry) mix normal and abnormal audio.
* In particular, **all one-shot sound effects (SFX)**—not limited to Mode Select—keep repeating indefinitely instead of stopping after one playback.
* Reports of slow-playing sounds were deferred to a follow-up task per user decision (ATRAC-related anomalies observed during investigation are recorded under Unresolved below).

## Root Cause (One Sentence)

The game's shared SFX helper (`0x0885215C`) passes `loop=1` to every `__sceSasSetVoice` call—including one-shot SFX whose VAG streams terminate with `LoopEnd(0x01)` (without `LoopRepeat(0x02)`) followed by `End+Mute(0x07)`—while `repsp`'s `DecodeNextVagBlock()` looped whenever `(flags & kVagFlagLoopRepeat) != 0 || v.loop`, causing `0x01` blocks to wrap back to offset `0` endlessly.

## Diagnostic Evidence

Diagnostic dumps location: `%TEMP%\redmp1-build\i026-diag\` (not committed)

1. `REPSP_SAS_TRACE=1` (`%TEMP%\redmp1-build\i026-diag\vag-flags.log`):
   * `v=1, 4, 5, 6, 7` (one-shot SFX): all configured with `loop=1`, with VAG block flags `00` (normal), one `01` (`LoopEnd` without `0x02`), and one trailing `07` (`End+Mute`) (`flags(00=...,01=1,02=0,...,07=1)`).
   * `v=2, 3` (Mode Select BGM stereo pair): `loop=1` with VAG flags `00`, `06` (`LoopStart|LoopRepeat`), `02` (`LoopRepeat`), and trailing `03` (`LoopEnd|LoopRepeat`).
2. Recompiled guest helper (`src/game/dmp1/recomp/generated/dmp1_funcs_048.c:2448-2450`):
   * Helper `0x0885215C` unconditionally passes `a3 = 1` (`loop=1`).
3. `REPSP_ATRAC_TRACE=1` (`%TEMP%\redmp1-build\i026-diag\run.log`):
   * Three ATRAC3+ streams (`Open id=0 mode=stream tag=0x0270 ch=2 rate=44100 blockAlign=384 spf=1024`) were observed reporting `loopEnd=1039283664` from `smpl` parsing.

## Changes in `repsp` and `reDMP1`

* `third_party/repsp` (`i026-sound-defects` branch):
  * Extended `REPSP_AUDIO_DUMP=<path>` to emit per-channel mixer input WAVs (`<stem>.ch<N>.wav`) and SAS output WAV (`<stem>.sas.wav`) (`hle_audio.cpp`, `hle_sascore.cpp`).
  * Added `REPSP_SAS_TRACE=1` and `REPSP_ATRAC_TRACE=1` trace logs (`hle_sascore.cpp`, `hle_atrac.cpp`).
  * Changed `DecodeNextVagBlock()` loop condition from `||` to `&&` (`(flags & kVagFlagLoopRepeat) != 0 && v.loop`) so `v.loop` applies only when the VAG block itself carries `LoopRepeat(0x02)` (`hle_sascore.cpp`).
  * Added `sas_probe` regression checks 5–6 (`flags=0x01` + `loop=1` stops; `flags=0x03` + `loop=0` stops) (`tools/sas_probe/main.cpp`).
  * Updated `docs/kb/sascore-and-vag.md`.
* `reDMP1` (`i026-sound-defects` branch):
  * Documented `REPSP_AUDIO_DUMP`, `REPSP_SAS_TRACE=1`, and `REPSP_ATRAC_TRACE=1` in [docs/guides/running-and-tracing.md](../guides/running-and-tracing.md) §4.
  * Recorded the 9-entry `--max-vblanks 2100` sound inventory and unresolved `smpl` `loopEnd` in [docs/analysis/audio.md](../analysis/audio.md) §7.

## Verification

### 1. Builds and CTest

* `repsp` Win64 Release (`%TEMP%\redmp1-build\repsp-win64-rel`): **21/21 passed** (including `sas_probe` 6/6)
* `repsp` Win64 Debug (`%TEMP%\redmp1-build\repsp-win64-dbg`): **21/21 passed** (including `sas_probe` 6/6)
* `reDMP1` Win64 Release + FFmpeg (`%TEMP%\redmp1-build\redmp1-win64-rel-ff`, `FFMPEG_ROOT=third_party\repsp\third_party\_ffmpeg`): **35/35 passed** (including `codec_ffmpeg_probe` and `sas_probe`)
* `reDMP1` Win64 Debug (`%TEMP%\redmp1-build\redmp1-win64-dbg`): **34/34 passed**
* `reDMP1` Win64 Release without FFmpeg (`%TEMP%\redmp1-build\redmp1-win64-rel-noff`, `-DREDMP1_CODEC_FFMPEG=OFF`): **34/34 passed**

### 2. Post-Fix 3-Run Real-Game Comparison

Verification output location: `%TEMP%\redmp1-build\i026-verify\` (`--max-vblanks 2100`, `play-script.txt`, not committed)

| Run | Display / Presented | SAS Summary | ATRAC Summary | Notes |
| --- | --- | --- | --- | --- |
| `run1` | `480x272 fmt=1 vblanks=2101 presented=926` | `instances=1 calls=2950 frames=755200 keyOn=7 keyOff=48 activeVoices=0` | `ffmpeg \| active=1 created=3 released=2 frames=696 samples=712704` | With `REPSP_SAS_TRACE=1` & `REPSP_AUDIO_DUMP`; all 7 `SetVoice` calls ended (`End` count = 7) |
| `run2` | `480x272 fmt=1 vblanks=2100 presented=1012` | `instances=1 calls=3463 frames=886528 keyOn=7 keyOff=48 activeVoices=0` | `ffmpeg \| active=1 created=3 released=2 frames=744 samples=761856` | Clean run (no diagnostics) |
| `run3` | `480x272 fmt=1 vblanks=2100 presented=1237` | `instances=1 calls=3481 frames=891136 keyOn=7 keyOff=48 activeVoices=0` | `ffmpeg \| active=1 created=3 released=2 frames=721 samples=738304` | Clean run (no diagnostics) |

`run1` `audio.sas.wav` has `755200 frames (17.12s)` with 1s RMS `[0, 0, 7354, 7257, 7473, 7699, 0, 0, 0, 4372, 0, 0, 0, 0, 0, 0, 0, 0]`, confirming silence (`0`) resumes after one-shot SFX playback.

## Unresolved Items

* **Slow-playing sounds (deferred)**: Investigation and fixes deferred to a follow-up task per user decision.
* **ATRAC `smpl` `loopEnd=1039283664` (`0x3DF236D0`)**: Observed across all 3 ATRAC3+ streams under `REPSP_ATRAC_TRACE=1`; whether this stems from `smpl` chunk parsing or source RIFF metadata remains unconfirmed.

## Windowed-Mode Listening Check for User

Run `%TEMP%\redmp1-build\redmp1-win64-rel-ff\src\app\redmp1.exe` in windowed mode and verify:
1. Pressing `START` on Title plays the SFX once and stops.
2. Mode Select cursor/confirm SFX play once without repeating, while BGM (`v=2,3`) loops cleanly.
3. Song Select and Gameplay entry SFX/keysounds stop after one playback.

## Constraints & Follow-Up

* `git commit` and WSL/Linux builds were not executed due to sandbox constraints; changes remain in the working trees of `reDMP1` and `third_party/repsp` for the reviewer.
