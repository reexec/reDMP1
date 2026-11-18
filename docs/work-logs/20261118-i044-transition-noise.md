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
