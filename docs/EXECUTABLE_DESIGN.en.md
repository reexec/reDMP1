# DJMAX Portable Executable Architecture and Design (EXECUTABLE_DESIGN)

This document accumulates findings on program architecture, memory layout, subsystem interactions, and design confirmed by analyzing the original executables (`PSP_GAME/SYSDIR/BOOT.BIN` and `EBOOT.BIN`) of DJMAX Portable (Korea `ULKS-46050`, 1.02).

---

## 1. Module Overview

* **Module Name**: `djmax` (version 1.1)
* **Module Attributes**: `0x0000` (User Mode PRX)
* **Entry Point**: `0x00000128` (`module_start`)
* **GP ($gp) Register**: `0x00026660`
* **Container**: ELF32 MIPS R4000 Little-Endian (PRX, Relocatable)
* **Plain vs Encrypted Binary Equivalence**: `EBOOT.BIN`'s Sony `~PSP` header tag is `0xC0CB167C`, and its decrypted ELF size (1,463,072 bytes) and segment layout match unencrypted `BOOT.BIN` 100%.

---

## 2. Memory Segments and Runtime Layout

The binary consists of 2 `PT_LOAD` segments:

| Segment | Base Address | File Size | Memory Size | Flags | Description |
|---|---|---|---|---|---|
| Segment 0 | `0x00000000` | 1,028,240 bytes | 1,028,240 bytes | `rwx` | Code (`.text`), read-only data, import/export stubs |
| Segment 1 | `0x000FB090` | 124,516 bytes | 2,718,852 bytes | `rw-` | Global variables (`.data`), runtime zero-filled area (`.bss`, 2,594,336 bytes) |

* Total runtime memory required (including BSS): ~3.75 MiB (3,747,092 bytes).
* Loaded and relocated above `0x08800000` in PSP user memory (24 MiB user RAM).

---

## 3. Subsystems and Imported Firmware Libraries

The game imports 144 distinct system functions across 20 firmware libraries:

* **Audio**: `sceSasCore` (18 functions for key sounds and sound effects), `sceAtrac3plus` (11 functions for BGM playback), `sceAudio` (9 functions for PCM output).
* **Video & Graphics**: `sceMpeg` (24 functions for PMF video playback), `sceGe_user` (10 functions for display list rendering), `sceDisplay` (6 functions for display mode and VBLANK wait).
* **Kernel & System**: `ThreadManForUser` (27 functions for threading and synchronization), `IoFileMgrForUser` (12 functions for file streaming), `sceUtility` (8 functions for save/load dialogs), `sceCtrl` (3 functions for button polling).

---

## 4. Control Flow and Function Analysis

* Functions found: 9,159.
* Code coverage: 92.4% of `.text` reached (761,108 / 823,684 bytes).
* VFPU usage: 208 instructions (0.11% of instructions), used in 67 functions.
* Indirect jump tables: 212 tables resolved to 5,264 branch targets with 0 unresolved computed-jump sites.

For detailed numbers and statistics, see [docs/analysis/executable.md](analysis/executable.md).
