# 분석 문서 색인

원본 디스크와 실행 파일에서 확인한 사실(**확인됨**), 가설(**추정**), 미해결 질문(**미확정**)을 주제별로 둔다.

| 문서 | 내용 |
|---|---|
| [disc-image.md](disc-image.md) | ULKS-46050 원본 UMD의 신원(redump)과 구성 |
| [executable.md](executable.md) | 원본 실행 파일(`BOOT.BIN`/`EBOOT.BIN`) 분석, 세그먼트, NID, 함수 커버리지 |
| [boot.md](boot.md) | 커널·UMD HLE 초기 부팅 흐름, 스레드, 메모리 할당, 첫 비커널 정지 지점 |
| [first-frame.md](first-frame.md) | GE·디스플레이 초기화 흐름, GE 통계, `sceCtrlSetSamplingMode`~`sceMpegInit` 진행 경과 |
| [audio.md](audio.md) | `sceAudio`·`sceSasCore`·`sceAtrac3plus` 초기화, 오디오 스레드 구조 및 스트리밍 경로 |
| [video.md](video.md) | `sceMpeg` 초기화, `opening.pmf` PSMF/PES/AU 구성, 스킵 후 `"NOW LOADING"` 전환 |
| [savedata.md](savedata.md) | `sceUtility` 세이브 데이터(`ULKS46050/SD.BIN`) 분기, `PARAM.SFO` 메타데이터, 메시지 다이얼로그 흐름 |

---

# Analysis index

Findings from the original disc and executable, by topic, marked **confirmed**, **inferred** or **unresolved**.

| Document | Contents |
|---|---|
| [disc-image.md](disc-image.md) | Identity (redump) and layout of the original ULKS-46050 UMD |
| [executable.md](executable.md) | Analysis of original executables (`BOOT.BIN`/`EBOOT.BIN`), segments, NIDs, function coverage |
| [boot.md](boot.md) | Initial boot flow under kernel/UMD HLE, threads, memory allocations, first non-kernel stop |
| [first-frame.md](first-frame.md) | GE/display initialization flow, GE stats, `sceCtrlSetSamplingMode` to `sceMpegInit` progression |
| [audio.md](audio.md) | `sceAudio`, `sceSasCore`, and `sceAtrac3plus` initialization, audio threads, and streaming paths |
| [video.md](video.md) | `sceMpeg` initialization, `opening.pmf` PSMF/PES/AU layout, skip transition to `"NOW LOADING"` |
| [savedata.md](savedata.md) | `sceUtility` save data (`ULKS46050/SD.BIN`) branches, `PARAM.SFO` metadata, message dialog flow |
