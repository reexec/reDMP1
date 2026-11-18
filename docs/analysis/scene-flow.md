# 타이틀 화면 이후 메뉴 진입 및 `Instance.cpp:37` 단언 실패 분석 (`ULKS-46050`, `BOOT.BIN`)

> 표기 기준: **확인됨**(실행·디스어셈블리·덤프·프레임 확인), **추정**(정적 분석 또는 공개 자료 기반 가설)

---

## 1. 타이틀 이후 장면 전환 흐름 (**확인됨**)

```mermaid
flowchart TD
    TITLE["타이틀 화면 루프 (TitleScene)<br/>frame_0175..: PRESS START BUTTON"] -->|START 입력| CLEAN["TitleScene::Cleanup (0x0880AE40)"]
    CLEAN -->|객체 해제 루프| REL["List::Release (0x088485E8)<br/>CInstance 참조 카운트 감소"]
    REL --> NEXT["Mode Select / Main Menu 진입<br/>ROOKIE DJING / EASY MODE / 4Buttons<br/>X EXIT / O OK"]
    NEXT -->|CIRCLE 입력 (vblank 850)| SONG["Song Select (Rookie Music / 1st.STAGE)<br/>DISC 01/10 바람에게 부탁해<br/>X EXIT / O OK (frame_0563)"]
    SONG -->|CIRCLE 입력 (vblank 1350)| PLAY["Gameplay (4-Button PGP + BGA)<br/>ScorePlayerThread (prio=34) 기동<br/>노트 낙하 루프 (frame_0982)"]
```

* `--input-script`로 오프닝(`280 START`)과 타이틀(`400 START`)을 통과하면 `TitleScene::Cleanup`(`0x0880AE40`)이 리소스를 해제한 뒤 메인 메뉴(Mode Select: `ROOKIE DJING / EASY MODE / 4Buttons`, `X EXIT / O OK`) 화면으로 전환됩니다.
* `--max-vblanks 900` 실행에서 분류 `(e) VBlank limit reached` (`Exit code: 0`)로 종료되며, `Display: mode=0 480x272 fb=0x04000000 stride=512 fmt=1 vblanks=900 presented=400 dumped=400`, `GE: 266 lists, 82050 commands, 0 signals, 266 finishes`를 기록했습니다.
* **곡 선택 및 플레이 진입 (`--max-vblanks 2100`, 확인됨)**:
  * Mode Select에서 `CIRCLE`(`850 CIRCLE`)을 입력하면 Song Select(`Rookie Music`, `1st.STAGE`, `DISC 01/10 바람에게 부탁해`, `EFFECTOR SELECT`, `GEAR SYSTEM`, `X EXIT / O OK`) 화면으로 진입합니다.
  * Song Select에서 `CIRCLE`(`1350 CIRCLE`)을 입력하면 곡 데이터와 BGA 리소스(`all.mp` `fd=3`, `all2.mp` `fd=8` 추가 열기)를 로드하고 게스트 스레드 `ScorePlayerThread`(`uid=0x1D`, `prio=34`, `stack=0x8000`, 진입점 `0x0886141C`)를 생성·기동한 뒤 플레이 화면(배경 BGA 재생, 중앙 4버튼 PGP 기어, 노트 낙하 루프)으로 진입합니다.
  * `--max-vblanks 2100` 실행에서 분류 `(e)` (`Exit code: 0`), `Display: mode=0 480x272 fb=0x04044000 stride=512 fmt=1 vblanks=2100 presented=983 dumped=983`, `GE: 849 lists, 774726 commands, 0 signals, 849 finishes`로 종료되었습니다.

---

## 2. `Instance.cpp:37` (`assertion "m_referenceCount" failed`) 원인 확정 (**확인됨**)

* **증상**: 기존 구현에서는 타이틀 화면에서 `START`를 누른 직후 게스트 `__assert("Instance.cpp", 37, "m_referenceCount")`로 정지했습니다.
* **리프터 근본 원인 (`third_party/psprecomp/tools/allegrexrecomp/emit.c:791`)**:
  * `emit.c`의 `last_terminal = in.is_return || in.is_indirect || (in.is_jump && !in.is_call);` 판정이 간접 호출 명령어인 `jalr` (`in.is_indirect == 1`, `in.is_call == 1`)을 `jr`과 똑같이 종단(terminal) 명령어로 취급했습니다.
  * 이 때문에 `0x0885197C`의 `jalr $ra, $v0`으로 끝나는 분할 본문 `psp_body_08851930` 뒤에 이어지는 에필로그 블록 `psp_func_08851984()`로의 폴스루(fall-through) 호출이 누락되었습니다.
  * 그 결과 `TitleScene::Cleanup` (`0x0880AE40`)으로 복귀할 때 `$sp`와 `$fp`가 32바이트 낮게(`0x09FBF930` 대신 `0x09FBF950`이어야 함) 유지되었고, `0x0880AED0`이 `0($fp)`에서 `0x08B97774` 대신 `0x09D8CFB0`을 읽어 `List::Release(0x08C4D520, 0)`(`0x088485E8`)에 `0x00000000`을 넘겼습니다.
  * `List::Release`가 이미 초기화된 객체 `0x09B08080` (`m_referenceCount == 0`)과 매칭되면서 `Instance.cpp:37` 단언이 발동했습니다.
