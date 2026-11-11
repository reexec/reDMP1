# 디스크 이미지 확인 가이드: redump #33114 검증 (Disc Image Check Guide)

이 문서는 사용자가 직접 정품 UMD로부터 덤프한 디스크 이미지(ISO)가 보존 데이터베이스 [redump.org #33114](http://redump.org/disc/33114/)에 등록된 원본 미디어와 1바이트 오차 없이 일치하는지 확인하는 일반 검증 절차를 설명합니다.

---

## 1. redump.org #33114 기준값

| 항목 | 기준 해시 및 규격 |
|---|---|
| 타이틀 | DJ Max Portable (디제이맥스 포터블) |
| 시리얼 | ULKS-46050 (디스크 라벨 `ULKS-46050`, PVD 볼륨 `DJMAX`) |
| 디스크 버전 | 1.02 |
| 총 크기 | 1,521,451,008 바이트 (742,896 섹터) |
| 레이어 브레이크 | 442,560 (듀얼 레이어 UMD) |
| CRC-32 | `06164a6a` |
| MD5 | `48319da34ed98f85a91720cb82d6d6f3` |
| SHA-1 | `80ef0d02e296acf09cba18cee4ac37566b07377c` |

---

## 2. Windows에서 해시 검증 절차

PowerShell 내장 명령어를 통해 ISO 파일의 크기와 해시를 검사합니다.

### 2.1. 파일 크기 확인

```powershell
(Get-Item ".\DJ Max Portable.iso").Length
# 출력값은 반드시 1521451008 이어야 합니다.
```

### 2.2. SHA-1 및 MD5 해시 계산

```powershell
# SHA-1 계산 (기준: 80ef0d02e296acf09cba18cee4ac37566b07377c)
Get-FileHash -Algorithm SHA1 ".\DJ Max Portable.iso"

# MD5 계산 (기준: 48319da34ed98f85a91720cb82d6d6f3)
Get-FileHash -Algorithm MD5 ".\DJ Max Portable.iso"
```

### 2.3. 7-Zip을 이용한 CRC-32 확인

```powershell
7z h -scrc* ".\DJ Max Portable.iso"
# CRC-32 값이 06164A6A 인지 확인합니다.
```

---

## 3. Linux / macOS에서 해시 검증 절차

```bash
# 파일 크기 검사
stat -c %s "DJ Max Portable.iso"
# 1521451008

# SHA-1 검사
sha1sum "DJ Max Portable.iso"
# 80ef0d02e296acf09cba18cee4ac37566b07377c

# MD5 검사
md5sum "DJ Max Portable.iso"
# 48319da34ed98f85a91720cb82d6d6f3
```

---

## 4. 해시 불일치 시 주의사항

* **수정된 이미지/립(Rip) 버전**: 더미 파일이 제거되었거나, 동영상이 재인코딩된 이미지(CSO 변환 후 역변환 등)는 redump 기준 해시와 일치하지 않습니다.
* **업데이터 파티션 제거본**: `PSP_GAME/SYSDIR/UPDATE/` 디렉터리가 비어 있는 이미지의 경우, 게임 실행에는 지장이 없을 수 있으나 ISO 전체 SHA-1 해시는 일치하지 않습니다.
* 재컴파일 파이프라인의 기준 검증을 통과하기 위해서는 원본 1:1 무변형 덤프를 사용하는 것을 강력히 권장합니다.

---

# Disc Image Check Guide (English)

Procedure to verify a dumped UMD ISO against [redump.org #33114](http://redump.org/disc/33114/).

## Reference Hashes

* File size: 1,521,451,008 bytes
* CRC-32: `06164a6a`
* MD5: `48319da34ed98f85a91720cb82d6d6f3`
* SHA-1: `80ef0d02e296acf09cba18cee4ac37566b07377c`

## Verification

On Windows (PowerShell):
```powershell
(Get-Item ".\DJ Max Portable.iso").Length
Get-FileHash -Algorithm SHA1 ".\DJ Max Portable.iso"
Get-FileHash -Algorithm MD5 ".\DJ Max Portable.iso"
```

On Linux:
```bash
sha1sum "DJ Max Portable.iso"
md5sum "DJ Max Portable.iso"
```
