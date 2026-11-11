#!/usr/bin/env python3
"""Generate a valid synthetic PARAM.SFO file for testing."""

import argparse
import struct
import sys
from pathlib import Path

def generate_sfo(entries: dict[str, str], out_path: Path):
    keys = sorted(entries.keys())
    key_table = b"".join(k.encode("utf-8") + b"\x00" for k in keys)
    pad_keys = (4 - (len(key_table) % 4)) % 4
    key_table_padded = key_table + (b"\x00" * pad_keys)

    header_size = 20
    entries_size = len(keys) * 16
    key_offset = header_size + entries_size
    data_offset = key_offset + len(key_table_padded)

    data_table = b""
    entry_records = []
    curr_k_off = 0
    for k in keys:
        val = entries[k].encode("utf-8") + b"\x00"
        pad_data = (4 - (len(val) % 4)) % 4
        val_padded = val + (b"\x00" * pad_data)

        val_len = len(val)
        val_off = len(data_table)
        data_table += val_padded

        entry_records.append((curr_k_off, 0x0204, val_len, len(val_padded), val_off))
        curr_k_off += len(k.encode("utf-8")) + 1

    header = struct.pack("<4sIIII", b"\x00PSF", 0x00000101, key_offset, data_offset, len(keys))
    entries_bytes = b"".join(
        struct.pack("<HHIII", k_off, fmt, d_len, max_len, d_off)
        for k_off, fmt, d_len, max_len, d_off in entry_records
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(header + entries_bytes + key_table_padded + data_table)

def main():
    parser = argparse.ArgumentParser(description="Generate synthetic PARAM.SFO")
    parser.add_argument("out_file", help="Path to output PARAM.SFO")
    parser.add_argument("--disc-id", default="TEST00000", help="DISC_ID value")
    parser.add_argument("--disc-version", default="1.00", help="DISC_VERSION value")
    parser.add_argument("--title", default="Synthetic Test Game", help="TITLE value")
    args = parser.parse_args()

    entries = {
        "DISC_ID": args.disc_id,
        "DISC_VERSION": args.disc_version,
        "TITLE": args.title,
    }
    generate_sfo(entries, Path(args.out_file))

if __name__ == "__main__":
    main()
