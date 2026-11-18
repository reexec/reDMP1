# 설계: 곡 표지 → 플레이 전환의 노이즈 한 프레임 (issue #44)

## 1. 요구사항

* 사용자 보고(2026-11-18): 곡 표지(아이캐치)에서 플레이로 넘어갈 때 노이즈가 한 프레임(약 33 ms) 나온다. 실기와 다른 에뮬레이터에서는 보이지 않는다.
* #43 조사에서 관측한 것(두 렌더러 모두):
  1. 로딩 중 게스트 코드가 표지 텍스처 버퍼 `0x08C4DA60`(512x256, ABGR1555, swizzle)을 다음 화면 데이터로 덮어쓴다.
  2. 플레이 루프의 첫 프레임이 뒷버퍼를 지운다.
  3. 그 텍스처를 화면 전체에 그린다(`rgba0=0xFFFFFFFF`, `tfunc=0`, blend 켬).
  4. 그 버퍼를 한 vblank 동안 표시한다.
* 원본에는 노이즈가 없으므로, 이 순서 어딘가에서 reDMP1이 하드웨어와 다르다. 그 차이를 찾아 원본처럼 되게 한다.

## 2. 조사 (먼저)

후보마다 근거를 모아 하나씩 배제한다.

1. **마지막 그리기가 원래는 보이지 않아야 하는 경우**: 그 draw가 원래는 아무것도 그리지 않거나 투명하게 그려지는 것일 수 있다. 다음 중 어느 하나가 그 이유일 수 있다.
   * 정점 색·알파
   * 재질 색(색 원천 선택)
   * 텍스처 함수와 알파 사용(`TFUNC`의 RGB/RGBA)
   * 알파 테스트
   * 블렌드 식과 계수
   * 색 마스크, 스텐실, 깊이 테스트, 시저 영역

   그 draw의 GE 상태를 모두 덤프한다. 그리고 공개 문서(PSPSDK `pspgu.h`의 명령 정의, 공개 GE 명령표)와 대조해 우리 해석이 맞는지 확인한다. ABGR1555의 알파 비트 처리도 확인한다.
2. **덮어쓰기가 원래는 다른 곳이어야 하는 경우**: 게임이 압축을 푸는 주소는 우리 HLE의 메모리 할당 결과에 따라 달라질 수 있다(`sceKernelAllocPartitionMemory`, 힙 HLE, 반환 주소와 정렬). 덮어쓴 버퍼가 어떤 할당에서 나왔는지, 그 할당 요청(크기, 종류, 위치 지정)과 우리가 돌려준 주소를 기록한다. 그리고 공개 문서(uOFW)의 할당 규칙과 대조한다.
3. **그 버퍼가 원래는 표시되지 않아야 하는 경우**: `sceDisplaySetFrameBuf`의 동기 모드(즉시인지 다음 vblank인지), 표시 버퍼가 바뀌는 시점, 같은 vblank 안의 두 번째 호출 처리를 확인한다. 그 프레임이 실제로 vblank 경계를 넘어 화면에 남는지, 아니면 다음 SetFrameBuf로 바로 바뀌어야 하는지 본다.
4. **순서가 원래와 다른 경우**: HLE 타이밍 때문에 로딩(비동기 읽기 완료, 콜백, 스레드 우선순위)과 그리기의 순서가 바뀌었을 수 있다. 덮어쓰기가 끝나는 시점과 마지막 draw 사이의 관계를 확인한다.
5. **근거로 쓸 수 있는 자료**: 공개 문서와 블랙박스 비교만 쓴다. PPSSPP 등 GPL 구현 코드는 보지 않는다.

## 3. 수정

* 원인 지점만 고치고, 게임별 분기는 두지 않는다. repsp에서 고친다면 게임과 무관한 동작으로 고친다.
* 원인을 재현하는 합성 probe를 넣는다. probe는 고치기 전에는 실패하고 고친 뒤에는 통과해야 한다. 스레드 타이밍에 기대지 않으며, 실패할 때 창을 띄우지 않고 끝나야 한다.

## 4. 검증

