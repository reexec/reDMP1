# 작업 지시: 스택 안전망 정리 (issue #39, repsp #37)

설계: [20261117-i039](../design/20261117-i039-stack-safety-nets.md)
브랜치: reDMP1 `i039-stack-safety-nets`(main의 v0.0.19 커밋에서), repsp는 submodule `third_party/repsp` 작업 트리에서(main `65b0744`에서)

1. repsp: 기본 예약 64 MiB → 8 MiB, 근거를 문서와 주석에(게임 측정값은 쓰지 않는다), probe가 옛 기본값을 단정하면 고침(설계 §2, §3의 4).
2. reDMP1: Debug에서 생성 코드 강제 최적화 제거, `REDMP1_RECOMP_UNOPT` 제거와 언급 정리(설계 §2).
3. 측정: Debug 스택표, Release 3회 실행, Debug 속도, `--debug-crash` 다섯 종류, 스택 넘침이 정지 보고로 잡히는지(설계 §3의 1~4).
4. 문서: 작업 로그 `docs/work-logs/20261117-i039-stack-safety-nets.md`, repsp 쪽 근거 문서, 빌드 안내.
5. 검증: 설계 §3의 1~5.

지킬 것: 생성 코드를 손으로 고치지 않는다. repsp에 psprecomp 의존을 만들지 않는다. repsp 문서에 특정 게임의 측정값이나 사용자의 파일 정보를 쓰지 않는다. 게임 데이터, 생성 코드, 실행 로그 원본, 덤프, 입력 스크립트, 세이브 데이터는 커밋하지 않는다. 문서와 로그에 사용자 계정 이름이 든 절대 경로를 쓰지 않는다. PPSSPP(GPL) 코드는 보지 않는다. 작업 단위마다 커밋, 제목 끝에 reDMP1은 `(#39)`, repsp는 `(#37)`. push·PR·merge·tag 금지.

작업 환경에서 git 커밋이나 WSL이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 작업 로그와 보고에 적는다. Linux에서도 빌드되도록 GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 미리 살피고, Visual Studio 생성기(`scripts\build_win64.bat`)로도 Debug/Release를 빌드한다.

완료 기준: 설계 §3의 1~5(6은 사용자가 확인).

---

# Work order: reducing the stack safety nets (issue #39, repsp #37)

Design: [20261117-i039](../design/20261117-i039-stack-safety-nets.md)
Branches: reDMP1 `i039-stack-safety-nets` (from the v0.0.19 commit on main); repsp in the submodule working tree `third_party/repsp` (from main `65b0744`).

1. **repsp**: lower the default reservation from 64 MiB to 8 MiB, with the reasoning in documents and comments (no game measurements). If a probe asserts the old default, fix it (design §2, §3 item 4).
2. **reDMP1**: drop the forced optimization of the generated code in Debug, remove `REDMP1_RECOMP_UNOPT`, and clean up the places that mention it (design §2).
3. **Measure**:
   - the Debug stack table;
   - three Release runs;
   - Debug speed;
   - the five `--debug-crash` kinds;
   - that a stack overflow is still caught as a stop report.
   
   (design §3, items 1-4)
4. **Documents**: the work log `docs/work-logs/20261117-i039-stack-safety-nets.md`, the reasoning document on the repsp side, and the build guide.
5. **Verify**: design §3, items 1-5.

**Must hold**:
- Generated code is never edited by hand.
- No psprecomp dependency in repsp.
- No particular game's measurements and no information about the user's files in repsp documents.
- Game data, generated code, raw run logs, dumps, input scripts and save data are never committed.
- No absolute path containing a user account name in documents or logs.
- PPSSPP (GPL) code is not read.
- Commit per unit of work, with titles ending in `(#39)` for reDMP1 and `(#37)` for repsp.
- No push, pull request, merge or tag.

If git commits or WSL are blocked in the working environment, do not work around it: leave the changes in the working tree and say so in the work log and the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`) so the Linux build passes, and also build Debug/Release with the Visual Studio generator (`scripts\build_win64.bat`).

**Done when**: design §3, items 1-5 (item 6 is the user's check).
