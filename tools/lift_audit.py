#!/usr/bin/env python3
"""Static audit of allegrexrecomp-generated C bodies against original MIPS instructions."""

from __future__ import annotations

import argparse
import glob
import re
import struct
import sys
from collections import Counter
from pathlib import Path

BODY_START_RE = re.compile(r"^static void psp_body_([0-9A-Fa-f]{8})\(uint32_t _entry\)\s*\{")
INSN_CMT_RE = re.compile(r"^\s*/\*\s*([0-9A-Fa-f]{8})\s+([a-zA-Z0-9_.]+)(?:\s+(.*?))?\*/\s*$")
THUNK_RE = re.compile(r"^void (psp_(?:func|at)_[0-9A-Fa-f]{8})\(void\)")
TAIL_CALL_RE = re.compile(r"\b(psp_func_[0-9A-Fa-f]{8}|psp_at_[0-9A-Fa-f]{8}|psp_import_[0-9A-Fa-f]{8}|psp_dispatch)\(")

BRANCH_LIKELY = {
    "beql", "bnel", "blezl", "bgtzl", "bltzl", "bgezl", "bc1fl", "bc1tl",
    "bgezall", "bltzall",
}
BRANCH_COND_CALL = {"bgezal", "bltzal", "bgezall", "bltzall"}
BRANCH_ORD = {
    "beq", "bne", "blez", "bgtz", "bltz", "bgez", "bc1f", "bc1t",
    "beqz", "bnez",
}
ALL_BRANCHES = BRANCH_LIKELY | BRANCH_COND_CALL | BRANCH_ORD


def read_u32(image: bytes | None, base: int, addr: int) -> int | None:
    if image is None or addr < base:
        return None
    off = addr - base
    if off + 4 > len(image):
        return None
    return struct.unpack_from("<I", image, off)[0]


def decode_mips_kind(word: int, addr: int) -> str:
    """Minimal raw Allegrex decoder for cross-checking end-of-body transfers."""
    op = (word >> 26) & 0x3F
    rs = (word >> 21) & 0x1F
    rt = (word >> 16) & 0x1F
    fn = word & 0x3F
    if op == 0x00:
        if fn == 0x08:  # jr
            return "jr_ra" if rs == 31 else "jr_reg"
        if fn == 0x09:  # jalr
            return "jalr"
        if fn == 0x0C:
            return "syscall"
        if fn == 0x0D:
            return "break"
        return "other"
    if op == 0x01:
        if rt in (0x10, 0x11):  # bltzal, bgezal
            return "cond_call"
        if rt in (0x12, 0x13):  # bltzall, bgezall
            return "cond_call_likely"
        if rt in (0x02, 0x03):  # bltzl, bgezl
            return "branch_likely"
        if rt in (0x00, 0x01):  # bltz, bgez
            return "branch"
        return "other"
    if op == 0x02:
        return "j"
    if op == 0x03:
        imm26 = word & 0x03FFFFFF
        target = ((addr + 4) & 0xF0000000) | (imm26 << 2)
        return "get_pc" if target == addr + 8 else "jal"
    if op in (0x04, 0x05, 0x06, 0x07):
        return "branch"
    if op in (0x14, 0x15, 0x16, 0x17):
        return "branch_likely"
    if op == 0x11 and rs == 0x08:  # bc1*
        nd_tf = (word >> 16) & 0x03
        return "branch_likely" if (nd_tf & 0x02) else "branch"
    return "other"


def classify_mnemonic(mnem: str, ops: str, addr: int) -> str:
    m = mnem.lower()
    if m == "jr":
        return "jr_ra" if "$ra" in ops else "jr_reg"
    if m == "jalr":
        return "jalr"
    if m == "j":
        return "j"
    if m == "jal":
        m_tgt = re.search(r"0x([0-9A-Fa-f]+)", ops)
        if m_tgt and int(m_tgt.group(1), 16) == addr + 8:
            return "get_pc"
        return "jal"
    if m in BRANCH_COND_CALL:
        return "cond_call_likely" if m.endswith("l") and m != "bgezal" and m != "bltzal" else "cond_call"
    if m in BRANCH_LIKELY:
        return "branch_likely"
    if m in BRANCH_ORD:
        return "branch"
    if m == "syscall":
        return "syscall"
    if m == "break":
        return "break"
    return "other"


def has_fallthrough_tail(tail_lines: list[str]) -> bool:
    joined = "\n".join(tail_lines)
    if "falls through into the next function" in joined:
        return True
    if "falls through into 0x" in joined:
        return True
    return False


