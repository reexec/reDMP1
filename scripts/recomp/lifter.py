#!/usr/bin/env python3
"""Wrapper around the allegrexrecomp host tool."""

import os
import sys
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent

def find_allegrexrecomp() -> Path:
    env_bin = os.environ.get("ALLEGREXRECOMP")
    if env_bin:
        p = Path(env_bin)
        if p.is_file():
            return p

    candidates = [
        REPO_ROOT / "build" / "win64-debug" / "Debug" / "allegrexrecomp.exe",
        REPO_ROOT / "build" / "win64-release" / "Release" / "allegrexrecomp.exe",
        REPO_ROOT / "build" / "win64-debug" / "allegrexrecomp.exe",
        REPO_ROOT / "build" / "win64-release" / "allegrexrecomp.exe",
        REPO_ROOT / "build" / "linux-x64-debug" / "allegrexrecomp",
        REPO_ROOT / "build" / "linux-x64-release" / "allegrexrecomp",
    ]

    for c in candidates:
        if c.is_file():
            return c

    # Search in PATH
    which = sys.platform == "win32" and "where" or "which"
    try:
        out = subprocess.check_output([which, "allegrexrecomp"], text=True).strip().splitlines()
        if out:
            p = Path(out[0])
            if p.is_file():
                return p
    except Exception:
        pass

    raise FileNotFoundError(
        "Could not find allegrexrecomp binary. Build the project first with cmake --preset <preset>."
    )

def run_allegrexrecomp(args: list[str], capture_output: bool = False) -> subprocess.CompletedProcess:
    exe = find_allegrexrecomp()
    cmd = [str(exe)] + args
    return subprocess.run(cmd, capture_output=capture_output, text=True, check=False)

if __name__ == "__main__":
    try:
        proc = run_allegrexrecomp(sys.argv[1:])
        sys.exit(proc.returncode)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
