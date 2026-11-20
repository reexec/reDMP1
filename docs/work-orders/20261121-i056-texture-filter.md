# 작업 지시: 텍스처 필터링 (issue #56, repsp #59)

설계: [20261121-i056-texture-filter](../design/20261121-i056-texture-filter.md)
브랜치: reDMP1 `i056-texture-filter`(main의 v0.0.27 커밋에서), repsp는 submodule `third_party/repsp`의 `i59-texture-filter`(main v0.0.26 커밋에서). 둘 다 만들어 두었다.

다음 단위로 나눠 진행하고, 단위마다 커밋한다.

1. **repsp GE 상태**(설계 §2.1)와 `ge_probe`
2. **repsp GL 하드웨어 샘플링**(설계 §2.2): `texture()`, 샘플러, 필터·감싸기, 레벨 0 크기 업로드, render-to-texture bilinear
3. **repsp 밉맵**(설계 §2.2): 게임 레벨 업로드, 생성 밉맵(이방성), TEXLEVEL, 텍스처 캐시
4. **repsp 소프트웨어 렌더러 bilinear**(설계 §2.3)
5. **probe**(설계 §3의 1)
6. **reDMP1**(설계 §2.4)
7. **측정**(설계 §3의 2~4)과 **문서**(설계 §4). 프레임 덤프는 `REPSP_FRAME_DUMP`를 쓰고 커밋하지 않는다. 입력 스크립트는 `%TEMP%\redmp1-build\i041-keysound-script.txt`.

지킬 것:
* 생성 코드를 손으로 고치지 않는다.
* repsp에는 게임 정보와 psprecomp 의존을 넣지 않는다.
* PPSSPP 등 GPL 코드는 보지 않는다. GE 레지스터 배치는 공개 문서를 따른다.
* 게임 데이터, 생성 코드, 로그 원본, 덤프, 캡처, 입력 스크립트, 세이브 데이터, `redmp1.ini` 실물은 커밋하지 않는다.
* 사용자 경로는 쓰지 않는다.
* probe는 창을 띄우지 않고(숨은 창의 GL 문맥은 허용), 실패해도 대화상자 없이 끝나며, 스레드 타이밍에 기대지 않는다.
* 커밋 제목 끝에 reDMP1은 `(#56)`, repsp는 `(#59)`를 붙인다.
* push·PR·merge·tag는 하지 않는다.

git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 단위별로 무엇을 바꿨는지 보고에 적는다. GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 살핀다.

완료 기준: 설계 §3의 1~4(5는 사용자가 확인). 확인이 필요한 결정(예: 확실하지 않은 GE 비트, 기울기 모드 근사)은 질문으로 올린다.

---

# Work order: texture filtering (issue #56, repsp #59)

Design: [20261121-i056-texture-filter](../design/20261121-i056-texture-filter.md)

Branches (both already created):
* reDMP1: `i056-texture-filter`, from the v0.0.27 commit on main.
* repsp: `i59-texture-filter` in the submodule `third_party/repsp`, from the v0.0.26 commit on main.

Work in these units, committing each:

1. **repsp GE state** (design §2.1) and `ge_probe`.
2. **repsp GL hardware sampling** (design §2.2): `texture()`, samplers, filter and wrap, level-0-size uploads, and bilinear render-to-texture.
3. **repsp mipmaps** (design §2.2): game-supplied levels, generated mipmaps (anisotropic), TEXLEVEL, and the texture cache.
4. **repsp software renderer bilinear** (design §2.3).
5. **Probes** (design §3 item 1).
6. **reDMP1** (design §2.4).
7. **Measurement** (design §3 items 2-4) and **documents** (design §4). Take frame dumps with `REPSP_FRAME_DUMP` and do not commit them. Use the input script at `%TEMP%\redmp1-build\i041-keysound-script.txt`.

**Must hold**:
* Generated code is never edited by hand.
* No game information or psprecomp dependency in repsp.
* PPSSPP and other GPL code is not read. GE register layouts follow public documentation.
* Never commit game data, generated code, raw logs, dumps, captures, input scripts, save data, or an actual `redmp1.ini`.
* No user paths.
* Probes open no window (a hidden window's GL context is allowed), exit without a dialog on failure, and do not depend on thread timing.
* Commit titles end in `(#56)` for reDMP1 and `(#59)` for repsp.
* No push, pull request, merge or tag.

If git commits or WSL are blocked, do not work around it. Leave the changes in the working tree and report what changed in each unit. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`).

**Done when**: design §3 items 1-4 pass (item 5 is the user's check). Raise any decision that needs confirming as a question, such as an uncertain GE bit or the slope-mode approximation.
