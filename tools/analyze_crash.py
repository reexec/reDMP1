#!/usr/bin/env python3
"""Analyze reDMP1 crash reports and Windows minidumps (.dmp) using dbghelp.dll + PDBs."""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import struct
import sys
from typing import Dict, List, Optional, Tuple


# Minidump stream types (Microsoft dbghelp.h MINIDUMP_STREAM_TYPE)
STREAM_THREAD_LIST = 3
STREAM_MODULE_LIST = 4
STREAM_MEMORY_LIST = 5
STREAM_EXCEPTION = 6
STREAM_MEMORY64_LIST = 9

EXCEPTION_NAMES = {
    0xC0000005: "EXCEPTION_ACCESS_VIOLATION",
    0xC000001D: "EXCEPTION_ILLEGAL_INSTRUCTION",
    0xC0000094: "EXCEPTION_INT_DIVIDE_BY_ZERO",
    0xC00000FD: "EXCEPTION_STACK_OVERFLOW",
    0x80000003: "EXCEPTION_BREAKPOINT",
    0xE0000001: "REPSP_RAISE_STOP",
    0xE06D7363: "MSVC_CPP_EXCEPTION",
    0x40000015: "STATUS_FATAL_APP_EXIT (abort)",
}


class MemoryMap:
    """Virtual-memory lookup backed by Minidump MemoryList / Memory64List."""

    def __init__(self, data: bytes) -> None:
        self._data = data
        self._ranges: List[Tuple[int, int, int]] = []  # (va_start, size, file_offset)

    def add_range(self, va_start: int, size: int, file_offset: int) -> None:
        if size > 0 and file_offset + size <= len(self._data):
            self._ranges.append((va_start, size, file_offset))

    def finalize(self) -> None:
        self._ranges.sort(key=lambda r: r[0])

    def read(self, va: int, size: int) -> Optional[bytes]:
        for start, rsize, foff in self._ranges:
            if start <= va and va + size <= start + rsize:
                delta = va - start
                return self._data[foff + delta : foff + delta + size]
        return None

    def read_u32(self, va: int) -> Optional[int]:
        raw = self.read(va, 4)
        return struct.unpack("<I", raw)[0] if raw is not None else None

    def read_u64(self, va: int) -> Optional[int]:
        raw = self.read(va, 8)
        return struct.unpack("<Q", raw)[0] if raw is not None else None

    def read_cstring(self, va: int, max_len: int = 256) -> Optional[str]:
        out = bytearray()
        for i in range(max_len):
            b = self.read(va + i, 1)
            if b is None:
                return None
            if b[0] == 0:
                return out.decode("utf-8", errors="replace")
            if b[0] < 0x20 or b[0] >= 0x7F:
                return None
            out.extend(b)
        return out.decode("utf-8", errors="replace")


def read_mdmp_string(data: bytes, rva: int) -> str:
    if rva == 0 or rva + 4 > len(data):
        return ""
    byte_len = struct.unpack_from("<I", data, rva)[0]
    start = rva + 4
    end = min(start + byte_len, len(data))
    return data[start:end].decode("utf-16-le", errors="replace")


