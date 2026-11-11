#!/usr/bin/env python3
"""Run allegrexrecomp to emit lifted C code into src/game/dmp1/recomp/generated/."""

import argparse
import sys
from pathlib import Path

from lifter import run_allegrexrecomp

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUTDIR = REPO_ROOT / "src" / "game" / "dmp1" / "recomp" / "generated"

def select_lift_input(game_dir: Path) -> tuple[Path, str]:
    boot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "BOOT.BIN"
    eboot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "EBOOT.BIN"

    if boot_bin.is_file():
        # Check if it's plain ELF
        with open(boot_bin, "rb") as f:
            magic = f.read(4)
        if magic == b"\x7fELF":
            return boot_bin, "BOOT.BIN (plain ELF)"
    
    if eboot_bin.is_file():
        return eboot_bin, "EBOOT.BIN"

    if boot_bin.is_file():
        return boot_bin, "BOOT.BIN"

    raise FileNotFoundError(f"No executable found in {game_dir / 'PSP_GAME' / 'SYSDIR'}")

def main():
    parser = argparse.ArgumentParser(description="Lift PSP executable to C code")
    parser.add_argument("game_dir", nargs="?", default="roms", help="Path to extracted game directory (default: roms)")
    parser.add_argument("--outdir", default=str(DEFAULT_OUTDIR), help="Output directory for generated C files")
    parser.add_argument("--prefix", default="dmp1", help="Symbol/file prefix (default: dmp1)")
    parser.add_argument("--info-only", action="store_true", help="Only run allegrexrecomp info")
    parser.add_argument("--funcs-only", action="store_true", help="Only run allegrexrecomp funcs")
    args = parser.parse_args()

    game_dir = Path(args.game_dir).resolve()
    try:
        input_path, desc = select_lift_input(game_dir)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Selected lift input: {input_path} [{desc}]")

    if args.info_only:
        proc = run_allegrexrecomp(["info", str(input_path)])
        sys.exit(proc.returncode)

    if args.funcs_only:
        proc = run_allegrexrecomp(["funcs", str(input_path)])
        sys.exit(proc.returncode)

    outdir = Path(args.outdir).resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    print(f"Emitting C code to: {outdir} (prefix: {args.prefix})")
    proc = run_allegrexrecomp(["emit", str(input_path), str(outdir), args.prefix])
    if proc.returncode != 0:
        print(f"allegrexrecomp emit failed with exit code {proc.returncode}", file=sys.stderr)
        sys.exit(proc.returncode)

    print("\nGeneration finished successfully.")

if __name__ == "__main__":
    main()
