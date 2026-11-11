#!/usr/bin/env python3
"""Register a disc dump into src/game/dmp1/recomp/variants.json."""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
VARIANTS_JSON = REPO_ROOT / "src" / "game" / "dmp1" / "recomp" / "variants.json"

def parse_sfo(data: bytes) -> dict[str, str | int]:
    if len(data) < 20 or data[:4] != b"\x00PSF":
        raise ValueError("Invalid SFO magic")

    _, _, key_offset, data_offset, num_entries = struct.unpack_from("<4sIIII", data, 0)
    entries = {}

    entry_struct = struct.Struct("<HHIII")
    for i in range(num_entries):
        k_off, fmt, d_len, _, d_off = entry_struct.unpack_from(data, 20 + i * entry_struct.size)
        # Read null-terminated key from key table
        k_start = key_offset + k_off
        k_end = data.find(b"\x00", k_start)
        key = data[k_start:k_end].decode("utf-8", errors="replace")

        # Read value based on format
        v_start = data_offset + d_off
        if fmt == 0x0204:  # UTF-8 string
            val_bytes = data[v_start:v_start + d_len].rstrip(b"\x00")
            entries[key] = val_bytes.decode("utf-8", errors="replace")
        elif fmt == 0x0404:  # 32-bit uint
            entries[key] = struct.unpack_from("<I", data, v_start)[0]
        else:
            entries[key] = data[v_start:v_start + d_len]

    return entries

def main():
    parser = argparse.ArgumentParser(description="Register disc dump into variants.json")
    parser.add_argument("game_dir", nargs="?", default="roms", help="Path to extracted game directory (default: roms)")
    args = parser.parse_args()

    game_dir = Path(args.game_dir).resolve()
    if not game_dir.is_dir():
        print(f"Error: Directory not found: {game_dir}", file=sys.stderr)
        sys.exit(1)

    sfo_path = game_dir / "PSP_GAME" / "PARAM.SFO"
    if not sfo_path.is_file():
        print(f"Error: PARAM.SFO not found at {sfo_path}", file=sys.stderr)
        sys.exit(1)

    with open(sfo_path, "rb") as f:
        sfo_data = f.read()

    try:
        sfo = parse_sfo(sfo_data)
    except Exception as e:
        print(f"Error: Failed to parse PARAM.SFO: {e}", file=sys.stderr)
        sys.exit(1)

    disc_id = str(sfo.get("DISC_ID", "")).strip()
    disc_version = str(sfo.get("DISC_VERSION", "")).strip()
    title = str(sfo.get("TITLE", "")).strip()

    # Find candidate executable: prefer BOOT.BIN, fallback to EBOOT.BIN
    boot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "BOOT.BIN"
    eboot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "EBOOT.BIN"

    if boot_bin.is_file():
        exe_path = boot_bin
        exe_name = "BOOT.BIN"
    elif eboot_bin.is_file():
        exe_path = eboot_bin
        exe_name = "EBOOT.BIN"
    else:
        print("Error: Neither BOOT.BIN nor EBOOT.BIN found in PSP_GAME/SYSDIR", file=sys.stderr)
        sys.exit(1)

    with open(exe_path, "rb") as f:
        exe_bytes = f.read()

    sha1 = hashlib.sha1(exe_bytes).hexdigest()
    size = len(exe_bytes)

    print(f"Disc Identity Found:")
    print(f"  Title:       {title}")
    print(f"  Disc ID:     {disc_id}")
    print(f"  Version:     {disc_version}")
    print(f"  Executable:  {exe_name} ({size} bytes)")
    print(f"  SHA-1:       {sha1}")

    # Load existing variants
    variants = []
    if VARIANTS_JSON.is_file():
        try:
            with open(VARIANTS_JSON, "r", encoding="utf-8") as f:
                variants = json.load(f)
        except Exception:
            variants = []

    # Check if already registered
    for v in variants:
        if v.get("disc_id") == disc_id and v.get("disc_version") == disc_version and v.get("sha1") == sha1:
            print(f"\nVariant is already registered in {VARIANTS_JSON}")
            return

    new_variant = {
        "disc_id": disc_id,
        "disc_version": disc_version,
        "title": title,
        "executable": exe_name,
        "sha1": sha1,
        "size": size,
    }
    variants.append(new_variant)

    VARIANTS_JSON.parent.mkdir(parents=True, exist_ok=True)
    with open(VARIANTS_JSON, "w", encoding="utf-8") as f:
        json.dump(variants, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"\nSuccessfully registered new variant into {VARIANTS_JSON}")

if __name__ == "__main__":
    main()
