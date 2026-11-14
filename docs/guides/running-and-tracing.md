# 실행 및 HLE 추적 가이드

이 문서는 사용자가 `redmp1`을 직접 실행해 커널·UMD HLE 부팅 상태와 NID 호출 추적을 확인하는 절차를 설명한다.

* 관련 설계: [20261114-i015-utility.md](../design/20261114-i015-utility.md)
* 관련 작업 로그: [20261114-i015-utility.md](../work-logs/20261114-i015-utility.md)

---

## 1. 기본 부팅 실행

1. Win64 Release 빌드 후 게임 디렉터리(`roms/`)를 인자로 전달해 실행한다. 창 없이 실행하려면 `--headless`를 붙인다(오디오는 더미 풀 클록으로 구동). 제한된 VBlank 수만큼만 실행하려면 `--max-vblanks <N>`을 지정하며, 정해진 VBlank 시점에 컨트롤러 입력을 주입하려면 `--input-script <파일>`을 사용한다.
   * `--renderer <sw|gl>`: 렌더 백엔드를 선택한다(기본값 `sw`). `gl`은 OpenGL 3.3 Core 백엔드를 사용하며, GL 문맥 생성에 실패하면 1줄 경고를 남기고 `sw`로 폴백한다. `--headless --renderer gl`은 숨김 창으로 GL 문맥을 만들어 실행한다.
   * `--scale <1..8>`: 창 배율(기본값 `2`)을 지정한다. `--renderer gl`에서는 내부 FBO 해상도(`480*scale × 272*scale`)에도 동일하게 적용된다.
   * `--save-dir <디렉터리>`: `ms0:/PSP/SAVEDATA`에 연결할 호스트 세이브 디렉터리를 지정한다. 생략하면 OS 사용자 데이터 디렉터리(`SDL_GetPrefPath` 기반) 아래 `savedata/`를 사용한다.
   * `--dialog-answer <yes|no>`: 그리지 않는 Yes/No 메시지 다이얼로그의 응답을 지정한다(기본값 `yes`).
   ```powershell
   .\build\win64-release\Release\redmp1.exe roms --variants src\game\dmp1\recomp\variants.json --headless --max-vblanks 120
   ```
2. 종료 후 출력되는 `[boot] === BOOT STOP REPORT ===`와 종료 코드를 확인한다.
   * **분류 (a) (종료 코드 `0`)**: 비커널 NID 미구현 지점 도달 또는 정상 종료(`sceKernelExitGame` 포함). `--input-script`로 오프닝 무비를 건너뛰면 `sceUtilitySavedata`/`MsgDialog`를 통과해 타이틀 화면까지 진행하며 `--max-vblanks`로 종료할 수 있다.

### 기본 컨트롤러 입력 매핑 (`sceCtrl`)

GUI 실행 시 SDL3 UI 스레드가 키보드·첫 번째 게임패드 폴링과 SDL3 오디오 스트림 출력을 담당하며, `--headless` 실행 시에는 중립 컨트롤러 상태(버튼 `0`, 아날로그 `128, 128`)와 더미 오디오 클록만 공급한다.

| PSP 입력 | 키보드 | 게임패드 (SDL3 1번 패드) |
| --- | --- | --- |
| D-Pad (Up / Right / Down / Left) | 방향키 | D-Pad |
| Cross / Circle / Square / Triangle | `Z` / `X` / `A` / `S` | South / East / West / North |
| L / R Trigger | `Q` / `W` | Left / Right Shoulder |
| Start / Select | `Enter` / `Backspace` | Start / Back |
| Analog Stick (`lx`, `ly`) | `I` / `J` / `K` / `L` | Left Stick X / Y |
   * **분류 (b) (종료 코드 `10` 또는 `11`)**: 커널 NID 미구현 또는 예기치 않은 런타임/HLE 정지.
   * **분류 (c) (종료 코드 `12`)**: CPU 폴트 또는 호스트 예외.
   * **분류 (e) (종료 코드 `0`)**: `--max-vblanks <N>` 상한 도달 정지.
   * 정지 보고서 하단에는 `Renderer:`, `OpenGL:`(`gl` 활성 시), `Present:`, `Display:`, `Ctrl:`, `GE:`, `Audio:`, `SAS:`, `ATRAC decode:`, `AVC decode:`, `Utility:`, `Dialog[i]:` 통계 줄이 출력된다.
3. 프레임버퍼가 latch된 프레임을 BMP로 저장하려면 `REPSP_FRAME_DUMP=<디렉터리>`와 `REPSP_FRAME_DUMP_LIMIT=<N>`을 설정한다(`--renderer gl`에서도 배율 1로 readback해 저장). 두 BMP의 픽셀 차이를 비교하려면 `python tools/compare_bmp.py <a.bmp> <b.bmp> [--tol 1] [--diff-out diff.bmp]`를 사용한다.

