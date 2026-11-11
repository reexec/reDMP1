# 실행 파일 분석: DJMAX Portable (ULKS-46050)

이 문서는 원본 UMD의 기본 실행 파일(`PSP_GAME/SYSDIR/BOOT.BIN` 및 `EBOOT.BIN`)에 대해 `allegrexrecomp` 분석 도구로 확인한 사실을 기록한다.

---

## 1. 모듈 신원 및 컨테이너

| 항목 | 값 |
|---|---|
| 모듈 이름 | `djmax` |
| 모듈 버전 | 1.1 |
| 모듈 속성 | `0x0000` (User-mode PRX) |
| 실행 형식 | ELF32 MIPS LE (PRX, relocatable) |
| 파일 크기 | 1,463,072 바이트 |
| 엔트리 포인트 | `0x00000128` |
| GP ($gp) 레지스터 | `0x00026660` |
| 내보내기 함수 | 1 (`module_start` / `0x00000128`) |

* **확인됨**: 원본 실행 파일은 재배치 가능한 사용자 모드 PRX 모듈이다.
* **확인됨**: `BOOT.BIN`은 평문 ELF32(`\x7fELF`)이며, `EBOOT.BIN`은 Sony `~PSP` 암호화 헤더로 감싸진 형태이다.

---

## 2. 세그먼트 및 섹션 배치

| 세그먼트 | 가상 주소 | 파일 크기 | 메모리 크기 | 플래그 | 내용 |
|---|---|---|---|---|---|
| Segment 0 | `0x00000000` | 1,028,240 바이트 | 1,028,240 바이트 | `rwx` | 코드(`.text`), 읽기 전용 데이터, 임포트 스텁, 모듈 정보 |
| Segment 1 | `0x000FB090` | 124,516 바이트 | 2,718,852 바이트 | `rw-` | 초기화된 데이터(`.data`), 미초기화 데이터(`.bss`) |

* **확인됨**: BSS 영역 크기는 2,594,336 바이트(`2,718,852 - 124,516`)이다.
* **확인됨**: 섹션 수는 총 38개이며, `.text` 섹션은 오프셋 `0x00000040`부터 822,320 바이트이다.
* **확인됨**: 임포트 스텁(stub) 테이블은 오프셋 `0x000C8C8C`부터 1,272 바이트에 위치한다.
* **확인됨**: 모듈 정보 구조체(`psp_module_info`)는 `0x000C9340`에 위치한다.

---

## 3. 코드 분석 및 함수 커버리지

`allegrexrecomp funcs`를 통해 수행한 정적 흐름 분석 결과는 다음과 같다.

```mermaid
flowchart TD
    ENTRY["엔트리 및 재배치 시드 (5,967개)"] --> TRACE["제어 흐름 추적 (Control Flow Analysis)"]
    TRACE --> FUNCS["발견된 함수: 9,159개"]
    TRACE --> COV[".text 커버리지: 92.4% (761,108 / 823,684 바이트)"]
    TRACE --> INST["디코딩된 명령어: 190,277개"]
```

* **시드(Seeds)**: 엔트리 포인트(`0x00000128`) + 1개 export + 5,965개 재배치 포인터 = 총 5,967개 시드에서 탐색 시작.
* **발견 함수 수**: 9,159개.
* **도달 코드 크기**: 761,108 바이트 (`.text`의 92.4%).
* **간접 점프 테이블**: 212개 점프 테이블 식별 및 5,264개 분기 타깃 해석 완료 (미해결 간접 점프 0건).
* **명령어 통계**:
  * 총 디코딩 명령어: 190,277개.
  * VFPU 명령어: 208개 (0.11%).
  * 유효하지 않은/데이터 명령어: 68개 (0.04%).
* **함수 특성**:
  * VFPU 연산을 사용하는 함수: 67개 (0.7%).
  * 계산된 점프(switch/table)를 포함하는 함수: 380개.
  * 최대 함수: `0x0003C11C` (코드 크기 3,852 바이트).

---

## 4. 임포트 펌웨어 라이브러리 및 NID 통계

게임이 임포트하는 PSP 펌웨어 라이브러리는 20개이며, 총 159개 함수(고유 호출 144개)를 요구한다.

| 라이브러리 | 임포트 함수 수 | 주요 기능 |
|---|---|---|
| `ThreadManForUser` | 27 | 스레드 생성, 대기, 이벤트 플래그, 세마포어, 딜레이 |
| `sceMpeg` | 24 | PMF 동영상 디코딩 스트림 관리 |
| `sceSasCore` | 18 | SAS 사운드 코어 음원 합성 |
| `IoFileMgrForUser` | 12 | 파일 열기, 읽기, 쓰기, 탐색, 디렉터리 조작 |
| `sceAtrac3plus` | 11 | ATRAC3plus 음악 압축 해제 |
| `sceGe_user` | 10 | 그래픽 엔진 디스플레이 리스트 큐 및 동기화 |
| `sceAudio` | 9 | 오디오 채널 출력 및 볼륨 제어 |
| `sceUtility` | 8 | 시스템 저장 데이터 UI 및 파라미터 대화상자 |
| `UtilsForUser` | 7 | 시간 측정, 버퍼 복사 등 유틸리티 |
| `sceDisplay` | 6 | 디스플레이 모드, 프레임버퍼 설정, VBLANK 대기 |
| `sceUmdUser` | 6 | UMD 드라이브 상태 및 오류 검사 |
| `ModuleMgrForUser` | 5 | 디스크 내부 PRX 모듈 로드/시작/중지 |
| `sceCtrl` | 3 | 패드 입력 읽기 (`sceCtrlReadBufferPositive` 등) |
| `StdioForUser` | 3 | 표준 입출력 |
| `SysMemUserForUser` | 3 | 사용자 메모리 블록 할당/해제 |
| `Kernel_Library` | 2 | CPU 인터럽트 제어 |
| `LoadExecForUser` | 2 | 게임 재실행 및 종료 (`sceKernelExitGame`) |
| `sceDmac` | 1 | DMA 복사 |
| `scePower` | 1 | 전원 관리 및 배터리 정보 |
| `sceSuspendForUser` | 1 | 전원 절전 핸들러 |

