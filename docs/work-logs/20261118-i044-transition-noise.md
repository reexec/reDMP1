# 2026-11-18 작업 로그: 곡 표지 → 플레이 전환 노이즈 수정 (#44)

설계: [20261118-i044-transition-noise.md](../design/20261118-i044-transition-noise.md)
작업 지시: [20261118-i044-transition-noise.md](../work-orders/20261118-i044-transition-noise.md)

## 1. 후보 1~4 조사 및 배제 근거

1. **후보 1 (마지막 그리기가 원래 보이지 않아야 함 — GE 상태/명령 해석): 배제**
   * `GeInterpreter::DrawPrim`에서 정상 아이캐치(`vc=3481`)와 노이즈 프레임(`vc=3556`)의 전체 `DrawState`·정점·미지원 opcode를 덤프해 비교했다.
   * `vc=3481`과 `vc=3556` 모두 `prim=4, n=4, vtype=0x0000019F, pos0=(0,0), posN=(479,271), rgba0=0xFFFFFFFF, mat=0xFFFFFFFF, tme=1, tfunc=0, talpha=1, tdouble=0, tenv=0x00000000, blend=1, src=2, dst=3, op=0, ate=1, afunc=6, aref=0, amask=255, cte=0, zte=1, zfunc=7, zw=1, z=49603.9, sc=(0,0)-(479,271)`로 **모든 필드가 비트 단위로 동일**했고, 두 시점 사이에 상태 변경용 미지원 GE 명령도 없었다.
2. **후보 2 (덮어쓰기가 원래 다른 주소여야 함 — 메모리 할당 HLE): 배제**
   * `REPSP_HLE_TRACE=1`로 부팅 시 `SysMemUserForUser` 호출을 추적한 결과, `sceKernelAllocPartitionMemory(partid=2, type=0, size=0x01300000)`가 **단 1회** 호출되어 `sceKernelGetBlockHeadAddr`가 `0x08B96E00`을 반환했고(256바이트 정렬, User RAM `0x08800000..0x0A000000` 내부), 다른 파티션/힙 HLE 호출은 0건이었다.
   * `0x08C4DA60`은 그 단일 19 MiB 블록 안에서 게임 자체의 `CMemoryManager`가 배치한 주소다.
3. **후보 3 (표시 버퍼가 화면에 남지 않아야 함 — `sceDisplaySetFrameBuf` 동기/표시): 배제**
   * `sceDisplaySetFrameBuf(0x04044000, 512, 1, 0)`과 다음 프레임 `sceDisplaySetFrameBuf(0x04000000, 512, 1, 0)` 사이에 매번 `sceDisplayWaitVblankStartCB()`가 호출되어 한 vblank 경계를 온전히 넘기므로, 같은 vblank 내 덮어쓰기 문제가 아니었다.
4. **후보 4 (실행/그리기 순서가 원본과 다름 — 확정!)**:
   * `GeInterpreter::RunList`에서 노이즈 프레임의 `DrawPrim` 직전 GE PC와 `q->stall`을 찍은 결과, **`pc=0x08C3CF74 stall=0x48C3CE98`**(`vc=3560`), **`pc=0x08C3CFE8 stall=0x48C3CE98`**(`vc=3562`)가 관측되었다.
   * 게임은 `sceGeListEnQueue`와 `sceGeListUpdateStallAddr`에 PSP **비캐시 주소**(`0x48C3CE70`, `0x48C3CE98`, 비트 30 `0x40000000` 포함)를 넘기지만, 디스플레이 리스트 내부의 `BASE` + `JUMP`/`CALL`(`0x08`/`0x0A`)는 28/29비트 버스 주소(`0x08C3....`)를 산출한다.
   * 기존 `GeInterpreter`는 `q->list`와 `q->stall`의 상위 세그먼트 비트를 접지 않은 채 `if (q->stall && q->list == q->stall)`로 비교해 **`0x08C3CE98 != 0x48C3CE98`로 정지(stall)에 실패**했다.
   * 그 결과 `sceGuClear(7)` 직후 멈춰야 할 지점을 지나쳐, 같은 디스플레이 리스트 버퍼(`0x08C3CE70..`)에 이전 프레임부터 남아 있던 아이캐치 그리기 명령(`0x08C3CF74`, 이미 새 데이터로 덮어써진 `0x08C4DA60` 참조)까지 계속 실행한 것이 원인이었다.

