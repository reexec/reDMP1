# 디스크 이미지: ULKS-46050 원본 UMD

DJMAX Portable 한국판 원본 UMD의 신원과 구성이다. 신원은 [redump.org #33114](http://redump.org/disc/33114/)의 기록이 기준이다. 덤프가 이 원본과 같은지 확인하는 절차는 [디스크 이미지 확인 가이드](../guides/disc-image-check.md)(issue #1의 구현에서 작성)에 둔다.

## 신원 (redump #33114)

| 항목 | 값 |
|---|---|
| 제목 | DJ Max Portable (디제이맥스 포터블) |
| 매체 | UMD (DL), layerbreak 442,560 |
| 시리얼 | ULKS-46050 |
| 버전 | 1.02 |
| 에디션 | Original |
| 볼륨 레이블 | DJMAX |
| PVD 생성 시각 | 2005-10-14 12:02:48.00 +09:00 |
| 펌웨어 패치 | PSP Firmware 2.00 |
| 섹터 수 / 크기 | 742,896 / 1,521,451,008 바이트 |
| CRC-32 | `06164a6a` |
| MD5 | `48319da34ed98f85a91720cb82d6d6f3` |
| SHA-1 | `80ef0d02e296acf09cba18cee4ac37566b07377c` |

## 구성

* **확인됨**: `UMD_DATA.BIN`의 시리얼은 `ULKS-46050`, `PSP_GAME/PARAM.SFO`는 `DISC_ID` `ULKS46050`, `DISC_VERSION` 1.02, `PSP_SYSTEM_VER` 2.00, `CATEGORY` `UG`, `TITLE` `DJMAX PORTABLE`이다.
* **확인됨**: `PSP_GAME/SYSDIR/EBOOT.BIN`은 `~PSP` 헤더(모듈 이름 `djmax`)로 감싼 암호화 모듈이고, `PSP_GAME/SYSDIR/BOOT.BIN`은 평문 ELF(`7F 45 4C 46`)다.
* **확인됨**: 게임이 디스크에서 직접 올리는 Sony 사용자 모드 PRX가 `PSP_GAME/USRDIR/modules/`에 있다: `audiocodec.prx`, `libatrac3plus.prx`, `mpeg.prx`, `mpegbase.prx`, `sc_sascore.prx`, `videocodec.prx`. 규칙에 따라 이것들은 lift하지 않고 NID 경계에서 HLE한다.
* **확인됨**: 동영상은 PMF 형식이다(`USRDIR/movie/opening.pmf`, `USRDIR/system/gallery/media/media*.pmf`).
* **추정**: `USRDIR/all.mp`(436 MB), `all2.mp`(842 MB)는 게임 자산 묶음이다. 형식은 자산 로딩 task에서 분석한다.
* **확인됨**: `PSP_GAME/SYSDIR/UPDATE/`는 펌웨어 2.00 업데이터 자리다. 게임 코드가 아니므로 재컴파일과 실행에 쓰지 않는다. 그래서 업데이트 파티션만 비운 이미지도 재컴파일에는 충분하지만, 그런 이미지는 redump 해시와 맞지 않는다.

## 미확정

* `BOOT.BIN`이 `EBOOT.BIN`을 복호화한 결과와 같은 코드인가. 같으면 키 없이 `BOOT.BIN`을 lift 입력으로 쓸 수 있다. issue #1에서 psprecomp의 `allegrexrecomp`로 비교해 이 절을 갱신한다.

---

# Disc image: the original ULKS-46050 UMD

The identity and layout of the original UMD of DJMAX Portable, the Korea build. Identity is taken from the [redump.org #33114](http://redump.org/disc/33114/) record. The procedure for checking that a dump matches this original goes in the [disc image check guide](../guides/disc-image-check.md) (written in issue #1's implementation).

## Identity (redump #33114)

See the table above: UMD (DL) with layerbreak 442,560, serial ULKS-46050, version 1.02, Original edition, volume label DJMAX, PVD created 2005-10-14 12:02:48.00 +09:00, PSP Firmware 2.00 patch, 742,896 sectors (1,521,451,008 bytes), CRC-32 `06164a6a`, MD5 `48319da34ed98f85a91720cb82d6d6f3`, SHA-1 `80ef0d02e296acf09cba18cee4ac37566b07377c`.

## Layout

* **Confirmed**: `UMD_DATA.BIN` carries the serial `ULKS-46050`; `PSP_GAME/PARAM.SFO` has `DISC_ID` `ULKS46050`, `DISC_VERSION` 1.02, `PSP_SYSTEM_VER` 2.00, `CATEGORY` `UG` and `TITLE` `DJMAX PORTABLE`.
* **Confirmed**: `PSP_GAME/SYSDIR/EBOOT.BIN` is an encrypted module wrapped in a `~PSP` header (module name `djmax`); `PSP_GAME/SYSDIR/BOOT.BIN` is a plain ELF (`7F 45 4C 46`).
* **Confirmed**: the Sony user-mode PRX modules the game loads from its own disc are in `PSP_GAME/USRDIR/modules/`: `audiocodec.prx`, `libatrac3plus.prx`, `mpeg.prx`, `mpegbase.prx`, `sc_sascore.prx`, `videocodec.prx`. Under the rules they are not lifted but HLE'd at the NID boundary.
* **Confirmed**: movies are PMF (`USRDIR/movie/opening.pmf`, `USRDIR/system/gallery/media/media*.pmf`).
* **Inferred**: `USRDIR/all.mp` (436 MB) and `all2.mp` (842 MB) are the game's asset archives; their format is analyzed in the asset-loading task.
* **Confirmed**: `PSP_GAME/SYSDIR/UPDATE/` holds the firmware 2.00 updater. It is not game code and is used by neither recompilation nor execution, so an image with only the update partition emptied is enough to recompile, though it no longer matches redump's hashes.

## Unresolved

* Whether `BOOT.BIN` holds the same code as `EBOOT.BIN` decrypted. If so, `BOOT.BIN` is the lift input and no keys are needed. Issue #1 compares them with psprecomp's `allegrexrecomp` and updates this section.
