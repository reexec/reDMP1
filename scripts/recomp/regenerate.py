#!/usr/bin/env python3
"""Run allegrexrecomp to emit lifted C code into src/game/dmp1/recomp/generated/."""

import argparse
import json
import sys
from pathlib import Path

from lifter import run_allegrexrecomp

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUTDIR = REPO_ROOT / "src" / "game" / "dmp1" / "recomp" / "generated"
DEFAULT_LIFT_CONFIG = REPO_ROOT / "src" / "game" / "dmp1" / "recomp" / "lift.json"

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
    parser.add_argument("--config", default=str(DEFAULT_LIFT_CONFIG), help="Path to lift.json config")
    parser.add_argument("--outdir", default=str(DEFAULT_OUTDIR), help="Output directory for generated C files")
    parser.add_argument("--prefix", default=None, help="Symbol/file prefix (defaults to lift.json prefix)")
    parser.add_argument("--base", default=None, help="Base address for PRX relocation (defaults to lift.json base)")
    parser.add_argument("--info-only", action="store_true", help="Only run allegrexrecomp info")
    parser.add_argument("--funcs-only", action="store_true", help="Only run allegrexrecomp funcs")
    args = parser.parse_args()

    base = args.base
    prefix = args.prefix
    config_path = Path(args.config)
    if config_path.is_file():
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                cfg = json.load(f)
                if not base and "base" in cfg:
                    base = cfg["base"]
                if not prefix and "prefix" in cfg:
                    prefix = cfg["prefix"]
        except Exception as e:
            print(f"Warning: could not read {config_path}: {e}", file=sys.stderr)

    if not prefix:
        prefix = "dmp1"

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
        cmd = ["funcs", str(input_path)]
        if base:
            cmd.extend(["--base", base])
        proc = run_allegrexrecomp(cmd)
        sys.exit(proc.returncode)

    outdir = Path(args.outdir).resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    print(f"Emitting C code to: {outdir} (prefix: {prefix}, base: {base or '0x00000000'})")
    cmd = ["emit", str(input_path), str(outdir), prefix]
    if base:
        cmd.extend(["--base", base])
    proc = run_allegrexrecomp(cmd)
    if proc.returncode != 0:
        print(f"allegrexrecomp emit failed with exit code {proc.returncode}", file=sys.stderr)
        sys.exit(proc.returncode)

    print("\nGeneration finished successfully.")

if __name__ == "__main__":
    main()
