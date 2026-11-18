# 작업 지시: 화면 셰이더 (issue #49, repsp #50)

설계: [20261118-i049](../design/20261118-i049-screen-shaders.md)
브랜치: reDMP1 `i049-screen-shaders`(main의 v0.0.24 커밋에서), repsp는 submodule `third_party/repsp`의 `i50-screen-shaders`(main v0.0.23 커밋에서)
참고 구현 사본: `%TEMP%\redmp1-build\borrow\re2DJ\` (re2DJ v0.0.75. 저장소에 복사해 넣지 말고, 필요한 코드를 repsp/reDMP1 구조와 이름으로 옮긴다. 옮긴 코드와 셰이더에는 re2DJ의 BSD 3-Clause 고지를 남긴다)

다음 단위로 나눠 진행하고, 단위마다 커밋한다.

1. **repsp 1단계**: GL 없는 부분. 목록, `#pragma parameter`, `.glslp` 해석, GL 3.3 core용 소스 조립, 내장 셰이더 포함(CMake), 단위 시험(설계 §3.2~3.5)
2. **repsp 2단계**: GL 후처리 pass(`DrawTexture`), 크기 uniform과 `LogicalSize`, 필터, 다단계 타깃, 제어 계약과 `SetInitialPostShader`, 실패 처리, GL probe(설계 §3.1, §3.3~3.7, §5의 3)
3. **reDMP1**: Graphics 탭 Screen shader 항목, `redmp1.ini`의 `post_shader`, `--post-shader`, `REDMP1_POST_SHADER`, 우선순위 시험(설계 §4)
4. **측정**(설계 §5의 1~6)과 **문서**(설계 §6)

지킬 것:
* 생성 코드를 손으로 고치지 않는다.
* repsp에는 게임 정보와 psprecomp 의존을 넣지 않는다.
* PPSSPP 등 GPL 코드는 보지 않는다. libretro glsl-shaders 저장소에서는 MIT인 ScaleFX만 re2DJ에 있는 그대로 쓴다. 다른 libretro 셰이더는 가져오지 않는다.
* 게임 데이터, 생성 코드, 로그 원본, 덤프, 캡처, 스크린샷, 입력 스크립트, 세이브 데이터, `redmp1.ini` 실물은 커밋하지 않는다.
* 사용자 경로는 쓰지 않는다.
* probe는 창을 띄우지 않고(숨은 창의 GL 문맥은 허용), 실패해도 대화상자 없이 끝나며, 스레드 타이밍에 기대지 않는다.
* 커밋 제목 끝에 reDMP1은 `(#49)`, repsp는 `(#50)`을 붙인다.
* push·PR·merge·tag는 하지 않는다.

작업 환경에서 git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 단위별로 무엇을 바꿨는지 보고에 적는다. GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 살핀다.

완료 기준: 설계 §5의 1~6(7은 사용자가 확인). 확인이 필요한 결정은 질문으로 올린다.

---

# Work order: screen shaders (issue #49, repsp #50)

Design: [20261118-i049](../design/20261118-i049-screen-shaders.md)

Branches:
* reDMP1: `i049-screen-shaders`, from the v0.0.24 commit on main.
* repsp: `i50-screen-shaders` in the submodule `third_party/repsp`, from the v0.0.23 commit on main.

Reference copies are in `%TEMP%\redmp1-build\borrow\re2DJ\` (re2DJ v0.0.75). Do not copy them into the repositories; move the code you need into repsp/reDMP1 structure and names. Code and shaders taken from re2DJ keep its BSD 3-Clause notice.

Work in these units, committing each:

1. **repsp, step 1**: the GL-free part (design §3.2-3.5):
   * listing, `#pragma parameter` and `.glslp` parsing;
   * source assembly for GL 3.3 core;
   * embedding the built-in shaders (CMake);
   * unit tests.
2. **repsp, step 2** (design §3.1, §3.3-3.7, §5 item 3):
   * the GL post-processing pass in `DrawTexture`;
   * size uniforms and `LogicalSize`, filtering, multi-pass targets;
   * the control contract and `SetInitialPostShader`;
   * failure handling;
   * the GL probe.
3. **reDMP1** (design §4):
   * the Screen shader item in the Graphics tab;
   * `post_shader` in `redmp1.ini`, `--post-shader` and `REDMP1_POST_SHADER`;
   * precedence tests.
4. **Measurement** (design §5, items 1-6) and **documents** (design §6).

**Must hold**:
* Generated code is never edited by hand.
* No game information or psprecomp dependency in repsp.
* PPSSPP and other GPL code is not read. From the libretro glsl-shaders repository, only ScaleFX (MIT) is used, exactly as re2DJ has it. No other libretro shader is brought in.
* Never commit game data, generated code, raw logs, dumps, captures, screenshots, input scripts, save data, or an actual `redmp1.ini`.
* No user paths.
* Probes open no window (a hidden window's GL context is allowed), exit without a dialog on failure, and do not depend on thread timing.
* Commit titles end in `(#49)` for reDMP1 and `(#50)` for repsp.
* No push, pull request, merge or tag.

If git commits or WSL are blocked, do not work around it. Leave the changes in the working tree and report what changed in each unit. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`).

**Done when**: design §5, items 1-6 pass (item 7 is the user's check). Raise any decision that needs confirming as a question.
