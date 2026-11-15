# 작업 지시: 크래시 분석 (issue #31)

설계: [20261115-i031](../design/20261115-i031-crash-analysis.md)
브랜치: `i031-crash-analysis`(main의 v0.0.15 커밋에서)
함께: [repsp issue #30](https://github.com/reexec/repsp/issues/30)(크래시 기록)과 [#31](https://github.com/reexec/repsp/issues/31)(probe 멈춤). submodule `third_party/repsp`의 `i030-crash-capture` 브랜치에서 함께 개발한다. reDMP1 커밋에는 `third_party/repsp` 포인터를 넣지 않는다.

1. 앱: 크래시 디렉터리, 실행 로그, 입력 기록(기본 켬), 추가 정보, 게스트 함수 진입 기록(비용 측정 뒤 결정), 개발용 `--debug-crash <kind>`(설계 §2, §5).
2. MSVC Release 빌드의 PDB 생성(설계 §2). `scripts\build_win64.bat`의 산출물에 PDB가 생기는 것을 확인한다.
3. 분석 도구 `tools/analyze_crash.py`(설계 §3). 리뷰어가 쓴 임시 도구의 방식(미니덤프 직접 해석, `dbghelp.dll`로 줄 번호 얻기, 스택의 C++ 예외 기록에서 형식과 오류 값 복원)을 참고하되 새로 쓴다.
4. 첫 사례의 분석 이어 가기(설계 §4): 스케줄러 뮤텍스를 쥔 채 정지가 일어나는 경로를 repsp 쪽 작업과 함께 찾는다. 고치는 방향은 질문으로 올린다.
5. 문서: `docs/guides/`(크래시가 났을 때 사용자가 할 일: 크래시 디렉터리의 위치, 보낼 파일; 분석 도구 사용법), `docs/guides/running-and-tracing.md`(새 옵션), 작업 로그 `docs/work-logs/20261115-i031-crash-analysis.md`.
6. 검증: 설계 §5.

참고: 첫 사례의 덤프는 Windows의 `%LOCALAPPDATA%\CrashDumps\`에 있고, 덤프를 만든 실행 파일은 `build\win64-release-ffmpeg\Release\redmp1.exe`(2026-11-15 20:28 빌드)다. 같은 구성에 디버그 정보를 더한 빌드는 `%TEMP%\claude-review\sym\Release\`에 있다. 덤프와 PDB는 커밋하지 않는다.

지킬 것: 덤프, PDB, 실행 로그 원본, 입력 기록, 세이브 데이터, 게임 데이터, 생성 코드는 커밋하지 않는다. 크래시 보고와 로그에 사용자 경로를 넣지 않는다(크래시 디렉터리 이름 정도는 된다). 분석 문서에는 원본 게임 사실만. 문서와 로그에 사용자 계정 이름이 든 절대 경로를 쓰지 않는다. PPSSPP(GPL) 코드는 보지 않는다. 작업 단위마다 커밋, 제목 끝에 `(#31)`. push·PR·merge·tag 금지. psprecomp 원격에는 아무것도 하지 않는다.

작업 환경에서 git 커밋이나 WSL 실행이 막히면 우회하지 말고, 변경을 작업 트리에 둔 채 작업 로그와 보고에 그 사실을 적는다. Linux에서도 빌드되도록 GCC 경고(`-Wall -Wextra -Wpedantic -Werror`)를 미리 살핀다.

완료 기준: 설계 §5의 1~5.

---

# Work order: crash analysis (issue #31)

Design: [20261115-i031](../design/20261115-i031-crash-analysis.md)
Branch: `i031-crash-analysis` (from the v0.0.15 commit on main)
Alongside: [repsp issue #30](https://github.com/reexec/repsp/issues/30) (crash capture) and [#31](https://github.com/reexec/repsp/issues/31) (probe hang), developed on the `i030-crash-capture` branch of the `third_party/repsp` submodule; reDMP1 commits never include the `third_party/repsp` pointer.

(1) App: the crash directory, run log, input recording (on by default), extra information, guest function entry recording (decided after measuring its cost), and the development option `--debug-crash <kind>` (design §2, §5). (2) PDBs for MSVC Release builds (design §2); confirm `scripts\build_win64.bat` output includes one. (3) The analysis tool `tools/analyze_crash.py` (design §3), written anew while following the approach of the reviewer's temporary tooling (reading the minidump directly, line numbers through `dbghelp.dll`, recovering the type and error value from the C++ exception record on the stack). (4) Continue the analysis of the first case (design §4): find, together with the repsp work, the path where a stop happens with the scheduler mutex held; how to fix it is raised as a question. (5) Documents: `docs/guides/` (what the user does after a crash: where the crash directory is and which files to send; how to use the analysis tool), `docs/guides/running-and-tracing.md` (new options), the work log `docs/work-logs/20261115-i031-crash-analysis.md`. (6) Verify: design §5.

For reference: the first case's dump is in Windows' `%LOCALAPPDATA%\CrashDumps\`, made by `build\win64-release-ffmpeg\Release\redmp1.exe` (built 2026-11-15 20:28); a build of the same configuration with debug information is in `%TEMP%\claude-review\sym\Release\`. Dumps and PDBs are never committed.

Must hold: dumps, PDBs, raw run logs, input recordings, save data, game data and generated code are never committed; crash reports and logs carry no user paths (the crash directory's own name is fine); analysis documents state facts about the original game only; no absolute path containing a user account name in documents or logs; PPSSPP (GPL) code is not read; commit per unit of work with titles ending in `(#31)`; no push, pull request, merge or tag; nothing is done on psprecomp's remote.

If git commits or WSL are blocked in the working environment, do not work around it: leave the changes in the working tree and say so in the work log and the report. Watch for GCC warnings (`-Wall -Wextra -Wpedantic -Werror`) so the Linux build passes.

Done when: design §5, items 1-5.