def audit_body(
    body_addr: int,
    lines: list[str],
    image: bytes | None,
    base: int,
    known_entries: set[int],
    counts: Counter[str],
    mismatches: list[dict],
) -> None:
    insns: list[tuple[int, str, str, int]] = []  # (addr, mnem, ops, line_idx)
    for idx, line in enumerate(lines):
        m = INSN_CMT_RE.match(line)
        if m:
            insn_addr = int(m.group(1), 16)
            mnem = m.group(2)
            ops = (m.group(3) or "").strip()
            insns.append((insn_addr, mnem, ops, idx))

    if not insns:
        counts["empty_body"] += 1
        mismatches.append({
            "body": body_addr,
            "kind": "empty_body",
            "insn_addr": body_addr,
            "insn": "<none>",
            "reason": "body has no decoded instructions",
            "tail": lines[-8:],
        })
        return

    # Check if last emitted instruction was a delay slot of the immediately preceding transfer.
    last_idx = len(insns) - 1
    ctrl_idx = last_idx
    if len(insns) >= 2:
        prev_addr, prev_mnem, prev_ops, _ = insns[-2]
        prev_kind = classify_mnemonic(prev_mnem, prev_ops, prev_addr)
        cur_addr = insns[-1][0]
        if prev_kind != "other" and prev_kind not in ("syscall", "break") and cur_addr == prev_addr + 4:
            ctrl_idx = last_idx - 1

    ctrl_addr, ctrl_mnem, ctrl_ops, ctrl_line = insns[ctrl_idx]
    kind = classify_mnemonic(ctrl_mnem, ctrl_ops, ctrl_addr)

    # Cross-check with relocated image if available.
    raw_word = read_u32(image, base, ctrl_addr)
    if raw_word is not None:
        raw_kind = decode_mips_kind(raw_word, ctrl_addr)
        if raw_kind != kind and not (kind == "other" and raw_kind == "other"):
            mismatches.append({
                "body": body_addr,
                "kind": f"decode_mismatch({kind}!={raw_kind})",
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops} (raw=0x{raw_word:08X})",
                "reason": "comment mnemonic disagrees with raw image word",
                "tail": lines[max(0, ctrl_line - 1):],
            })

    # Determine the end address of this body's trailing control/fall-through region.
    has_slot = (ctrl_idx != last_idx)
    next_addr = ctrl_addr + (8 if (kind != "other" and kind not in ("syscall", "break") and has_slot) else 4)
    if kind == "other":
        next_addr = insns[-1][0] + 4

    counts[kind] += 1
    tail_lines = lines[ctrl_line:]
    tail_joined = "\n".join(tail_lines)

    # Verify end-of-body shape matches instruction kind.
    if kind == "jr_ra":
        if "psp_ra_is_jump(" not in tail_joined or "PSP_SP_CHECK(" not in tail_joined or "return;" not in tail_joined:
            mismatches.append({
                "body": body_addr,
                "kind": kind,
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops}",
                "reason": "jr $ra missing psp_ra_is_jump / PSP_SP_CHECK / return",
                "tail": tail_lines[-10:],
            })
    elif kind == "jr_reg":
        if "psp_dispatch(" not in tail_joined or "return;" not in tail_joined:
            mismatches.append({
                "body": body_addr,
                "kind": kind,
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops}",
                "reason": "jr <reg> missing psp_dispatch / return",
                "tail": tail_lines[-10:],
            })
    elif kind == "j":
        has_goto = "goto L_" in tail_joined
        has_tail = bool(TAIL_CALL_RE.search(tail_joined)) and "return;" in tail_joined
        if not (has_goto or has_tail):
            mismatches.append({
                "body": body_addr,
                "kind": kind,
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops}",
                "reason": "j missing goto or tail transfer + return",
                "tail": tail_lines[-10:],
            })
    elif kind in ("jal", "jalr", "cond_call", "cond_call_likely", "get_pc"):
        # Ending in a call / get_pc must continue at next_addr (fall-through) unless next_addr is unowned data.
        if kind in ("jal", "jalr") and "PSP_CALL_END(" not in tail_joined:
            mismatches.append({
                "body": body_addr,
                "kind": kind,
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops}",
                "reason": f"{kind} missing PSP_CALL_END",
                "tail": tail_lines[-10:],
            })
        elif not has_fallthrough_tail(tail_lines):
            next_word = read_u32(image, base, next_addr)
            next_is_entry = next_addr in known_entries
            mismatches.append({
                "body": body_addr,
                "kind": f"{kind}_no_fallthrough",
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops}",
                "reason": (
                    f"ends in {kind} without fall-through to 0x{next_addr:08X} "
                    f"(known_entry={next_is_entry}, next_word={f'0x{next_word:08X}' if next_word is not None else 'N/A'})"
                ),
                "tail": tail_lines[-10:],
            })
    elif kind in ("branch", "branch_likely"):
        # Ending in a conditional branch means the not-taken path falls out of the body!
        if not has_fallthrough_tail(tail_lines):
            next_word = read_u32(image, base, next_addr)
            next_is_entry = next_addr in known_entries
            mismatches.append({
                "body": body_addr,
                "kind": f"{kind}_no_fallthrough",
                "insn_addr": ctrl_addr,
                "insn": f"{ctrl_mnem} {ctrl_ops}",
                "reason": (
                    f"ends in {kind} without not-taken fall-through to 0x{next_addr:08X} "
                    f"(known_entry={next_is_entry}, next_word={f'0x{next_word:08X}' if next_word is not None else 'N/A'})"
                ),
                "tail": tail_lines[-10:],
            })
    elif kind in ("syscall", "break"):
        pass
    elif kind == "other":
        # Non-branching last instruction: either falls through to next function/label, or falls off into unowned data.
        if not has_fallthrough_tail(tail_lines):
            next_word = read_u32(image, base, next_addr)
            next_is_entry = next_addr in known_entries
            # Flag if next_addr is a known entry/label or looks like real non-zero code (not padding nop/zero).
            if next_is_entry or (next_word is not None and next_word != 0):
                mismatches.append({
                    "body": body_addr,
                    "kind": "fallout_silent_return",
                    "insn_addr": ctrl_addr,
                    "insn": f"{ctrl_mnem} {ctrl_ops}",
                    "reason": (
                        f"falls off end without transfer to 0x{next_addr:08X} "
                        f"(known_entry={next_is_entry}, next_word={f'0x{next_word:08X}' if next_word is not None else 'N/A'})"
                    ),
                    "tail": tail_lines[-10:],
                })

    # Interior check: likely branches must emit their delay slot inside `if (...) {`, not before `if`.
    for i_idx, (i_addr, i_mnem, i_ops, i_line) in enumerate(insns):
        ik = classify_mnemonic(i_mnem, i_ops, i_addr)
        if ik in ("branch_likely", "cond_call_likely"):
            # Look at lines between i_line and next insn or if-block.
            window = lines[i_line:min(len(lines), i_line + 8)]
            if_pos = next((k for k, l in enumerate(window) if "if (" in l), None)
            slot_pos = next((k for k, l in enumerate(window) if f"{i_addr + 4:08X}" in l), None)
            if if_pos is not None and slot_pos is not None and slot_pos < if_pos:
                mismatches.append({
                    "body": body_addr,
                    "kind": "likely_slot_before_if",
                    "insn_addr": i_addr,
                    "insn": f"{i_mnem} {i_ops}",
                    "reason": "likely branch delay slot emitted before `if` condition",
                    "tail": window,
                })


