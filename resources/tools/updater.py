#!/usr/bin/env python3
#
#  Copyright 2026 - RiiK
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

"""PG2-Cheetos Project Updater, Auto-Healer, Doctor & AOB Workflow CLI.

Subcommands:
  sync      (default) Sync config.json metadata & verify Tier 1 AOBs.
  doctor    Full 9-point read-only health check.
  autoheal  Post-patch recovery: optional dump, config.json sync, broken AOB
            regeneration, full offsets.h sync, and optional rebuild.
  gen-aob   Generate & verify a unique Tier 1 AOB for an RVA or Class::Method.
"""

import argparse
from pathlib import Path
import re
import sys

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import aob_gen
import dump_parser
from lib.console import err, header, info, ok
from lib.doctor import run_doctor
from lib.game_env import find_project_root, locate_game
from lib.healer import run_sync_or_heal
from lib.registry import GAME_ASSEMBLY_NAME


def run_gen_aob(args: argparse.Namespace) -> int:
    header("PG2-Cheetos AOB Generator")
    root = find_project_root()
    dump_path = (
        Path(args.dump_file) if args.dump_file else (root / "resources/dumped/dump.cs")
    )

    try:
        game_dir, _ = locate_game(args.game_dir)
    except FileNotFoundError as exc:
        err(str(exc))
        return 1

    asm_path = game_dir / GAME_ASSEMBLY_NAME
    pe_bytes = asm_path.read_bytes()
    image_base, exec_sections = aob_gen.get_pe_image_base_and_exec_sections(pe_bytes)

    target = args.target.strip()
    if re.fullmatch(r"0x[0-9A-Fa-f]+", target):
        rva = int(target, 16)
        label = target
    elif "::" in target:
        cls_name, method_name = target.split("::", 1)
        if not dump_path.is_file():
            err(f"dump.cs required for symbol lookup, not found at {dump_path}")
            return 1
        parsed = dump_parser.parse_dump_targets(dump_path, {("", cls_name)})
        t = parsed.get(("", cls_name))
        if not t:
            err(f"Class '{cls_name}' not found in {dump_path}")
            return 1
        me = dump_parser.find_method_in_type(
            t, method_name, args.arg_count, args.param_filter
        )
        if not me:
            err(f"Method '{cls_name}::{method_name}' not found in {dump_path}")
            return 1
        rva = me.rva
        label = f"{cls_name}::{method_name} (0x{rva:X})"
    else:
        err(
            "Target must be a hex RVA (e.g. 0x1223510) or Class::Method (e.g. PlayerVisuals::Present)."
        )
        return 1

    info(f"Generating unique AOB for {label}...")
    pat, count = aob_gen.generate_unique_aob(
        asm_path, rva, exec_sections, image_base=image_base, max_bytes=args.max_bytes
    )
    if pat and count == 1:
        ok(f"UNIQUE (1 match) at 0x{rva:X} ({len(pat.split())} bytes):")
        print(f'\n    "{pat}"\n')
        return 0

    err(
        f"Could not generate a unique AOB at 0x{rva:X} (hits={count} within {args.max_bytes} bytes)."
    )
    return 1


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="PG2-Cheetos Project Updater, Auto-Healer, Doctor & AOB Generator."
    )
    parser.add_argument(
        "--game-dir", help="Override Pixel Gun 2 installation directory."
    )
    parser.add_argument("--dump-file", help="Override path to IL2CPP dump.cs.")

    sub = parser.add_subparsers(dest="command")

    p_sync = sub.add_parser(
        "sync", help="Sync config.json metadata & verify Tier 1 AOBs (default)."
    )
    p_sync.add_argument(
        "--check", action="store_true", help="Dry-run without editing files."
    )
    p_sync.add_argument(
        "--sync-offsets",
        action="store_true",
        help="Sync shifted RVAs & offsets into offsets.h.",
    )
    p_sync.add_argument(
        "--build", action="store_true", help="Run ./resources/scripts/build after sync."
    )
    p_sync.add_argument(
        "--debug", action="store_true", help="Build in Debug mode (with --build)."
    )
    p_sync.add_argument(
        "--no-injector", action="store_true", help="Skip injector build (with --build)."
    )

    p_doc = sub.add_parser("doctor", help="Full 9-point read-only health check.")
    p_doc.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON report (for CI/scripts).",
    )

    p_heal = sub.add_parser(
        "autoheal",
        help="Full post-patch recovery: dump, sync config.json, regenerate broken AOBs, sync offsets.h, optional build.",
    )
    p_heal.add_argument(
        "--dump",
        action="store_true",
        help="Run IL2CPP dumper if GameAssembly.dll changed.",
    )
    p_heal.add_argument(
        "--dumper-cmd", help="Dumper command template (or set PG2_DUMPER_CMD)."
    )
    p_heal.add_argument(
        "--check", action="store_true", help="Dry-run without editing files."
    )
    p_heal.add_argument(
        "--build",
        action="store_true",
        help="Run ./resources/scripts/build after healing.",
    )
    p_heal.add_argument(
        "--debug", action="store_true", help="Build in Debug mode (with --build)."
    )
    p_heal.add_argument(
        "--no-injector", action="store_true", help="Skip injector build (with --build)."
    )

    p_aob = sub.add_parser(
        "gen-aob",
        help="Generate & verify a unique Tier 1 AOB for an RVA or Class::Method.",
    )
    p_aob.add_argument(
        "target",
        help="Hex RVA (e.g. 0x1223510) or Class::Method (e.g. PlayerVisuals::Present).",
    )
    p_aob.add_argument(
        "--arg-count", type=int, default=-1, help="Filter method overload by arg count."
    )
    p_aob.add_argument(
        "--param-filter", help="Filter method overload by parameter type substring."
    )
    p_aob.add_argument(
        "--max-bytes",
        type=int,
        default=256,
        help="Max disassembly bytes (default: 256).",
    )

    return parser


def main() -> int:
    argv = sys.argv[1:]
    subcommands = {"sync", "doctor", "autoheal", "gen-aob", "-h", "--help"}
    if not any(a in subcommands for a in argv):
        argv = ["sync", *argv]

    args = build_arg_parser().parse_args(argv)
    if args.command == "doctor":
        return run_doctor(args)
    if args.command == "autoheal":
        return run_sync_or_heal(args, autoheal=True)
    if args.command == "gen-aob":
        return run_gen_aob(args)
    return run_sync_or_heal(args, autoheal=False)


if __name__ == "__main__":
    sys.exit(main())