## 2. 수정 및 합성 프로브

* **`GeInterpreter` 수정 (`third_party/repsp/src/gpu/ge_interpreter.cpp`)**:
  * `kGeAddrMask = runtime::GuestMemory::kFoldMask & ~3u` (`0x1FFFFFFCu`)를 적용하는 `FoldGeAddr(uint32_t addr)` 헬퍼를 추가했다.
  * `Enqueue`, `UpdateStallAddr`, `kGeJump`, `kGeCall`에서 `q->list`와 `q->stall`을 모두 `FoldGeAddr(...)`로 정규화해 CPU 캐시/커널 세그먼트 비트를 제거하고 29비트 물리/버스 주소로 비교하도록 고쳤다.
* **합성 프로브 (`third_party/repsp/tools/ge_probe/main.cpp`, Test 8)**:
  * 리스트 버퍼(`0x08980000`)의 stall 지점(`+0x28`) 뒤에 이전 프레임의 빨간 사각형(`0xFF0000FF`) 그리기 명령을 미리 심어 두고, 비캐시 주소(`0x48980000`, `stall=0x48980028`)로 큐잉한 뒤 내부 `BASE` + `JUMP`(`0x08980010`)를 거쳐 `sceGeListUpdateStallAddr`를 호출한다.
  * 수정 전에는 `0x08980028 != 0x48980028`로 stall이 풀리지 않고 빨간 사각형을 그려 실패하며, 수정 후에는 `kGeListStalled`로 정지하고 픽셀이 검은색(`0xFF000000`)으로 유지된다.

## 3. 측정 및 검증

* **두 곡 연속 전환 프레임 덤프 (`--headless --max-vblanks 3700`)**:
  * `--renderer sw`: 두 번째 곡 표지(`frame_1518.bmp`, `Luv Flow`) → 검은 전환 프레임(`frame_1519.bmp`) → 플레이 화면. **노이즈 프레임 0건**.
  * `--renderer gl`: 두 번째 곡 표지(`frame_2290.bmp`, `Luv Flow`) → 검은 전환 프레임(`frame_2291.bmp`) → 플레이 화면. **노이즈 프레임 0건**.
* **Release 3회 실행 (`--headless --renderer gl --max-vblanks 2400`)**:
  * 1회: `Exit code: 0`, `Dispatches: 0`, `Present: 1933 frames (interval mean=20.72 ms)` (`GE: 1800 lists, 1943594 commands`)
  * 2회: `Exit code: 0`, `Dispatches: 0`, `Present: 1927 frames (interval mean=20.78 ms)` (`GE: 1794 lists, 1934333 commands`)
  * 3회: `Exit code: 0`, `Dispatches: 0`, `Present: 1940 frames (interval mean=20.64 ms)` (`GE: 1807 lists, 1951166 commands`)
* **빌드 및 CTest**:
  * Win64 Ninja Release **35/35 통과**, Win64 Ninja Debug **35/35 통과**.
  * Visual Studio 18 2026 생성기 및 Linux x64(WSL) 검증 상태는 보고에 별도 기록한다.

## 4. 후속: 곡 선택 미리듣기 끊김 수정 (`#44b`)

### 4.1 재현 및 경로 확인