def main() -> int:
    ap = argparse.ArgumentParser(description="Audit allegrexrecomp generated C bodies.")
    ap.add_argument("--generated", default="src/game/dmp1/recomp/generated", help="Generated C directory")
    ap.add_argument("--image", default=None, help="Relocated binary image (e.g. from allegrexrecomp relocate)")
    ap.add_argument("--base", default="0x08804000", help="Image base address in hex")
    ap.add_argument("--max-mismatches", type=int, default=40, help="Max mismatches to print (0 = all)")
    args = ap.parse_args()

    gen_dir = Path(args.generated)
    c_files = sorted(gen_dir.glob("*_funcs_*.c"))
    if not c_files:
        print(f"Error: no *_funcs_*.c files found in {gen_dir}", file=sys.stderr)
        return 1

    image: bytes | None = None
    base = int(args.base, 16)
    if args.image:
        image = Path(args.image).read_bytes()

    # Pass 1: collect all known psp_func_ / psp_at_ entries.
    known_entries: set[int] = set()
    for cf in c_files:
        for line in cf.read_text(encoding="utf-8", errors="replace").splitlines():
            mt = THUNK_RE.match(line)
            if mt:
                known_entries.add(int(mt.group(1).rsplit("_", 1)[1], 16))

    # Pass 2: parse and audit each psp_body_.
    counts: Counter[str] = Counter()
    mismatches: list[dict] = []
    total_bodies = 0

    for cf in c_files:
        text_lines = cf.read_text(encoding="utf-8", errors="replace").splitlines()
        cur_addr: int | None = None
        cur_lines: list[str] = []
        for line in text_lines:
            mb = BODY_START_RE.match(line)
            if mb:
                cur_addr = int(mb.group(1), 16)
                cur_lines = [line]
                continue
            if cur_addr is not None:
                cur_lines.append(line)
                if line == "}":
                    total_bodies += 1
                    audit_body(cur_addr, cur_lines, image, base, known_entries, counts, mismatches)
                    cur_addr = None
                    cur_lines = []

    print(f"Audited {total_bodies} bodies across {len(c_files)} files (known thunks: {len(known_entries)}).")
    print("End-of-body classification counts:")
    for k, v in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"  {k:20s}: {v}")

    by_kind = Counter(m["kind"] for m in mismatches)
    print(f"\nMismatches found: {len(mismatches)}")
    for k, v in sorted(by_kind.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"  {k:32s}: {v}")

    limit = len(mismatches) if args.max_mismatches == 0 else min(args.max_mismatches, len(mismatches))
    if limit > 0:
        print(f"\nFirst {limit} mismatches:")
        for i, mm in enumerate(mismatches[:limit], 1):
            print(f"[{i}] body=0x{mm['body']:08X} insn=0x{mm['insn_addr']:08X} ({mm['kind']}) :: {mm['insn']}")
            print(f"    reason: {mm['reason']}")
            for tl in mm["tail"]:
                print(f"      | {tl}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
