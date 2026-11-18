# 작업 지시: 전환 노이즈 (issue #44)

설계: [20261118-i044](../design/20261118-i044-transition-noise.md)
브랜치: reDMP1 `i044-transition-noise`(main의 v0.0.22 커밋에서). repsp를 고치게 되면 submodule `third_party/repsp`의 `i44-transition-noise`(main `b3ca15a`에서)에서 작업한다.

1. 조사: 설계 §2의 후보 1~4마다 근거를 모아 하나씩 배제한다. 원인을 찾기 전에는 고치지 않는다.
2. 수정과 probe: 설계 §3.
3. 측정과 검증: 설계 §4의 1~3.
4. 문서: `docs/analysis/scene-flow.md`, 작업 로그.

지킬 것: #43 작업 지시와 같다.
* GPL 코드를 보지 않는다.
* 생성 코드를 손대지 않는다.
* repsp에 게임 정보를 넣지 않는다.
* 덤프·캡처·스크립트를 커밋하지 않는다.
* 사용자 경로를 쓰지 않는다.
* probe가 실패해도 창을 띄우지 않는다.
* push·PR·merge·tag를 하지 않는다.

커밋 제목 끝에 reDMP1은 `(#44)`를 붙인다. repsp 이슈가 필요하면 보고에 적는다.

완료 기준: 설계 §4의 1~3(4는 사용자가 확인). 원인이 원본에도 있는 동작으로 보이거나 큰 구조 변경이 필요하면, 고치지 말고 근거와 함께 질문으로 올린다.

---

# Work order: transition noise (issue #44)

Design: [20261118-i044](../design/20261118-i044-transition-noise.md)
Branches: reDMP1 `i044-transition-noise` (from the v0.0.22 commit on main). If repsp needs fixing, work in `i44-transition-noise` in the submodule `third_party/repsp` (from main `b3ca15a`).

1. **Investigate**: collect evidence for each of candidates 1-4 in design §2 and rule them out one by one. Do not fix before the cause is found.
2. **Fix and probe**: design §3.
3. **Measure and verify**: design §4, items 1-3.
4. **Documents**: `docs/analysis/scene-flow.md` and the work log.

**Must hold** (as in the #43 work order):
* Do not read GPL code.
* Do not touch generated code.
* No game information in repsp.
* Do not commit dumps, captures or scripts.
* No user paths.
* Probe failures exit without a dialog.
* No push, pull request, merge or tag.

Commit titles end in `(#44)` for reDMP1. If a repsp issue is needed, say so in the report.

**Done when**: design §4, items 1-3 (item 4 is the user's check). If the cause looks like behaviour the original also has, or needs a large structural change, do not fix it: raise it as a question with the evidence.