### `--input-script <파일>` 형식

무인 실행 중 특정 VBlank에서 버튼을 누르거나 떼려면 `--input-script`에 텍스트 스크립트를 전달한다(가장 최근에 도달한 타임스탬프의 버튼 마스크를 유지).

* 각 줄 형식: `<vblank> <버튼 토큰 ...>` (`#` 이후는 주석, 공백으로 여러 버튼 조합 가능).
* 지원 토큰(대소문자 무관): `START`, `SELECT`, `UP`, `RIGHT`, `DOWN`, `LEFT`, `L`(`LTRIGGER`, `L_TRIGGER`), `R`(`RTRIGGER`, `R_TRIGGER`), `TRIANGLE`, `CIRCLE`, `CROSS`, `SQUARE`, `NONE`(`CLEAR`, `0`, 또는 빈 토큰 목록으로 모든 버튼 해제).
```text
# VBlank 280에서 Start 버튼으로 오프닝 동영상 스킵
280 START
290 NONE
```

---

## 2. `REPSP_HLE_TRACE` 추적 실행

환경 변수 `REPSP_HLE_TRACE=1`을 설정하면 게스트가 호출하는 모든 HLE 함수의 이름, 인자 4개(`$a0`–`$a3`), 반환값을 순서대로 출력한다. `REPSP_HLE_TRACE_LIMIT`으로 최대 출력 건수를 제한할 수 있다.

```powershell
$env:REPSP_HLE_TRACE = "1"
.\build\win64-release\Release\redmp1.exe roms --variants src\game\dmp1\recomp\variants.json
```

실행 끝에는 항상 다음 요약이 함께 출력된다.

* `HLE Call Count Summary`: 라이브러리·함수별 호출 횟수
* `Guest Threads`: 생성된 게스트 스레드 상태(`module_start`, `user_main` 등)
* `Open Files`: 현재 열려 있는 VFS 파일 목록
* `User Partition Blocks`: 사용자 메모리 파티션 블록 현황

## 3. 게스트 참조 카운트·단언 진단 (`REDMP1_TRACE_GUEST`)

CMake 구성 시 `-DREDMP1_TRACE_GUEST=ON`을 켜면(`OFF`가 기본값이며 비활성 시 오버헤드 0) `src/recomp_abi/trace.c`의 게스트 진단 훅이 활성화된다. 실행 시 `REDMP1_TRACE_REFCOUNT=1`을 설정하면 `CInstance`/`Holder`/`List` 계열 호출과 게스트 `__assert` 인자(`file`, `line`, `expr`)를 표준 오류로 기록한다.

## 4. 오디오·SAS·ATRAC 진단

* **`REPSP_AUDIO_DUMP=<path.wav>`**: 최종 믹서 출력 WAV(`<path.wav>`)와 함께, 실제 출력이 발생한 `sceAudio` 채널별 믹서 입력 WAV(`<stem>.ch<N>.wav`) 및 `__sceSasCore` 믹서 출력 WAV(`<stem>.sas.wav`)를 함께 기록한다.
* **`REPSP_SAS_TRACE=1`**: `Init`, `SetVoice`(주소, 크기, `startOff`, `loop`, VAG 블록 `flags` 분포), `KeyOn`, `KeyOff`, `SetPitch`, 첫/특수 블록, 보이스 종료 사유, `GetEndFlag` 변화를 `[SAS_TRACE]`로 출력한다.
* **`REPSP_ATRAC_TRACE=1`**: `Open`(모드, 코덱 태그, 채널, 표본화 주파수, `block_align`, `spf`, `end_sample`, `delay`, 루프 지점), `SetLoopNum`, `ResetPlayPosition`, 디코딩 호출별 상태, `Release`를 `[ATRAC_TRACE]`로 출력한다.

---

# Running and HLE Tracing Guide

This guide describes how to run `redmp1` to inspect kernel/UMD HLE boot behavior and trace NID calls.

* Related design: [20261114-i015-utility.md](../design/20261114-i015-utility.md)
* Related work log: [20261114-i015-utility.md](../work-logs/20261114-i015-utility.md)

## 1. Default Boot Execution

```powershell
.\build\win64-release\Release\redmp1.exe roms --variants src\game\dmp1\recomp\variants.json --headless --max-vblanks 120
```

