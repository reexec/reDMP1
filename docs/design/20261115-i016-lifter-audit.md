# 설계: lifter 점검 — 본문 끝 판정의 전수 점검과 호스트 스택 (issue #16)

## 1. 요구사항

* issue #18에서 lifter(`allegrexrecomp`)의 본문 끝 판정 결함(`jalr`을 `jr`처럼 끝으로 취급)이 게스트 assertion으로 드러났다. 같은 종류의 결함이 더 있는지 **체계적으로** 찾는다. 증상이 나타날 때까지 기다리지 않는다.
* issue #16: 함수 경계를 넘는 전이는 `callee(); return;`(꼬리 위치의 호출)로 옮겨진다. 컴파일러가 이것을 점프로 바꾸지 않으면(MSVC `/Od`) 게스트에서 스택을 쓰지 않는 루프가 호스트 스택을 쌓는다. v0.0.7은 MSVC 빌드에 64 MiB 스택 예약(`/STACK`)으로 우회했다. 원인을 측정하고, 우회를 없앨 수 있는지 정한다.
* 고치는 곳은 `patches/psprecomp/`의 로컬 패치다. psprecomp 원격(upstream)에는 아무것도 하지 않는다(사용자가 허락할 때만). 생성 코드를 손으로 고치지 않는다.

## 2. 본문 끝 판정의 전수 점검

### 2.1 정적 점검 도구