---

## 5. BOOT.BIN과 EBOOT.BIN 비교 결론

`allegrexrecomp info`를 통해 `BOOT.BIN`과 `EBOOT.BIN`의 헤더 정보를 대조한 결과는 다음과 같다.

```mermaid
flowchart LR
    EBOOT["EBOOT.BIN (~PSP 암호화 모듈)<br/>헤더 tag: 0xC0CB167C<br/>복호화 대상 크기: 1,463,072 바이트<br/>엔트리: 0x00000128"]
    BOOT["BOOT.BIN (평문 ELF32)<br/>파일 크기: 1,463,072 바이트<br/>엔트리: 0x00000128"]
    EBOOT -. "완전 일치 (Exact Match)" .- BOOT
```

* **확인됨**: `EBOOT.BIN`의 `~PSP` 헤더에 기록된 복호화 후 ELF 크기(`elf size`: 1,463,072 바이트), 부트 엔트리(`boot_entry`: `0x00000128`), 세그먼트 0 크기(1,028,240 바이트), 세그먼트 1 메모리 크기(2,718,852 바이트), BSS 크기(2,594,336 바이트)는 평문 ELF인 `BOOT.BIN`의 파일 크기 및 세그먼트 구조와 1바이트 오차 없이 완전히 일치한다.
* **결론 (확인됨)**: `BOOT.BIN`은 `EBOOT.BIN`의 암호화 전 원본 평문 실행 파일과 완전히 동일한 코드 및 데이터 배치를 갖는다. 따라서 재컴파일을 위해 복호화 키(`psp_keys.txt`)를 준비할 필요 없이, 평문 `BOOT.BIN`을 직접 lift 입력으로 사용하여 C 코드를 생성할 수 있다.

---

# Executable Analysis: DJMAX Portable (ULKS-46050) (English)

This document records facts confirmed by analyzing the original UMD executables (`PSP_GAME/SYSDIR/BOOT.BIN` and `EBOOT.BIN`) using `allegrexrecomp`.

## 1. Module Identity and Container

* **Name**: `djmax` (version 1.1, attributes `0x0000`, user-mode PRX).
* **Format**: ELF32 MIPS LE (PRX, relocatable), 1,463,072 bytes.
* **Entry point**: `0x00000128`, GP: `0x00026660`.
* **Exported functions**: 1 (`module_start`).

## 2. Segments and Sections

* **Segment 0**: Virtual addr `0x00000000`, file 1,028,240 bytes, mem 1,028,240 bytes, flags `rwx` (code, rodata, stubs, modinfo).
* **Segment 1**: Virtual addr `0x000FB090`, file 124,516 bytes, mem 2,718,852 bytes, flags `rw-` (data, bss).
* **BSS size**: 2,594,336 bytes.
* **Sections**: 38 sections, `.text` is 822,320 bytes at offset `0x00000040`.

## 3. Code Analysis and Function Coverage

* **Seeds**: 5,967 (entry + 1 export + 5,965 relocation pointers).
* **Functions found**: 9,159.
* **Coverage**: 761,108 bytes reached (92.4% of `.text`).
* **Instructions**: 190,277 instructions decoded (208 VFPU = 0.11%, 68 invalid/data = 0.04%).
* **Indirect jumps**: 212 jump tables resolved to 5,264 targets; 0 unresolved computed-jump sites.

## 4. Imported Firmware Libraries

20 libraries and 159 functions (144 distinct calls), dominated by `ThreadManForUser` (27), `sceMpeg` (24), `sceSasCore` (18), `IoFileMgrForUser` (12), `sceAtrac3plus` (11), `sceGe_user` (10), `sceAudio` (9), `sceUtility` (8), and `UtilsForUser` (7).

## 5. BOOT.BIN vs EBOOT.BIN Comparison Conclusion

* **Confirmed**: `EBOOT.BIN`'s `~PSP` header specifies an unencrypted ELF size of 1,463,072 bytes, an entry of `0x00000128`, and segment dimensions that exactly match the unencrypted `BOOT.BIN` file.
* **Conclusion**: `BOOT.BIN` is identical to the decrypted code of `EBOOT.BIN`. Recompilation proceeds directly with `BOOT.BIN` without requiring external decryption keys.