Pass `--max-vblanks <N>` to stop deterministically after `N` VBlanks, `--renderer <sw|gl>` to select the render backend (`sw` default; `gl` uses OpenGL 3.3 Core and falls back to `sw` with a 1-line warning if GL context creation fails; `--headless --renderer gl` uses a hidden GL window), `--scale <1..8>` to set window scale and GL internal FBO resolution (`480*scale x 272*scale`, default `2`), `--input-script <path>` to inject timed controller state, `--save-dir <dir>` to override the host `ms0:/PSP/SAVEDATA` directory (defaults to `savedata/` under `SDL_GetPrefPath`), and `--dialog-answer <yes|no>` to answer undrawn yes/no message dialogs (default `yes`). Under `--headless`, audio uses a dummy pull clock, whereas windowed mode opens the SDL3 audio stream. Check `[boot] === BOOT STOP REPORT ===` and the exit code:
* **Class (a) (`0`)**: Stopped at an unimplemented non-kernel NID or exited cleanly (`sceKernelExitGame`). After skipping the opening movie with `--input-script`, execution proceeds past `sceUtilitySavedata`/`MsgDialog` into the title screen until `--max-vblanks`.

### Default Controller Input Mapping (`sceCtrl`)

In GUI mode the SDL3 UI thread polls keyboard input and Gamepad #0 alongside the SDL3 audio stream; under `--headless` only neutral controller state (buttons `0`, analog `128, 128`) and the dummy audio pull clock are supplied.

| PSP Input | Keyboard | Gamepad (SDL3 Pad #0) |
| --- | --- | --- |
| D-Pad (Up / Right / Down / Left) | Arrow Keys | D-Pad |
| Cross / Circle / Square / Triangle | `Z` / `X` / `A` / `S` | South / East / West / North |
| L / R Trigger | `Q` / `W` | Left / Right Shoulder |
| Start / Select | `Enter` / `Backspace` | Start / Back |
| Analog Stick (`lx`, `ly`) | `I` / `J` / `K` / `L` | Left Stick X / Y |
* **Class (b) (`10` or `11`)**: Unimplemented kernel NID or unexpected runtime/HLE stop.
* **Class (c) (`12`)**: CPU fault or host exception.
* **Class (e) (`0`)**: Reached VBlank limit (`--max-vblanks <N>`).
* The stop report prints `Renderer:`, `OpenGL:` (when `gl` is active), `Present:`, `Display:`, `Ctrl:`, `GE:`, `Audio:`, `SAS:`, `ATRAC decode:`, `AVC decode:`, `Utility:`, and `Dialog[i]:` summary lines.

Set `REPSP_FRAME_DUMP=<dir>` and `REPSP_FRAME_DUMP_LIMIT=<N>` to dump latched frames as BMP files (reads back at scale 1 when `--renderer gl` is active). To compare two BMP dumps pixel-by-pixel, run `python tools/compare_bmp.py <a.bmp> <b.bmp> [--tol 1] [--diff-out diff.bmp]`.

### `--input-script <path>` Format

Each non-comment (`#`) line has `<vblank> <button tokens...>` (`START`, `SELECT`, `UP`, `RIGHT`, `DOWN`, `LEFT`, `L`/`LTRIGGER`/`L_TRIGGER`, `R`/`RTRIGGER`/`R_TRIGGER`, `TRIANGLE`, `CIRCLE`, `CROSS`, `SQUARE`, or `NONE`/`CLEAR`/`0` to release). Multiple space-separated tokens on one line are ORed; the most recent script entry with `entry.vblank <= current_vblank` remains latched.

## 2. Tracing with `REPSP_HLE_TRACE`

Set `REPSP_HLE_TRACE=1` (and optionally `REPSP_HLE_TRACE_LIMIT=<N>`) to log each HLE call with its first four arguments and return value, followed by the kernel call-count, thread, open-file, and user-partition summaries.

## 3. Guest Refcount and Assertion Diagnostics (`REDMP1_TRACE_GUEST`)

Configure CMake with `-DREDMP1_TRACE_GUEST=ON` (default `OFF`, zero runtime cost when disabled) to enable the diagnostic hook in `src/recomp_abi/trace.c`. At runtime, setting `REDMP1_TRACE_REFCOUNT=1` logs `CInstance`/`Holder`/`List` reference-count transitions and guest `__assert(file, line, expr)` failures to `stderr`.

## 4. Audio, SAS, and ATRAC Diagnostics

* **`REPSP_AUDIO_DUMP=<path.wav>`**: In addition to the final mixed WAV (`<path.wav>`), writes per-channel `sceAudio` mixer input WAVs (`<stem>.ch<N>.wav`) for active channels and the `__sceSasCore` mixer output WAV (`<stem>.sas.wav`).
* **`REPSP_SAS_TRACE=1`**: Logs `[SAS_TRACE]` lines for `Init`, `SetVoice` (address, size, `startOff`, `loop`, VAG block `flags` histogram), `KeyOn`, `KeyOff`, `SetPitch`, first/non-zero blocks, voice end reasons, and `GetEndFlag` changes.
* **`REPSP_ATRAC_TRACE=1`**: Logs `[ATRAC_TRACE]` lines for `Open` (mode, codec tag, channels, sample rate, `block_align`, `spf`, `end_sample`, `delay`, loop points), `SetLoopNum`, `ResetPlayPosition`, per-call `Decode`, and `Release`.