* `tools/lift_audit.py`(표준 라이브러리만): 원본 실행 파일(재배치한 이미지)과 생성 코드를 함께 읽어, 생성된 본문마다 **마지막 명령의 종류와 생성 코드의 끝 모양이 맞는지** 대조한다.
* 분류할 전이(Allegrex): `jr $ra`(반환), `jr <reg>`(점프 표·간접 점프), `j`(함수 진입 / 함수 내부 / 발견되지 않은 곳), `jal`·`jalr`(호출 뒤 다음 명령으로 이어짐), 조건 분기와 likely 분기(`beql` 등: 지연 슬롯이 taken 경로에서만 실행), `bgezal`류(조건부 호출), `syscall`·`break`, 그리고 분기 없이 다음 함수로 흘러드는 것(fall-out).
* 대조 규칙 예: 
  * 호출(`jal`, `jalr`, 조건부 호출)로 끝나는 본문은 다음 명령으로 이어지는 전이(fall-through 호출)가 있어야 한다(issue #18의 결함).
  * `jr <reg>`(`$ra` 아님)는 점프 표 해석이 있으면 그 표의 모든 대상이, 없으면 `psp_dispatch`가 있어야 한다.
  * 지연 슬롯이 다른 본문의 소유일 때 taken 경로에 그 사본이 있어야 한다(upstream이 이미 다루는 경우의 확인).
  * likely 분기의 지연 슬롯은 not-taken 경로에서 실행되지 않아야 한다.
  * fall-out은 다음 함수로의 꼬리 호출이 있어야 한다.
* 결과는 분류별 개수와 불일치 목록(주소, 원본 명령, 생성 코드의 끝 몇 줄)이다. 불일치마다 결함인지 도구의 오판인지 가린다.
* 도구의 오판을 줄이기 위해, 생성 코드의 형식은 lifter의 출력 규칙(`emit.c`)에서 읽어 맞춘다. psprecomp는 MIT이므로 읽어도 된다.

### 2.2 실행 점검(스택 균형)

* lifter에는 이미 스택 균형 검사(`PSP_SP_CALL`, `PSP_SP_LEAVE`, `PSP_SP_CHECK` → `psp_trace_sp*`)가 있다. 지금 `src/recomp_abi/trace.c`에서는 이 훅이 비어 있다.
* 진단 빌드(`REDMP1_TRACE_GUEST=ON`)에서 이 훅을 구현한다: 불균형이 생기면 함수 주소, 호출 지점, 들어갈 때와 나올 때의 `$sp`를 한 번씩 기록하고 개수를 센다(같은 지점은 한 번만 찍는다). 꺼진 빌드에는 비용이 없다.
* 오프닝부터 플레이 화면까지의 입력 스크립트로 실행해 불균형 목록을 얻는다. 불균형은 lift 결함의 강한 신호다(issue #18의 결함은 32바이트 불균형이었다). 목록의 항목마다 원본 명령열과 생성 코드를 대조해 원인을 가린다.

### 2.3 고치기

* 결함이 확인되면 로컬 패치로 고친다(패치 하나에 결함 하나). 패치마다 `patches/psprecomp/README.md`에 원본 명령열, 잘못 옮겨진 방식, 고친 방식을 적는다. 다시 생성한 코드로 정적 점검과 실행 점검을 되풀이해 불일치·불균형이 줄었는지 확인한다.
* 패치 0001~0003과 충돌하지 않게 순서를 지킨다. upstream에 낼 수 있게 각 패치는 이 타이틀에 묶이지 않는 일반적인 수정이어야 한다(제출은 사용자가 허락할 때).

## 3. 호스트 스택(issue #16)

### 3.1 측정

* 게스트 스레드는 호스트 스레드다. Windows에서는 `CreateThread(…, 0, …)`로 만들어 실행 파일의 기본 예약 크기(지금 `/STACK`의 64 MiB)를 쓰고, Linux에서는 pthread 기본(보통 8 MiB)을 쓴다.
* 진단 빌드에서 게스트 스레드마다 호스트 스택의 최고 사용량(high-water)을 잰다(스레드 시작 때 스택 위치를 기록하고, 꼬리 호출 지점이나 HLE 진입 때 현재 위치와 비교). 구성별(MSVC Debug/Release, GCC Debug/Release)로 오프닝·플레이 구간의 최고값과, 최고값을 만든 호출 사슬(가장 깊을 때의 반복 함수 쌍)을 기록한다.

### 3.2 선택지(측정 뒤 정한다)

* **A. 생성 코드만 최적화해 빌드**: `redmp1_recomp` 타깃을 Debug 구성에서도 최적화(MSVC `/O2`, GCC `-O2`)로 컴파일한다. 컴파일러가 꼬리 호출을 점프로 바꾸면 스택이 자라지 않는다. 생성 코드를 디버거로 단계 실행하는 일은 드물다. 단, MSVC는 꼬리 호출 최적화를 보장하지 않으므로 측정으로 확인해야 한다.
* **B. 트램펄린**: 본문을 끝내는 전이를 "다음에 실행할 함수"를 돌려주는 방식으로 바꾸고 바깥 루프가 부른다. 컴파일러와 무관하게 스택이 자라지 않지만, 모든 생성 함수의 형태와 호출 규약이 바뀌는 큰 변경이고 lifter·런타임 계약에 닿는다. 사용자 결정이 필요하다.
* **C. 지금처럼 큰 예약을 유지**: 가장 단순하다. 64비트에서는 예약만 하고 실제로 쓰는 만큼만 메모리를 차지한다. Linux의 게스트 스레드에도 같은 크기를 명시해야 한다.
* 측정에서 A로 모든 구성의 최고값이 작아지면(예: 1 MiB 이하) A를 적용하고 `/STACK` 예약을 없앤다. 그렇지 않으면 측정 결과와 B·C의 비교를 질문으로 올린다.

## 4. 이번 task의 검증

1. 정적 점검 도구가 전체 생성 코드에 대해 분류별 개수와 불일치 목록을 낸다. 불일치마다 "결함 → 패치 번호" 또는 "오판 → 이유"가 작업 로그에 있다.
2. 실행 점검: 플레이 화면까지의 실행에서 스택 불균형 목록을 얻고, 항목마다 원인이 기록돼 있다. 고친 뒤 불균형이 0이거나 남은 것의 이유가 있다.
3. 호스트 스택의 구성별 최고값이 기록돼 있고, §3.2의 결정(또는 질문)이 있다.
4. 새 패치가 있으면 재생성한 코드로 오프닝부터 플레이 화면까지의 입력 스크립트 실행이 같은 화면에 이른다(3회).
5. Win64 Debug/Release와 Linux x64 빌드·CTest, CI 통과. 게임 데이터 없는 구성도 경고 없이 빌드된다.

---

# Design: lifter audit — a full check of end-of-body decisions, and the host stack (issue #16)

## 1. Requirements

In issue #18 a defect in the lifter's (`allegrexrecomp`) end-of-body decision (`jalr` treated as an end like `jr`) surfaced as a guest assertion. Look for defects of the same kind **systematically**, without waiting for symptoms. Issue #16: transfers that cross a function boundary are lifted as `callee(); return;` (a call in tail position); when the compiler does not turn it into a jump (MSVC `/Od`), a guest loop that uses no stack grows the host stack, and v0.0.7 worked around it with a 64 MiB stack reservation (`/STACK`) in the MSVC build — measure the cause and decide whether the workaround can go. Fixes go into local patches under `patches/psprecomp/`; nothing is done on psprecomp's remote (upstream) unless the user allows it; generated code is never edited by hand.

## 2. Full check of end-of-body decisions

**Static checker**: `tools/lift_audit.py` (standard library only) reads the original executable (the relocated image) together with the generated code and checks, for every generated body, that **the kind of its last instruction matches the shape of the generated code's ending**. Transfers classified (Allegrex): `jr $ra` (return), `jr <reg>` (jump tables, indirect jumps), `j` (to a function entry, inside the function, to somewhere undiscovered), `jal`/`jalr` (a call continuing at the next instruction), conditional and likely branches (`beql` and the like, whose delay slot runs only on the taken path), `bgezal` and the like (conditional calls), `syscall`/`break`, and running into the next function without a branch (fall-out). Example rules: a body ending in a call (`jal`, `jalr`, a conditional call) must have the transfer that continues at the next instruction (the fall-through call; the defect of issue #18); `jr <reg>` other than `$ra` needs every target of its jump table when the table was resolved, or `psp_dispatch` when it was not; when a delay slot is owned by another body, the taken path must carry a copy (confirming a case upstream already handles); a likely branch's delay slot must not run on the not-taken path; a fall-out must end in a tail call to the next function. The output is a count per class and a list of mismatches (address, original instruction, last lines of the generated code); each mismatch is classified as a defect or a false report of the tool. To keep false reports down, the tool follows the lifter's output rules (`emit.c`); psprecomp is MIT, so reading it is fine.

**Run-time check (stack balance)**: the lifter already has stack balance checks (`PSP_SP_CALL`, `PSP_SP_LEAVE`, `PSP_SP_CHECK` calling `psp_trace_sp*`), whose hooks are empty in `src/recomp_abi/trace.c` today. In the diagnostic build (`REDMP1_TRACE_GUEST=ON`) these hooks are implemented: an imbalance logs the function address, the call site and `$sp` on entry and exit once per site, and is counted; the build without diagnostics pays nothing. The scripted run from the opening to gameplay yields the list of imbalances, a strong signal of lifting defects (the defect of issue #18 was a 32-byte imbalance), and each entry is traced to its cause by comparing the original instruction sequence with the generated code.

**Fixing**: a confirmed defect is fixed by a local patch, one defect per patch, each described in `patches/psprecomp/README.md` with the original instruction sequence, how it was mislifted and how it is fixed; the code is regenerated and the static and run-time checks repeated to confirm that mismatches and imbalances went down. Patches are ordered so as not to conflict with 0001-0003, and each must be a general fix not tied to this title, so it can go upstream (when the user allows).

## 3. Host stack (issue #16)

**Measurement**: guest threads are host threads — on Windows created with `CreateThread(…, 0, …)` and so using the executable's default reservation (the 64 MiB of `/STACK` now), on Linux using the pthread default (usually 8 MiB). In the diagnostic build each guest thread's host stack high-water mark is measured (the stack position recorded at thread start and compared with the current position at tail-call sites or HLE entry). For each configuration (MSVC Debug/Release, GCC Debug/Release) the high-water over the opening and gameplay stretches is recorded, with the call chain that produced it (the pair of functions repeating at the deepest point).

**Options (decided after measuring)**: **A. Optimize the generated code only** — compile the `redmp1_recomp` target optimized (MSVC `/O2`, GCC `-O2`) even in Debug; if the compiler turns tail calls into jumps the stack does not grow, and stepping through generated code in a debugger is rare; MSVC does not guarantee tail-call optimization, so this must be confirmed by measurement. **B. A trampoline** — transfers that end a body return "the function to run next" and an outer loop calls it; the stack does not grow whatever the compiler does, but the form and calling convention of every generated function change, a large change touching the lifter and runtime contracts that needs the user's decision. **C. Keep a large reservation as now** — the simplest; on 64-bit only what is used is committed; Linux guest threads would need the same size stated explicitly. If the measurements show A brings every configuration's high-water down (to 1 MiB or less, say), A is applied and the `/STACK` reservation removed; otherwise the measurements and a comparison of B and C are raised as a question.

## 4. Verification for this task

1. The static checker reports per-class counts and a mismatch list for all generated code; for each mismatch the work log says "defect → patch number" or "false report → why".
2. Run-time check: the list of stack imbalances over the run to gameplay is obtained and each entry's cause recorded; after the fixes imbalances are zero or the remaining ones are explained.
3. Host stack high-water per configuration is recorded, with the decision of §3.2 (or a question).
4. If new patches exist, the scripted run from the opening to gameplay on regenerated code reaches the same screens (three runs).
5. Win64 Debug/Release and Linux x64 builds and CTest, and CI, pass; the configuration without game data builds without warnings.