class DbgHelpResolver:
    """Resolve addresses to PDB symbol + source line via Windows dbghelp.dll."""

    SYMOPT_UNDNAME = 0x00000002
    SYMOPT_DEFERRED_LOADS = 0x00000004
    SYMOPT_LOAD_LINES = 0x00000010

    def __init__(self, search_path: str) -> None:
        self._available = False
        self._dbghelp = None
        self._hproc = ctypes.c_void_p(0x1337)
        if sys.platform != "win32":
            return
        try:
            self._dbghelp = ctypes.WinDLL("dbghelp.dll")
        except OSError:
            return

        self._dbghelp.SymSetOptions.argtypes = [wintypes.DWORD]
        self._dbghelp.SymSetOptions.restype = wintypes.DWORD
        self._dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
        self._dbghelp.SymInitializeW.restype = wintypes.BOOL
        self._dbghelp.SymLoadModuleExW.argtypes = [
            wintypes.HANDLE, wintypes.HANDLE, wintypes.LPCWSTR, wintypes.LPCWSTR,
            ctypes.c_uint64, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
        ]
        self._dbghelp.SymLoadModuleExW.restype = ctypes.c_uint64
        self._dbghelp.SymCleanup.argtypes = [wintypes.HANDLE]
        self._dbghelp.SymCleanup.restype = wintypes.BOOL

        self._dbghelp.SymSetOptions(
            self.SYMOPT_UNDNAME | self.SYMOPT_DEFERRED_LOADS | self.SYMOPT_LOAD_LINES
        )
        if self._dbghelp.SymInitializeW(self._hproc, search_path or None, False):
            self._available = True

    def load_module(self, path: str, base: int, size: int) -> None:
        if not self._available:
            return
        self._dbghelp.SymLoadModuleExW(self._hproc, None, path, None, base, size, None, 0)

    def resolve(self, addr: int) -> Tuple[Optional[str], int, Optional[str], Optional[int]]:
        if not self._available or addr == 0:
            return (None, 0, None, None)

        class SYMBOL_INFOW(ctypes.Structure):
            _fields_ = [
                ("SizeOfStruct", wintypes.ULONG),
                ("TypeIndex", wintypes.ULONG),
                ("Reserved", ctypes.c_uint64 * 2),
                ("Index", wintypes.ULONG),
                ("Size", wintypes.ULONG),
                ("ModBase", ctypes.c_uint64),
                ("Flags", wintypes.ULONG),
                ("Value", ctypes.c_uint64),
                ("Address", ctypes.c_uint64),
                ("Register", wintypes.ULONG),
                ("Scope", wintypes.ULONG),
                ("Tag", wintypes.ULONG),
                ("NameLen", wintypes.ULONG),
                ("MaxNameLen", wintypes.ULONG),
                ("Name", ctypes.c_wchar * 512),
            ]

        class IMAGEHLP_LINEW64(ctypes.Structure):
            _fields_ = [
                ("SizeOfStruct", wintypes.DWORD),
                ("Key", ctypes.c_void_p),
                ("LineNumber", wintypes.DWORD),
                ("FileName", wintypes.LPCWSTR),
                ("Address", ctypes.c_uint64),
            ]

        sym = SYMBOL_INFOW()
        sym.SizeOfStruct = 88  # sizeof(SYMBOL_INFOW) up to Name[0]
        sym.MaxNameLen = 511
        disp = ctypes.c_uint64(0)
        ok_sym = self._dbghelp.SymFromAddrW(
            self._hproc, ctypes.c_uint64(addr), ctypes.byref(disp), ctypes.byref(sym)
        )
        sym_name = sym.Name if ok_sym else None

        line = IMAGEHLP_LINEW64()
        line.SizeOfStruct = ctypes.sizeof(IMAGEHLP_LINEW64)
        line_disp = wintypes.DWORD(0)
        ok_line = self._dbghelp.SymGetLineFromAddrW64(
            self._hproc, ctypes.c_uint64(addr), ctypes.byref(line_disp), ctypes.byref(line)
        )
        file_name = line.FileName if ok_line else None
        line_no = int(line.LineNumber) if ok_line else None
        return (sym_name, int(disp.value), file_name, line_no)

    def close(self) -> None:
        if self._available:
            self._dbghelp.SymCleanup(self._hproc)
            self._available = False


