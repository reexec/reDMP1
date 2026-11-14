# 설계: 소리 결함 — 모드 선택의 반복 효과음, 느리게 나는 소리 (issue #26)

## 1. 요구사항

* 코덱 플러그인을 켠 창 모드에서 사용자가 귀로 확인한 결함이다(2026-11-15).
  * 오프닝 동영상의 소리는 정상이다.
  * 그 밖의 소리는 괜찮은 것과 아닌 것이 섞여 있다.
  * 모드 선택 화면에서 효과음이 계속 반복된다.
  * 느리게 재생되는 소리가 있는 것 같다.
* 어느 소리가 어느 경로로 나는지 가려, 원인을 증거로 확정하고 원인이 있는 곳에서 고친다. repsp 쪽은 [repsp issue #26](https://github.com/reexec/repsp/issues/26)이다.

## 2. 조사 방법

1. **소리의 목록 만들기**: 타이틀, 모드 선택, 곡 선택, 플레이 구간에서 나는 소리를 경로별로 나눈다. repsp의 진단 기능(채널별 WAV, SAS 음성 추적, ATRAC 추적)을 켜고 플레이 화면까지의 입력 스크립트를 실행한다. 소리마다 경로(SAS 음성 / ATRAC 스트림), 원본 형식(VAG의 크기·블록 플래그·pitch·loop 인자, ATRAC의 코덱·채널·표본화 주파수·루프 지점), 출력 채널을 표로 만든다.
2. **반복되는 효과음**: 모드 선택 구간의 SAS 추적에서 반복해 나는 음성을 찾는다. (a) 타이틀이 반복해서 key on을 거는 것인지(그렇다면 왜: 끝 보고, 입력 판정, 스케줄링), (b) 한 번 건 음성이 HLE 안에서 되돌아가 반복되는 것인지(loop 인자와 블록 플래그)를 가린다.
3. **느린 소리**: 소리마다 "원본 형식으로 계산한 재생 시간"과 "실제로 출력된 시간"을 비교한다. ATRAC 스트림은 같은 RIFF 데이터를 FFmpeg 명령줄 도구로 디코딩한 결과(길이, 채널, 표본화 주파수)와 비교한다. 비율이 2배, 44100/48000, 44100/22050 가운데 어느 것에 가까운지로 원인 후보를 좁힌다.
4. **가설 가르기와 확정**: repsp 설계 §2의 후보를 증거로 지우거나 확정한다. 게스트 쪽의 이상이 의심되면 lift 결과를 먼저 본다(issue #18의 교훈).

## 3. 고치는 위치

* repsp의 결함은 submodule 브랜치 `i026-sound-defects`에서 고치고 probe를 더한다.
* lifter의 결함은 `patches/psprecomp/`의 로컬 패치로 고친다(upstream에는 아무것도 하지 않는다).
* 설계 판단이 필요한 원인(스케줄러 의미, 소비자 계약)은 고치지 말고 근거와 선택지를 작업 로그의 질문으로 올린다.

## 4. 기록

* `docs/analysis/audio.md`에 이어서: 소리의 목록(화면, 경로, 원본 형식, 출력 채널), 효과음을 거는 코드 경로와 끝을 확인하는 방식, 배경 음악의 형식과 루프.
* 원본 게임의 사실만 적는다. 추출한 소리, 덤프, 사용자 계정 이름이 든 경로는 저장소에 넣지 않는다.

## 5. 이번 task의 검증

1. 결함마다 증거(추적의 해당 줄, 시간 비교 수치)와 한 문장의 원인이 작업 로그에 있다.
2. 고친 뒤: 모드 선택 구간에서 같은 음성이 입력 없이 반복되지 않는다(SAS 추적과 SAS 출력 WAV의 구간별 에너지로 확인). 느렸던 소리의 출력 시간이 계산값(또는 기준 디코딩)과 2% 안에서 맞는다.
3. 플레이 화면까지의 입력 스크립트 실행이 같은 화면에 이른다. 3회 실행을 비교한다.
4. Win64 Debug/Release와 Linux x64 빌드·CTest, CI 통과(플러그인 없는 구성). 플러그인 구성의 CTest도 Win64에서 통과한다.
5. 최종 확인은 사용자가 창 모드로 듣는다. 확인할 화면과 소리를 작업 로그에 적는다.

---

# Design: sound defects — the repeating effect on mode select, sounds that play slowly (issue #26)

## 1. Requirements

Defects the user confirmed by ear in windowed mode with the codec plugin (2026-11-15): the opening movie's audio is right; other sounds are a mix of right and wrong; on the mode select screen a sound effect keeps repeating; some sounds seem to play slowly. Work out which sound goes through which path, confirm the causes by evidence and fix them where they lie. The repsp side is [repsp issue #26](https://github.com/reexec/repsp/issues/26).

## 2. Method

(1) **Inventory of sounds**: the sounds heard on the title, mode select, song select and gameplay stretches are sorted by path. With repsp's diagnostics on (per-channel WAVs, the SAS voice trace, the ATRAC trace) the scripted run to the gameplay screen is executed, and for each sound a table records its path (SAS voice or ATRAC stream), source format (for VAG: size, block flags, pitch, loop argument; for ATRAC: codec, channels, sample rate, loop points) and output channel. (2) **The repeating effect**: in the SAS trace of the mode select stretch, find the voice that keeps sounding and decide whether (a) the title keeps keying it on (and then why: end reporting, input detection, scheduling) or (b) a voice keyed on once loops back inside the HLE (loop argument and block flags). (3) **Slow sounds**: for each sound compare the playing time computed from its source format with the time actually output; ATRAC streams are compared with the same RIFF data decoded by the FFmpeg command-line tool (length, channels, sample rate). Whether the ratio is near 2, 44100/48000 or 44100/22050 narrows the candidates. (4) **Separating hypotheses and confirming**: the candidates in §2 of the repsp design are ruled out or confirmed by evidence; if a guest-side fault is suspected, the lifted code is examined first (the lesson of issue #18).

## 3. Where to fix

A repsp defect is fixed on the submodule branch `i026-sound-defects` with a probe. A lifter defect is fixed as a local patch under `patches/psprecomp/` (nothing is done upstream). A cause that needs a design decision (scheduler semantics, a consumer contract) is not fixed but raised as a question in the work log with evidence and options.

## 4. Record

Continuing `docs/analysis/audio.md`: the inventory of sounds (screen, path, source format, output channel), the code path that triggers sound effects and how it checks for their end, and the format and looping of background music. Facts about the original game only; extracted sounds, dumps and paths containing a user account name stay out of the repository.

## 5. Verification for this task

1. For each defect the work log holds the evidence (the trace lines, the timing figures) and a one-sentence cause.
2. After the fixes: on the mode select stretch no voice repeats without input (checked in the SAS trace and by per-interval energy of the SAS output WAV), and the output time of the sounds that were slow matches the computed value (or the reference decode) within 2%.
3. The scripted run to the gameplay screen reaches the same screens; three runs are compared.
4. Win64 Debug/Release and Linux x64 builds and CTest, and CI, pass (the configuration without the plugin); CTest in the plugin configuration passes on Win64.
5. The final check is the user listening in windowed mode; the work log lists the screens and sounds to check.
