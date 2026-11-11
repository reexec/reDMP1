# Decompiled Functions (src/game/dmp1/decomp/)

이 디렉터리는 기계적으로 재컴파일된 C 코드(`src/game/dmp1/recomp/generated/`)를 사람이 읽고 검증할 수 있는 C/C++ 코드로 점진적으로 대체(decompile)하여 배치하는 곳입니다.

* issue #1 (프로젝트 부트스트랩) 시점에는 아직 대체된 함수가 없습니다.
* 추후 디컴파일 파이프라인(issue #3 예정) 구축 시 함수 단위의 대체 및 동등성 검증 체계가 구성됩니다.

---

# Decompiled Functions (English)

This directory hosts human-readable, verified C/C++ reimplementations of original functions, replacing mechanically lifted C functions from `src/game/dmp1/recomp/generated/` one at a time.

* As of issue #1 (project bootstrap), no functions are decompiled yet.
* A per-function override and equivalence verification pipeline will be established in later decompilation tasks.