1. 두 곡 연속 실행의 전환 구간 프레임 덤프(sw·gl)에 노이즈 프레임이 없고, 곡마다 표지가 맞다.
2. 플레이 화면까지 3회 실행한다. 미발견 주소 dispatch 0건, 크래시 0건이고, 표시 간격은 이전과 같은 범위다.
3. Win64 Debug/Release(Visual Studio 생성기 포함)와 Linux x64에서 빌드하고 CTest를 돌린다(repsp, reDMP1).
4. 사용자가 전환 장면을 확인한다(사용자 확인).

## 5. 기록

* `docs/analysis/scene-flow.md`: 원인이 확정되면, #43에서 **미확정**으로 둔 항목을 고친다.
* 작업 로그 `docs/work-logs/20261118-i044-transition-noise.md`: 후보별 근거와 배제 이유, 측정, 질문.

---

# Design: one noise frame at the jacket-to-gameplay transition (issue #44)

## 1. Requirements

* **User report (2026-11-18)**: one frame of noise (about 33 ms) at the transition from the song jacket (eyecatch) to gameplay. The real hardware and another emulator show none.
* **Observed in the #43 investigation** (both renderers):
  1. During loading, guest code overwrites the jacket texture buffer `0x08C4DA60` (512x256, ABGR1555, swizzled) with the next screen's data.
  2. The first gameplay-loop frame clears the back buffer.
  3. It draws that texture over the full screen (`rgba0=0xFFFFFFFF`, `tfunc=0`, blend on).
  4. It displays that buffer for one vblank.
* The original shows no noise, so somewhere in this sequence reDMP1 differs from the hardware. Find the difference and match the original.

## 2. Investigation (first)

Collect evidence for each candidate and rule them out one by one.

1. **The last draw should not be visible.** It may originally draw nothing, or draw transparently, because of one of these:
   * vertex colour and alpha;
   * material colour (colour source selection);
   * the texture function and alpha use (`TFUNC` RGB/RGBA);
   * alpha test;
   * blend equation and factors;
   * colour mask, stencil, depth test or scissor.

   Dump the draw's full GE state and check our interpretation against public documents (PSPSDK `pspgu.h` command definitions, public GE command tables). Check the ABGR1555 alpha bit handling as well.
2. **The overwrite should land elsewhere.** The decompression address may depend on our HLE's allocation results (`sceKernelAllocPartitionMemory`, heap HLEs, returned addresses and alignment). Record which allocation the overwritten buffer came from: the request (size, type, placement) and the address we returned. Compare them with the allocation rules in public documents (uOFW).
3. **That buffer should not be displayed.** Check `sceDisplaySetFrameBuf`'s sync mode (immediate or next vblank), when the displayed buffer switches, and how a second call within one vblank is handled. Determine whether that frame really stays on screen across a vblank boundary, or should be replaced by the next SetFrameBuf at once.
4. **The order differs from the original.** HLE timing may reorder loading (async read completion, callbacks, thread priorities) and drawing. Check how the end of the overwrite relates to the last draw.
5. **Allowed evidence**: public documents and black-box comparison only. PPSSPP and other GPL implementation code is not read.

## 3. Fix

* Fix only the point of cause, with no game-specific branches. A fix in repsp must be game-independent behaviour.
* Add a synthetic probe that reproduces the cause. It fails before the fix and passes after, does not depend on thread timing, and exits without a dialog on failure.

## 4. Verification

1. Frame dumps of the transition in a two-song run (sw and gl) show no noise frame, and each jacket is right.
2. Three runs to gameplay: zero undiscovered-address dispatches, zero crashes, and presentation intervals in the same range as before.
3. Build and run CTest on Win64 Debug/Release (including the Visual Studio generator) and Linux x64, for repsp and reDMP1.
4. The user checks the transition (the user's check).

## 5. Record

* `docs/analysis/scene-flow.md`: once the cause is confirmed, correct the item left **unconfirmed** in #43.
* Work log `docs/work-logs/20261118-i044-transition-noise.md`: evidence for and against each candidate, measurements and questions.