* 곡 선택 화면에 머무는 입력 스크립트(`%TEMP%\redmp1-build\i044b-song-select.txt`: `500 START`, `750 START`, `1000 START`, `1250 CIRCLE` 후 2600 vblank까지 대기)로 창 모드(`--renderer gl --max-vblanks 2600`) 및 `REPSP_AUDIO_DUMP`를 수집했다.
* 미리듣기는 **`sceAtrac` 스트리밍 → `sceAudio` `ch1` (`samples=1024, stereo`)** 경로로 출력된다(선택적 FFmpeg 플러그인 없이 `ATRAC decode: silent` 환경에서도 `ch1` underrun/shortfall, `ch1` 호출 수, `ATRAC frames`, `streamAdded`가 스트리밍 기아를 직접 드러낸다).

### 4.2 `repsp` 이분 탐색 결과 (동일 스크립트, 창 모드 `--renderer gl --max-vblanks 2600`)

| repsp 리비전 | Present (mean) | ch1 calls / frames | ch1 ur (short) | ch2 ur (short) | ATRAC frames / streamAdded |
| --- | --- | --- | --- | --- | --- |
| `782a4b4` (v0.0.19) | 1578 (27.48 ms) | 1097 / 1,315,840 | 71 (9,088) | 1184 (151,552) | 909 / 327,840 |
| `46dcaef` (v0.0.20) | 1580 (27.45 ms) | 1094 / 1,312,768 | 68 (8,704) | 105 (13,440) | 907 / 327,840 |
| `b3ca15a` (v0.0.21) | 1574 (27.56 ms) | 844 / 1,056,768 | **1985 (254,080)** | 129 (16,512) | **656 / 229,152** |
| `c293665` (#44 GE stall) | 1585 (27.37 ms) | 856 / 1,069,056 | **1981 (253,568)** | 814 (104,192) | **668 / 229,152** |

* 원인은 `46dcaef` → `b3ca15a`(`GL texture cache hashes textures in user RAM`) 단일 커밋이었다.
* `b3ca15a` 이후 `ResolveTextureLocked()`(`third_party/repsp/src/gpu/gl_backend.cpp`)가 모든 `GlDraw` 호출마다 User RAM 텍스처 전체를 **1바이트 단위 FNV-1a(`HashBytes`)** 로 매번 다시 해시해, 한 디스플레이 리스트에서 같은 텍스처를 여러 번 그릴 때도 게스트 CPU 시간을 과도하게 소모하며 `stream_sound_poll_thread`(`ch1`)가 제때 실행되지 못했다.

### 4.3 수정 및 합성 프로브

* **`third_party/repsp/src/gpu/gl_backend.cpp`**:
  * `HashBytes`를 4레인 64비트 워드(`std::memcpy`, 32바이트 청크 + 8바이트/1바이트 꼬리 + SplitMix64 마무리) 해시로 교체했다.
  * `GuestTexEntry`에 `clut_hash`와 `verified_epoch`를 두고 `FlushPendingLocked()`마다 `g_gl.flush_epoch`를 증가시켜, 같은 GE flush 내 반복 그리기는 CLUT만 확인하고 User RAM 텍스처 본문 재해시를 건너뛰되 다음 flush/프레임에서는 다시 해시해 `#43`(곡 표지 덮어쓰기)과 `#44`(GE stall) 동작을 보존했다.
* **`third_party/repsp/include/repsp/gpu/render_backend.h` & `tools/gl_probe/main.cpp`**:
  * `OpenGlStats::texture_bytes_hashed`를 추가하고, 512x512 RGBA8888(1 MiB) User RAM 텍스처를 한 flush 내에서 32회 그릴 때 `texture_bytes_hashed == 1 MiB`(수정 전에는 32 MiB로 실패), 다음 flush에서 덮어쓰기 후 그리면 `2 MiB` 및 새 색상 반영을 검증하는 결정적 합성 프로브를 추가했다.

### 4.4 수정 전후 창 모드 측정 및 CTest

* **수정 전 (`c293665`, 창 모드 `--renderer gl --max-vblanks 2600`, 2회)**:
  * 1회: `Present=1585 (mean=27.36 ms)`, `AUDIO_UR=2795`, `ch1 ur=1981 (short=253568)`, `ch2 ur=814 (short=104192)`, `ATRAC frames=668, streamAdded=229152`
  * 2회: `Present=1585 (mean=27.37 ms)`, `AUDIO_UR=2874`, `ch1 ur=1968 (short=251904)`, `ch2 ur=906 (short=115968)`, `ATRAC frames=681, streamAdded=229152`
* **수정 후 (창 모드 `--renderer gl --max-vblanks 2600`, 2회)**:
  * 1회: `Present=2196 (mean=19.75 ms)`, `AUDIO_UR=102`, **`ch1 ur=68 (short=8704)`**, **`ch2 ur=34 (short=4352)`**, `ATRAC frames=924, streamAdded=327840`
  * 2회: `Present=2194 (mean=19.77 ms)`, `AUDIO_UR=102`, **`ch1 ur=68 (short=8704)`**, **`ch2 ur=34 (short=4352)`**, `ATRAC frames=923, streamAdded=327840`
* **CTest**: Win64 Ninja Release **35/35 통과**, Win64 Ninja Debug **35/35 통과**.

---

# 2026-11-18 Work Log: Jacket-to-Gameplay Transition Noise Fix (#44)

Design: [20261118-i044-transition-noise.md](../design/20261118-i044-transition-noise.md)
Work order: [20261118-i044-transition-noise.md](../work-orders/20261118-i044-transition-noise.md)

## 1. Investigation & Elimination of Candidates 1–4

1. **Candidate 1 (Last draw should not be visible — GE state/command decoding): Ruled out**
   * Full `DrawState`, vertex attributes, and unsupported GE opcodes were dumped in `GeInterpreter::DrawPrim` for both the normal eyecatch (`vc=3481`) and the noise draw (`vc=3556`).
   * Every field was bit-for-bit identical (`prim=4, n=4, vtype=0x0000019F, pos0=(0,0), posN=(479,271), rgba0=0xFFFFFFFF, mat=0xFFFFFFFF, tme=1, tfunc=0, talpha=1, tdouble=0, tenv=0x00000000, blend=1, src=2, dst=3, op=0, ate=1, afunc=6, aref=0, amask=255, cte=0, zte=1, zfunc=7, zw=1, z=49603.9, sc=(0,0)-(479,271)`), with no intervening unsupported state-changing opcodes.
2. **Candidate 2 (Overwrite should land elsewhere — memory allocation HLE): Ruled out**
   * `REPSP_HLE_TRACE=1` at boot showed `SysMemUserForUser:sceKernelAllocPartitionMemory(partid=2, type=0, size=0x01300000)` is called **once**, returning `0x08B96E00` via `sceKernelGetBlockHeadAddr` (256-byte aligned inside User RAM `0x08800000..0x0A000000`), with zero other partition or heap HLE calls.
   * `0x08C4DA60` is placed inside that single 19 MiB block by the game's own `CMemoryManager`.
3. **Candidate 3 (Buffer should not be displayed — `sceDisplaySetFrameBuf` sync/present): Ruled out**
   * `sceDisplayWaitVblankStartCB()` is called between `sceDisplaySetFrameBuf(0x04044000, 512, 1, 0)` and the next `sceDisplaySetFrameBuf(0x04000000, 512, 1, 0)`, so the buffer crosses a full vblank boundary rather than being overwritten within the same vblank.
4. **Candidate 4 (Execution/draw order differs from original — Confirmed!)**:
   * Logging GE PC and `q->stall` in `GeInterpreter::RunList` right at the noise `DrawPrim` revealed **`pc=0x08C3CF74 stall=0x48C3CE98`** (`vc=3560`) and **`pc=0x08C3CFE8 stall=0x48C3CE98`** (`vc=3562`).
   * The game passes **uncached** addresses (`0x48C3CE70`, `0x48C3CE98`, with bit 30 `0x40000000` set) to `sceGeListEnQueue` and `sceGeListUpdateStallAddr`, whereas `BASE` + `JUMP`/`CALL` (`0x08`/`0x0A`) inside the list resolve to 28/29-bit bus addresses (`0x08C3....`).
   * Because `GeInterpreter` did not fold the upper segment bits on `q->list` and `q->stall`, `if (q->stall && q->list == q->stall)` compared **`0x08C3CE98 == 0x48C3CE98` (false)** and failed to stall.
   * Consequently, after `sceGuClear(7)`, execution blew past the stall point into stale leftover eyecatch draw commands (`0x08C3CF74`, referencing already-overwritten `0x08C4DA60`) remaining in the display list buffer from earlier frames.

## 2. Fix & Synthetic Probe

* **`GeInterpreter` fix (`third_party/repsp/src/gpu/ge_interpreter.cpp`)**:
  * Added `FoldGeAddr(uint32_t addr)` applying `kGeAddrMask = runtime::GuestMemory::kFoldMask & ~3u` (`0x1FFFFFFCu`).
  * Normalized `q->list` and `q->stall` in `Enqueue`, `UpdateStallAddr`, `kGeJump`, and `kGeCall` so CPU cache/kernel segment bits are stripped and list PC vs stall comparisons use 29-bit physical/bus addresses.
* **Synthetic probe (`third_party/repsp/tools/ge_probe/main.cpp`, Test 8)**:
  * Pre-populates a stale red rectangle (`0xFF0000FF`) right after the stall point (`+0x28`) in a list buffer at `0x08980000`, enqueues the list using uncached `0x48980000` (`stall=0x48980028`), executes a `BASE` + `JUMP` to `0x08980010`, and updates the stall address to `0x48980028`.
  * Fails before the fix (runs past `0x08980028 != 0x48980028` and draws red), and passes after (`kGeListStalled`, pixel remains `0xFF000000`).

## 3. Measurements & Verification

* **Two-song transition frame dumps (`--headless --max-vblanks 3700`)**:
  * `--renderer sw`: Song 2 jacket (`frame_1518.bmp`, `Luv Flow`) → clean black transition frame (`frame_1519.bmp`) → gameplay. **Zero noise frames**.
  * `--renderer gl`: Song 2 jacket (`frame_2290.bmp`, `Luv Flow`) → clean black transition frame (`frame_2291.bmp`) → gameplay. **Zero noise frames**.
* **3 Release runs (`--headless --renderer gl --max-vblanks 2400`)**:
  * Run #1: `Exit code: 0`, `Dispatches: 0`, `Present: 1933 frames (interval mean=20.72 ms)` (`GE: 1800 lists, 1943594 commands`)
  * Run #2: `Exit code: 0`, `Dispatches: 0`, `Present: 1927 frames (interval mean=20.78 ms)` (`GE: 1794 lists, 1934333 commands`)
  * Run #3: `Exit code: 0`, `Dispatches: 0`, `Present: 1940 frames (interval mean=20.64 ms)` (`GE: 1807 lists, 1951166 commands`)
* **Builds & CTest**:
  * Win64 Ninja Release **35/35 passed**, Win64 Ninja Debug **35/35 passed**.
  * Visual Studio 18 2026 generator and Linux x64 (WSL) status are recorded in the report.

## 4. Follow-up: Song Select Preview Stutter Fix (`#44b`)

### 4.1 Reproduction & Audio Path

* Captured windowed-mode runs (`--renderer gl --max-vblanks 2600`) with `REPSP_AUDIO_DUMP` using `%TEMP%\redmp1-build\i044b-song-select.txt` (`500 START`, `750 START`, `1000 START`, `1250 CIRCLE`, then idle on Song Select).
* Preview music plays via **`sceAtrac` streaming -> `sceAudio` `ch1` (`samples=1024, stereo`)**. Even without the optional FFmpeg codec plugin (`ATRAC decode: silent`), `ch1` underrun/shortfall, `ch1` call counts, `ATRAC frames`, and `streamAdded` directly measure streaming starvation.

### 4.2 `repsp` Bisect (Same Script, Windowed `--renderer gl --max-vblanks 2600`)

| repsp revision | Present (mean) | ch1 calls / frames | ch1 ur (short) | ch2 ur (short) | ATRAC frames / streamAdded |
| --- | --- | --- | --- | --- | --- |
| `782a4b4` (v0.0.19) | 1578 (27.48 ms) | 1097 / 1,315,840 | 71 (9,088) | 1184 (151,552) | 909 / 327,840 |
| `46dcaef` (v0.0.20) | 1580 (27.45 ms) | 1094 / 1,312,768 | 68 (8,704) | 105 (13,440) | 907 / 327,840 |
| `b3ca15a` (v0.0.21) | 1574 (27.56 ms) | 844 / 1,056,768 | **1985 (254,080)** | 129 (16,512) | **656 / 229,152** |
| `c293665` (#44 GE stall) | 1585 (27.37 ms) | 856 / 1,069,056 | **1981 (253,568)** | 814 (104,192) | **668 / 229,152** |

* The regression is isolated to `b3ca15a` (`GL texture cache hashes textures in user RAM`).
* After `b3ca15a`, `ResolveTextureLocked()` (`third_party/repsp/src/gpu/gl_backend.cpp`) ran a **byte-by-byte FNV-1a hash (`HashBytes`)** over the entire User RAM texture on every single `GlDraw` call, even when the same texture was drawn repeatedly within one display list, starving `stream_sound_poll_thread` (`ch1`).

### 4.3 Fix & Synthetic Probe

* **`third_party/repsp/src/gpu/gl_backend.cpp`**:
  * Replaced byte-by-byte FNV-1a in `HashBytes` with a 4-lane 64-bit word hash (`std::memcpy`, 32-byte chunks + 8-byte/1-byte tail + SplitMix64 finalizer).
  * Added `clut_hash` and `verified_epoch` to `GuestTexEntry`, advancing `g_gl.flush_epoch` in `FlushPendingLocked()`, so repeated draws of the same `TextureKey` within one GE flush only check `clut_hash` and skip re-hashing User RAM texture bytes while still re-hashing on subsequent flushes/frames (preserving `#43` jacket overwrite detection and `#44` GE stall fix).
* **`third_party/repsp/include/repsp/gpu/render_backend.h` & `tools/gl_probe/main.cpp`**:
  * Added `OpenGlStats::texture_bytes_hashed` and a deterministic probe that draws a 512x512 RGBA8888 (1 MiB) User RAM texture 32 times in one flush (asserting `texture_bytes_hashed == 1 MiB`, failing before the fix with `32 MiB`) and verifies overwrite detection on the next flush (`2 MiB`).

### 4.4 Before/After Windowed Measurements & CTest

* **Before fix (`c293665`, windowed `--renderer gl --max-vblanks 2600`, 2 runs)**:
  * Run 1: `Present=1585 (mean=27.36 ms)`, `AUDIO_UR=2795`, `ch1 ur=1981 (short=253568)`, `ch2 ur=814 (short=104192)`, `ATRAC frames=668, streamAdded=229152`
  * Run 2: `Present=1585 (mean=27.37 ms)`, `AUDIO_UR=2874`, `ch1 ur=1968 (short=251904)`, `ch2 ur=906 (short=115968)`, `ATRAC frames=681, streamAdded=229152`
* **After fix (windowed `--renderer gl --max-vblanks 2600`, 2 runs)**:
  * Run 1: `Present=2196 (mean=19.75 ms)`, `AUDIO_UR=102`, **`ch1 ur=68 (short=8704)`**, **`ch2 ur=34 (short=4352)`**, `ATRAC frames=924, streamAdded=327840`
  * Run 2: `Present=2194 (mean=19.77 ms)`, `AUDIO_UR=102`, **`ch1 ur=68 (short=8704)`**, **`ch2 ur=34 (short=4352)`**, `ATRAC frames=923, streamAdded=327840`
* **CTest**: Win64 Ninja Release **35/35 passed**, Win64 Ninja Debug **35/35 passed**.