def parse_minidump(dmp_path: Path) -> Dict[str, object]:
    data = dmp_path.read_bytes()
    if len(data) < 32 or data[:4] != b"MDMP":
        raise ValueError(f"Not a valid Windows minidump: {dmp_path}")

    _, stream_count, stream_dir_rva = struct.unpack_from("<III", data, 4)
    streams: Dict[int, Tuple[int, int]] = {}
    for i in range(stream_count):
        off = stream_dir_rva + i * 12
        stype, ssize, srva = struct.unpack_from("<III", data, off)
        streams[stype] = (ssize, srva)

    mem = MemoryMap(data)
    if STREAM_MEMORY_LIST in streams:
        _, srva = streams[STREAM_MEMORY_LIST]
        count = struct.unpack_from("<I", data, srva)[0]
        for i in range(count):
            va, dsize, drva = struct.unpack_from("<QII", data, srva + 4 + i * 16)
            mem.add_range(va, dsize, drva)
    if STREAM_MEMORY64_LIST in streams:
        _, srva = streams[STREAM_MEMORY64_LIST]
        count, base_rva = struct.unpack_from("<QQ", data, srva)
        cur_rva = base_rva
        for i in range(count):
            va, dsize = struct.unpack_from("<QQ", data, srva + 16 + i * 16)
            mem.add_range(va, dsize, cur_rva)
            cur_rva += dsize
    mem.finalize()

    modules = []
    if STREAM_MODULE_LIST in streams:
        _, srva = streams[STREAM_MODULE_LIST]
        count = struct.unpack_from("<I", data, srva)[0]
        for i in range(count):
            moff = srva + 4 + i * 108
            base_addr, image_size = struct.unpack_from("<QI", data, moff)
            name_rva = struct.unpack_from("<I", data, moff + 20)[0]
            modules.append({
                "base": base_addr,
                "size": image_size,
                "name": read_mdmp_string(data, name_rva),
            })

    threads = {}
    if STREAM_THREAD_LIST in streams:
        _, srva = streams[STREAM_THREAD_LIST]
        count = struct.unpack_from("<I", data, srva)[0]
        for i in range(count):
            toff = srva + 4 + i * 48
            tid, suspend_count, prio_class, prio, teb = struct.unpack_from("<IIIIQ", data, toff)
            stack_start, stack_size, stack_rva = struct.unpack_from("<QII", data, toff + 24)
            ctx_size, ctx_rva = struct.unpack_from("<II", data, toff + 40)
            ctx = {}
            if ctx_size >= 256 and ctx_rva + ctx_size <= len(data):
                # AMD64 CONTEXT: Rsp=0x98, Rbp=0xA0, Rip=0xF8
                ctx["rsp"] = struct.unpack_from("<Q", data, ctx_rva + 0x98)[0]
                ctx["rbp"] = struct.unpack_from("<Q", data, ctx_rva + 0xA0)[0]
                ctx["rip"] = struct.unpack_from("<Q", data, ctx_rva + 0xF8)[0]
            threads[tid] = {
                "tid": tid,
                "stack_start": stack_start,
                "stack_size": stack_size,
                "ctx": ctx,
            }

    exception = None
    if STREAM_EXCEPTION in streams:
        _, srva = streams[STREAM_EXCEPTION]
        exc_tid = struct.unpack_from("<I", data, srva)[0]
        # MINIDUMP_EXCEPTION starts at srva + 8
        exc_code, exc_flags = struct.unpack_from("<II", data, srva + 8)
        exc_addr = struct.unpack_from("<Q", data, srva + 24)[0]
        num_params = struct.unpack_from("<I", data, srva + 32)[0]
        params = list(struct.unpack_from("<15Q", data, srva + 40))[: min(num_params, 15)]
        exception = {
            "tid": exc_tid,
            "code": exc_code,
            "flags": exc_flags,
            "address": exc_addr,
            "params": params,
        }

    return {"memory": mem, "modules": modules, "threads": threads, "exception": exception}


def find_module(modules: List[dict], addr: int) -> Optional[dict]:
    for m in modules:
        if m["base"] <= addr < m["base"] + m["size"]:
            return m
    return None


def recover_cpp_exception_strings(mem: MemoryMap, params: List[int]) -> List[str]:
    """Recover type/what() strings from MSVC 0xE06D7363 parameters or object memory."""
    found: List[str] = []
    if len(params) < 3:
        return found
    obj_va = params[1]
    throw_info_va = params[2]
    img_base = params[3] if len(params) >= 4 else 0

    # Check direct pointer fields inside std::exception / std::runtime_error object (e.g. _Data._What at +8)
    for off in (8, 16, 24):
        ptr = mem.read_u64(obj_va + off)
        if ptr and ptr > 0x10000:
            s = mem.read_cstring(ptr)
            if s and len(s) >= 3:
                found.append(f"object+{off}: {s}")

    # Check RTTI TypeDescriptor name via _ThrowInfo -> _CatchableTypeArray -> _CatchableType -> pType
    if img_base and throw_info_va:
        cta_rva = mem.read_u32(throw_info_va + 12)
        if cta_rva:
            n_types = mem.read_u32(img_base + cta_rva) or 0
            for idx in range(min(n_types, 4)):
                ct_rva = mem.read_u32(img_base + cta_rva + 4 + idx * 4)
                if ct_rva:
                    td_rva = mem.read_u32(img_base + ct_rva + 4)
                    if td_rva:
                        name = mem.read_cstring(img_base + td_rva + 16)
                        if name:
                            found.append(f"rtti[{idx}]: {name}")
    return found


def format_addr(addr: int, modules: List[dict], resolver: DbgHelpResolver) -> str:
    mod = find_module(modules, addr)
    mod_name = Path(mod["name"]).name if mod else "<unknown>"
    mod_off = addr - mod["base"] if mod else 0
    sym_name, disp, file_name, line_no = resolver.resolve(addr)
    parts = [f"0x{addr:016X}", f"{mod_name}+0x{mod_off:X}"]
    if sym_name:
        parts.append(f"{sym_name}+0x{disp:X}")
    if file_name and line_no:
        parts.append(f"({Path(file_name).name}:{line_no})")
    return "  ".join(parts)


