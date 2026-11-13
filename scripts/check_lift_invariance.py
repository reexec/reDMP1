#!/usr/bin/env python3
"""Verify that allegrexrecomp funcs --list produces identical results under translation."""

import argparse
import re
import subprocess
import sys


def parse_output(text: str, base: int):
    functions_count = None
    reached_line = None
    seeds_count = None
    funcs = {}

    func_re = re.compile(r"^(0x[0-9a-fA-F]+)\s+(\d+)\s+(\d+)(.*)$")

    for line in text.splitlines():
        s = line.strip()
        if s.startswith("seeds:"):
            m = re.match(r"seeds:\s+(\d+)", s)
            if m:
                seeds_count = int(m.group(1))
        elif s.startswith("functions:"):
            m = re.match(r"functions:\s+(\d+)", s)
            if m:
                functions_count = int(m.group(1))
        elif s.startswith("reached:"):
            reached_line = s
        else:
            m = func_re.match(s)
            if m:
                addr = int(m.group(1), 16)
                if addr < base:
                    raise ValueError(f"Function address 0x{addr:08X} < base 0x{base:08X}")
                rel_addr = addr - base
                size = int(m.group(2))
                insns = int(m.group(3))
                flags = m.group(4).strip()
                funcs[rel_addr] = (size, insns, flags)

    return {
        "seeds": seeds_count,
        "functions_count": functions_count,
        "reached": reached_line,
        "funcs": funcs,
    }


def run_funcs(allegrexrecomp: str, binary: str, base: int) -> str:
    cmd = [allegrexrecomp, "funcs", binary, "--list", "--base", hex(base)]
    res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return res.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description="Check lift translation invariance across base addresses.")
    parser.add_argument("--allegrexrecomp", required=True, help="Path to allegrexrecomp executable")
    parser.add_argument("--binary", required=True, help="Path to decrypted PRX binary (e.g. BOOT.BIN)")
    parser.add_argument("--base", default="0x08804000", help="Target relocated base address in hex")
    args = parser.parse_args()

    reloc_base = int(args.base, 0)

    out_zero = run_funcs(args.allegrexrecomp, args.binary, 0)
    out_reloc = run_funcs(args.allegrexrecomp, args.binary, reloc_base)

    data_zero = parse_output(out_zero, 0)
    data_reloc = parse_output(out_reloc, reloc_base)

    errors = []

    if data_zero["seeds"] != data_reloc["seeds"]:
        errors.append(f"Seed count mismatch: base 0={data_zero['seeds']}, base {hex(reloc_base)}={data_reloc['seeds']}")

    if data_zero["functions_count"] != data_reloc["functions_count"]:
        errors.append(
            f"Function count mismatch: base 0={data_zero['functions_count']}, "
            f"base {hex(reloc_base)}={data_reloc['functions_count']}"
        )

    if data_zero["reached"] != data_reloc["reached"]:
        errors.append(
            f"Reached mismatch:\n  base 0: {data_zero['reached']}\n  base {hex(reloc_base)}: {data_reloc['reached']}"
        )

    keys_zero = set(data_zero["funcs"].keys())
    keys_reloc = set(data_reloc["funcs"].keys())

    only_zero = sorted(keys_zero - keys_reloc)
    only_reloc = sorted(keys_reloc - keys_zero)

    if only_zero:
        errors.append(f"{len(only_zero)} functions only in base 0 (first: 0x{only_zero[0]:08X})")
    if only_reloc:
        errors.append(f"{len(only_reloc)} functions only in base {hex(reloc_base)} (first: 0x{only_reloc[0]:08X})")

    common = sorted(keys_zero & keys_reloc)
    diff_info = []
    for k in common:
        if data_zero["funcs"][k] != data_reloc["funcs"][k]:
            diff_info.append((k, data_zero["funcs"][k], data_reloc["funcs"][k]))

    if diff_info:
        first = diff_info[0]
        errors.append(
            f"{len(diff_info)} functions differ in size/insns/flags "
            f"(first at +0x{first[0]:08X}: {first[1]} vs {first[2]})"
        )

    if errors:
        print("LIFT INVARIANCE CHECK FAILED:", file=sys.stderr)
        for e in errors:
            print("  -", e, file=sys.stderr)
        return 1

    print(
        f"lift_invariance passed: {len(common)} functions, {data_zero['seeds']} seeds, "
        f"{data_zero['reached']}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
