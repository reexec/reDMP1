# 작업 지시: lifter 점검 (issue #16)

설계: [20261115-i016](../design/20261115-i016-lifter-audit.md)
브랜치: `i016-lifter-audit`(main의 v0.0.14 커밋에서). repsp를 바꿀 일이 생기면(게스트 스레드의 호스트 스택 크기 등) 작업 로그에 적고 리뷰어에게 알린다(repsp 이슈는 리뷰어가 만든다).

1. 정적 점검 도구 `tools/lift_audit.py`와 전체 생성 코드의 결과(설계 §2.1).
2. 실행 점검: 진단 빌드의 스택 균형 훅과 플레이 화면까지의 불균형 목록(설계 §2.2).
3. 결함마다 로컬 패치와 재생성, 점검 반복(설계 §2.3).
4. 호스트 스택의 구성별 측정과 §3.2의 결정 또는 질문(설계 §3).
5. 문서: `patches/psprecomp/README.md`(패치별 설명), `docs/guides/running-and-tracing.md`(진단 빌드와 점검 도구 사용법), `docs/analysis/`(lift 결과에 관한 사실이 있으면), 작업 로그 `docs/work-logs/20261115-i016-lifter-audit.md`(분류별 개수, 불일치·불균형마다 판정, 스택 측정표, 질문).
6. 검증: 설계 §4.

개발 환경: 재생성은 `python scripts/recomp/regenerate.py roms`. 저장소 안 `src/game/dmp1/recomp/generated/`에 쓸 수 없으면 `--outdir`로 임시 디렉터리에 만들고 `REDMP1_GENERATED_DIR`로 빌드한다. 입력 스크립트는 `%TEMP%\redmp1-build\play-script.txt`를 쓸 수 있다.

지킬 것: 생성 코드를 손으로 고치지 않는다. psprecomp 원격에는 아무것도 하지 않는다(push, fork, PR, issue 금지). 패치는 이 타이틀에 묶이지 않는 일반적인 수정이어야 한다. 게임 데이터, 생성 코드, 실행 로그 원본, 덤프, 입력 스크립트, 세이브 데이터는 커밋하지 않는다(점검 도구의 출력에서 원본 명령열을 문서에 적을 때는 짧은 인용만). 분석 문서에는 원본 게임 사실만. 문서와 로그에 사용자 계정 이름이 든 절대 경로를 쓰지 않는다. PPSSPP(GPL) 코드는 보지 않는다. 작업 단위마다 커밋, 제목 끝에 `(#16)`. push·PR·merge·tag 금지.

작업 환경에서 git 커밋, WSL 실행, 저장소 안 생성 코드 쓰기가 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 작업 로그와 보고에 그 사실을 적는다. Linux에서도 빌드되도록 GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 미리 살핀다.

완료 기준: 설계 §4의 1~5.

---

# Work order: lifter audit (issue #16)

Design: [20261115-i016](../design/20261115-i016-lifter-audit.md)
Branch: `i016-lifter-audit` (from the v0.0.14 commit on main). If repsp needs a change (the host stack size of guest threads, say), note it in the work log and tell the reviewer (the reviewer opens the repsp issue).

(1) The static checker `tools/lift_audit.py` and its results over all generated code (design §2.1). (2) Run-time check: the stack balance hooks in the diagnostic build and the list of imbalances over the run to gameplay (design §2.2). (3) A local patch per defect, regeneration, and repeated checks (design §2.3). (4) Host stack measurement per configuration and the decision of §3.2, or a question (design §3). (5) Documents: `patches/psprecomp/README.md` (per patch), `docs/guides/running-and-tracing.md` (the diagnostic build and the checker), `docs/analysis/` (if there are facts about the lifted code), the work log `docs/work-logs/20261115-i016-lifter-audit.md` (counts per class, a verdict for each mismatch and imbalance, the stack measurement table, questions). (6) Verify: design §4.

Development environment: regenerate with `python scripts/recomp/regenerate.py roms`; if the in-repository `src/game/dmp1/recomp/generated/` cannot be written, generate into a temporary directory with `--outdir` and build with `REDMP1_GENERATED_DIR`. The input script `%TEMP%\redmp1-build\play-script.txt` can be used.

Must hold: generated code is never edited by hand; nothing is done on psprecomp's remote (no push, fork, pull request or issue); patches must be general fixes not tied to this title; game data, generated code, raw run logs, dumps, input scripts and save data are never committed (original instruction sequences quoted in documents only briefly); analysis documents state facts about the original game only; no absolute path containing a user account name in documents or logs; PPSSPP (GPL) code is not read; commit per unit of work with titles ending in `(#16)`; no push, pull request, merge or tag.

If git commits, WSL, or writes to the in-repository generated code are blocked in the working environment, do not work around it: leave the changes in the working tree and say so in the work log and the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`) so the Linux build passes.

Done when: design §4, items 1-5.