def analyze_target(target: Path, pdb_dirs: List[str]) -> int:
    report_path: Optional[Path] = None
    dmp_path: Optional[Path] = None
    run_log_path: Optional[Path] = None
    input_rec_path: Optional[Path] = None

    if target.is_dir():
        reports = sorted(target.glob("crash_report_*.txt"))
        dumps = sorted(target.glob("crash_dump_*.dmp"))
        report_path = reports[-1] if reports else None
        dmp_path = dumps[-1] if dumps else None
        if (target / "run.log").exists():
            run_log_path = target / "run.log"
        if (target / "input_recording.txt").exists():
            input_rec_path = target / "input_recording.txt"
    elif target.suffix.lower() == ".dmp":
        dmp_path = target
    elif target.suffix.lower() == ".txt":
        report_path = target
        dumps = sorted(target.parent.glob("crash_dump_*.dmp"))
        dmp_path = dumps[-1] if dumps else None
    else:
        print(f"Unsupported target: {target}", file=sys.stderr)
        return 1

    if report_path:
        print("=== Crash Report ===")
        print(report_path.read_text(encoding="utf-8", errors="replace").rstrip())
        print()

    if not dmp_path:
        print("No minidump (.dmp) found.")
        return 0 if report_path else 1

    parsed = parse_minidump(dmp_path)
    modules: List[dict] = parsed["modules"]  # type: ignore[assignment]
    threads: Dict[int, dict] = parsed["threads"]  # type: ignore[assignment]
    exc: Optional[dict] = parsed["exception"]  # type: ignore[assignment]
    mem: MemoryMap = parsed["memory"]  # type: ignore[assignment]

    search_dirs = list(pdb_dirs)
    for m in modules:
        p = str(Path(m["name"]).parent)
        if p and p not in search_dirs and Path(p).exists():
            search_dirs.append(p)

    resolver = DbgHelpResolver(";".join(search_dirs))
    try:
        for m in modules:
            mod_path = m["name"]
            if not Path(mod_path).exists():
                for d in pdb_dirs:
                    cand = Path(d) / Path(mod_path).name
                    if cand.exists():
                        mod_path = str(cand)
                        break
            resolver.load_module(mod_path, m["base"], m["size"])

        print(f"=== Minidump ({dmp_path.name}) ===")
        if exc:
            code = exc["code"]
            cname = EXCEPTION_NAMES.get(code, "UNKNOWN_EXCEPTION")
            print(f"Crashing TID:   {exc['tid']}")
            print(f"Exception:      0x{code:08X} ({cname})")
            print(f"Fault Address:  {format_addr(exc['address'], modules, resolver)}")
            if code == 0xE06D7363:
                recovered = recover_cpp_exception_strings(mem, exc["params"])
                if recovered:
                    print("C++ Exception Info:")
                    for item in recovered:
                        print(f"  {item}")

            tinfo = threads.get(exc["tid"])
            if tinfo:
                ctx = tinfo["ctx"]
                if ctx:
                    print(f"Registers:      RIP=0x{ctx['rip']:016X}  RSP=0x{ctx['rsp']:016X}  RBP=0x{ctx['rbp']:016X}")
                    if ctx["rip"] and ctx["rip"] != exc["address"]:
                        print(f"RIP Symbol:     {format_addr(ctx['rip'], modules, resolver)}")
                if ctx and ctx["rsp"]:
                    print("Stack Return-Address Scan:")
                    seen = 0
                    for slot in range(0, 16384, 8):
                        val = mem.read_u64(ctx["rsp"] + slot)
                        if val is None:
                            break
                        mod = find_module(modules, val)
                        if mod:
                            mod_base_name = Path(mod["name"]).name.lower()
                            is_app = mod_base_name.endswith(".exe") or "repsp" in mod_base_name
                            sym_name, disp, file_name, line_no = resolver.resolve(val)
                            if is_app or (seen < 8 and sym_name):
                                print(f"  [rsp+0x{slot:03X}] {format_addr(val, modules, resolver)}")
                                seen += 1
                                if seen >= 30:
                                    break
        else:
            print("No ExceptionStream in minidump (process dump or manual dump).")
    finally:
        resolver.close()

    if run_log_path:
        lines = run_log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        print(f"\n=== Run Log Tail ({len(lines)} total lines) ===")
        for line in lines[-15:]:
            print(f"  {line}")

    if input_rec_path:
        lines = input_rec_path.read_text(encoding="utf-8", errors="replace").splitlines()
        print(f"\n=== Input Recording ({len(lines)} lines) ===")
        for line in lines[:10]:
            print(f"  {line}")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze reDMP1 crash reports and Windows minidumps.")
    parser.add_argument("target", help="Crash run directory, crash_report_*.txt, or .dmp file")
    parser.add_argument("--pdb-dir", action="append", default=[], help="Additional directory to search for PDB/EXE files")
    args = parser.parse_args()
    return analyze_target(Path(args.target), args.pdb_dir)


if __name__ == "__main__":
    raise SystemExit(main())