* **수정 (`patches/psprecomp/0003-jalr-fallthrough.patch`)**:
  * 간접 **비호출**(`in.is_indirect && !in.is_call`, 즉 `jr`)만 종단으로 취급하도록 패치하고 `BOOT.BIN`을 재리프트(`8785 functions, 17215 interior entries, 0.00% VFPU traps`)하여 원인을 해결했습니다.

## 3. 곡 선택 후 아이캐치(곡 표지) 텍스처 적재 특성 (**확인됨**)

* Song Select에서 `CIRCLE`을 누르면 디스크 확대 연출 뒤 전체 화면 곡 표지(아이캐치) 화면을 거쳐 플레이 화면으로 진입한다.
* 곡 리소스(`all.mp`, `all2.mp`에서 `sceIoOpenAsync` 경로로 비동기 읽기)가 User RAM(`0x08000000..0x0A000000`)에 적재되며, 대형 UI/표지 텍스처는 비-CLUT 형식(`fmt=1` ABGR1555 `512x256` `0x08C4DA60`, `fmt=2` ABGR4444 `512x256` `0x08DC1F50` / `256x256`, `fmt=3` ABGR8888 `512x256` `0x09219110`, 모두 `swz=1`)으로 게스트 User RAM 버퍼에 배치된 뒤 GE로 그려진다.
* 두 번째 곡을 시작할 때 게스트는 같은 User RAM 버퍼 주소·크기·형식으로 새 곡의 표지를 덮어쓰므로, 호스트 GL 텍스처 캐시가 User RAM 범위의 내용 해시를 계산하지 않으면 첫 번째 곡의 표지가 재사용된다(`repsp` `gl_backend.cpp` `HashGuestRange` 수정으로 해결).
* **아이캐치 → 플레이 전환 직전 1프레임 버퍼 재사용 (`0x08C4DA60`; reDMP1 실행에서 관측됨, 원본 동작인지는 **미확정**)**:
  * 아이캐치 진입 시 `0x08C4DA60`(`512x256`, `fmt=1`, `swz=1`, `262144` 바이트)에 표지 이미지를 배치해 `fb=0x04044000`에 그린 뒤 `sceDisplaySetFrameBuf(0x04044000)`로 표시한다.
  * 이어 곡 로딩 구간(약 80 vblank, `all.mp` 읽기 + 게스트 메모리 관리자 할당/해제) 동안 게스트 CPU 코드가 `0x08C4DA60` 영역을 플레이 화면 UI 텍스처 아틀라스 데이터로 덮어쓴다(`sceIoReadAsync`·`sceDmacMemcpy` 직접 덮어쓰기 아님).
  * 로딩 완료 직후 플레이 루프의 첫 프레임에서 게스트는 백버퍼(`fb=0x04000000`)를 지운 뒤(`clear` `0x00000000`), 덮어써진 `tex=0x08C4DA60`(`512x256`, `fmt=1`, `rgba0=0xFFFFFFFF`, `tfunc=0`)을 `(0,0)-(479,271)`에 한 번 더 그리고 `sceGeDrawSync(0)` → `sceDisplayWaitVblankStartCB()` → `sceDisplaySetFrameBuf(0x04000000)` 순서로 표시한 다음, 다음 프레임(`fb=0x04044000`)부터 정상 플레이 화면으로 넘어간다. 이 동작은 `--renderer sw`와 `--renderer gl` 모두에서 동일하게 재현된다. 다만 실기와 다른 에뮬레이터에서는 이 전환에 노이즈 프레임이 보이지 않으므로, 위 순서 가운데 하나(메모리 배치, 마지막 그리기의 색·알파, 표시 버퍼 선택 등)가 원본과 다를 수 있다([#44](https://github.com/reexec/reDMP1/issues/44)에서 조사).

---

# Post-Title Menu Entry and `Instance.cpp:37` Assertion Analysis (`ULKS-46050`, `BOOT.BIN`)

> Status labels: **Confirmed** (verified by runtime execution, disassembly, trace, and frame dump), **Inferred** (static analysis or external reference)

## 1. Post-Title Scene Transition Flow (**Confirmed**)

* Passing the opening (`280 START`) and title (`400 START`) prompts via `--input-script` runs `TitleScene::Cleanup` (`0x0880AE40`) and transitions cleanly into the Main Menu / Mode Select screen (`ROOKIE DJING / EASY MODE / 4Buttons`, `X EXIT / O OK`), verified visually in `frame_0399.png`.
* Running to `--max-vblanks 900` finishes cleanly with Class `(e) VBlank limit reached` (`Exit code: 0`), `Display: mode=0 480x272 fb=0x04000000 stride=512 fmt=1 vblanks=900 presented=400 dumped=400`, and `GE: 266 lists, 82050 commands, 0 signals, 266 finishes`.
* **Song Select and Gameplay Entry (`--max-vblanks 2100`, Confirmed)**:
  * Pressing `CIRCLE` (`850 CIRCLE`) on Mode Select enters Song Select (`Rookie Music`, `1st.STAGE`, `DISC 01/10`, `EFFECTOR SELECT`, `GEAR SYSTEM`, `X EXIT / O OK`).
  * Pressing `CIRCLE` (`1350 CIRCLE`) on Song Select loads chart/BGA assets (`all.mp` `fd=3`, `all2.mp` `fd=8`), spawns `ScorePlayerThread` (`uid=0x1D`, `prio=34`, `stack=0x8000`, entry `0x0886141C`), and enters the 4-button PGP gameplay screen with scrolling notes over the BGA, ending cleanly at `--max-vblanks 2100` (`Exit code: 0`, `Display: mode=0 480x272 fb=0x04044000 stride=512 fmt=1 vblanks=2100 presented=983 dumped=983`, `GE: 849 lists, 774726 commands, 0 signals, 849 finishes`).

## 2. Root Cause of `Instance.cpp:37` (`assertion "m_referenceCount" failed`) (**Confirmed**)

* **Symptom**: Immediately after pressing `START` on the title screen, guest execution aborted in `__assert("Instance.cpp", 37, "m_referenceCount")`.
* **Root cause in lifter (`third_party/psprecomp/tools/allegrexrecomp/emit.c:791`)**:
  * `last_terminal = in.is_return || in.is_indirect || (in.is_jump && !in.is_call);` treated `jalr` (`in.is_indirect == 1`, `in.is_call == 1`) as a terminal instruction just like `jr`.
  * Consequently, `psp_body_08851930` (ending with `0x0885197C jalr $ra, $v0`) omitted the fall-through call `psp_func_08851984();` to its split epilogue.
  * Returning to `TitleScene::Cleanup` (`0x0880AE40`) left `$sp` and `$fp` 32 bytes too low (`0x09FBF930` instead of `0x09FBF950`), so `0x0880AED0` loaded `0x09D8CFB0` instead of `0x08B97774` from `0($fp)` and passed `0x00000000` into `List::Release(0x08C4D520, 0)` (`0x088485E8`), hitting reset object `0x09B08080` (`m_referenceCount == 0`).
* **Fix (`patches/psprecomp/0003-jalr-fallthrough.patch`)**:
  * Changed the terminal check to `(in.is_indirect && !in.is_call)` and re-lifted `BOOT.BIN` (`8785 functions, 17215 interior entries, 0.00% VFPU traps`), resolving the crash.

## 3. Post-Selection Song Eyecatch (Jacket) Texture Characteristics (**Confirmed**)

* Pressing `CIRCLE` on Song Select zooms the disc, displays the full-screen song eyecatch (jacket) screen, and transitions into gameplay.
* Song assets (read asynchronously via `sceIoOpenAsync` from `all.mp` and `all2.mp`) are staged in User RAM (`0x08000000..0x0A000000`). Large UI/eyecatch textures are placed in User RAM buffers in non-CLUT swizzled formats (`fmt=1` ABGR1555 `512x256` at `0x08C4DA60`, `fmt=2` ABGR4444 `512x256` at `0x08DC1F50` / `256x256`, `fmt=3` ABGR8888 `512x256` at `0x09219110`, all `swz=1`) and drawn by the GE.
* When starting a second song, the guest overwrites the same User RAM texture buffer address/dimensions/format with the new song's eyecatch; if the host GL texture cache fails to hash User RAM ranges, it hits the stale entry and keeps showing the first song's jacket (fixed in `repsp` `gl_backend.cpp` `HashGuestRange`).
* **One-frame `0x08C4DA60` buffer reuse at eyecatch → gameplay transition (observed in reDMP1 runs; whether the original does this is **unconfirmed**)**:
  * On entering the eyecatch, the game places the jacket image at `0x08C4DA60` (`512x256`, `fmt=1`, `swz=1`, `262144` bytes), draws it to `fb=0x04044000`, and presents it via `sceDisplaySetFrameBuf(0x04044000)`.
  * During song loading (~80 vblanks, `all.mp` reads plus guest memory-manager allocations/frees), guest CPU code overwrites `0x08C4DA60` with gameplay UI texture atlas data (not overwritten directly by `sceIoReadAsync` or `sceDmacMemcpy`).
  * Immediately after loading finishes, on the first transition frame (`fb=0x04000000`), the guest clears `(0,0)-(480,272)` (`rgba0=0x00000000`), then draws the overwritten `tex=0x08C4DA60` (`512x256`, `fmt=1`, `rgba0=0xFFFFFFFF`, `tfunc=0`) over `(0,0)-(479,271)`, calls `sceGeDrawSync(0)` → `sceDisplayWaitVblankStartCB()` → `sceDisplaySetFrameBuf(0x04000000)` to display it for one frame, and switches to normal gameplay rendering on the next frame (`fb=0x04044000`). Both `--renderer sw` and `--renderer gl` reproduce this identically. The real hardware and another emulator show no noise frame at this transition, though, so one step above (memory layout, the colour or alpha of the last draw, the choice of displayed buffer, ...) may differ from the original (investigated in [#44](https://github.com/reexec/reDMP1/issues/44)).
