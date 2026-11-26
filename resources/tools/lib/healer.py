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

"""Sync and Auto-Heal workflows (`updater.py sync` / `updater.py autoheal`)."""

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
from typing import Dict

import aob_gen
import dump_parser

from .console import err, header, info, ok, warn
from .game_env import (
    compute_sha256,
    extract_exe_version,
    extract_steam_build_id,
    find_project_root,
    locate_game,
)
from .registry import (
    GAME_ASSEMBLY_NAME,
    GAME_EXE_NAME,
    METADATA_REL_PATH,
    TRACKED_ENUMS,
    TRACKED_FIELDS,
    TRACKED_METHODS,
)
from .source_sync import (
    append_enum_members,
    find_used_signatures,
    load_tracked_dump,
    parse_cpp_enum,
    parse_offsets_h,
    patch_offsets_h,
)


def _run_dumper(dumper_cmd: str, game_dir: Path, dump_dir: Path) -> bool:
    asm = game_dir / GAME_ASSEMBLY_NAME
    meta = game_dir / METADATA_REL_PATH
    dump_dir.mkdir(parents=True, exist_ok=True)
    formatted = dumper_cmd.format(
        game_dir=str(game_dir),
        assembly=str(asm),
        metadata=str(meta),
        out_dir=str(dump_dir),
    )
    info(f"Running IL2CPP dumper: {formatted}")
    return subprocess.run(shlex.split(formatted)).returncode == 0


def _run_build(root: Path, debug: bool, with_injector: bool) -> int:
    build_script = root / "resources/scripts/build"
    if not build_script.is_file():
        err(f"Build script not found: {build_script}")
        return 1
    cmd = [str(build_script), "debug" if debug else "release"]
    if with_injector:
        cmd.append("--injector")
    info(f"Running build: {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=str(root)).returncode


def run_sync_or_heal(args: argparse.Namespace, autoheal: bool) -> int:
    header(
        "PG2-Cheetos Auto-Heal Workflow" if autoheal else "PG2-Cheetos Sync Workflow"
    )
    root = find_project_root()
    config_path = root / "config.json"
    offsets_h = root / "src/utils/offsets.h"
    dump_dir = root / "resources/dumped"
    dump_path = Path(args.dump_file) if args.dump_file else (dump_dir / "dump.cs")

    cfg = json.loads(config_path.read_text(encoding="utf-8"))

    try:
        game_dir, manifest = locate_game(args.game_dir)
    except FileNotFoundError as exc:
        err(str(exc))
        return 1

    ok(f"Game directory: {game_dir}")

    exe_path = game_dir / GAME_EXE_NAME
    asm_path = game_dir / GAME_ASSEMBLY_NAME
    if not asm_path.is_file():
        err(f"{GAME_ASSEMBLY_NAME} not found in {game_dir}")
        return 1

    game_version = extract_exe_version(exe_path)
    build_id = extract_steam_build_id(manifest)
    sha256 = compute_sha256(asm_path)
    sha_changed = sha256 != cfg.get("assembly_sha256", "")

    info(
        f"Game version   : {game_version or '(unknown)'} (was: {cfg.get('version', '')})"
    )
    info(
        f"Steam build ID : {build_id or '(unknown)'} (was: {cfg.get('steam-build-id', '')})"
    )
    info(
        f"Assembly SHA256: {sha256[:16]}... ({'CHANGED' if sha_changed else 'unchanged'})"
    )

    # Optional IL2CPP dump refresh
    if getattr(args, "dump", False):
        dumper_cmd = getattr(args, "dumper_cmd", None) or os.environ.get(
            "PG2_DUMPER_CMD", ""
        )
        needs_dump = (
            sha_changed
            or not dump_path.is_file()
            or dump_path.stat().st_mtime < asm_path.stat().st_mtime
        )
        if not needs_dump:
            info("dump.cs is already up to date with GameAssembly.dll.")
        elif not dumper_cmd:
            warn(
                "--dump requested, but no dumper command configured. Set PG2_DUMPER_CMD or pass "
                '--dumper-cmd "Il2CppDumper {assembly} {metadata} {out_dir}".'
            )
        else:
            if not _run_dumper(dumper_cmd, game_dir, dump_dir):
                err("IL2CPP dumper failed.")
                return 1
            ok("IL2CPP dump refreshed.")

    pe_bytes = asm_path.read_bytes()
    image_base, exec_sections = aob_gen.get_pe_image_base_and_exec_sections(pe_bytes)
    _, _, _, header_sigs = parse_offsets_h(offsets_h)
    existing_sigs = cfg.get("signatures", {})
    all_sig_names = list(dict.fromkeys([*header_sigs.keys(), *existing_sigs.keys()]))
    used_sigs = find_used_signatures(root / "src", all_sig_names)

    dump_types = load_tracked_dump(dump_path) if dump_path.is_file() else {}
    if autoheal and not dump_types:
        warn(f"dump.cs not found at {dump_path}; autoheal will only use AOB scans.")

    synced_rvas: Dict[str, int] = {}
    synced_offs: Dict[str, int] = {}
    healed_sigs: Dict[str, str] = {}

    if dump_types and (autoheal or getattr(args, "sync_offsets", False)):
        for const_name, (ns, cls, mname, argc, psub) in TRACKED_METHODS.items():
            t = dump_types.get((ns, cls))
            if not t:
                if autoheal:
                    warn(f"dump.cs missing type {ns}.{cls} for {const_name}")
                continue
            me = dump_parser.find_method_in_type(t, mname, argc, psub)
            if me:
                synced_rvas[const_name] = me.rva
            elif autoheal:
                warn(
                    f"dump.cs missing method {cls}::{mname}({argc} args) for {const_name}"
                )

        for const_name, (ns, cls, fname) in TRACKED_FIELDS.items():
            t = dump_types.get((ns, cls))
            if t and fname in t.fields:
                synced_offs[const_name] = t.fields[fname]
            elif autoheal:
                warn(f"dump.cs missing field {cls}::{fname} for {const_name}")

    info(
        f"Scanning {len(all_sig_names)} Tier 1 AOB signature(s) in {GAME_ASSEMBLY_NAME}..."
    )
    new_sigs = {}
    ok_count = 0
    broken_count = 0
    used_count = 0
    dormant_count = 0

    for name in all_sig_names:
        pattern = header_sigs.get(name) or existing_sigs.get(name, {}).get(
            "pattern", ""
        )
        if name in used_sigs:
            used_count += 1
        else:
            dormant_count += 1

        hits = (
            aob_gen.scan_pattern(exec_sections, pattern, max_hits=3) if pattern else []
        )
        if len(hits) == 1:
            rva = hits[0]
            ok_count += 1
            synced_rvas[name] = rva
            rva_hex = f"0x{rva:X}"
            prev_rva = existing_sigs.get(name, {}).get("rva", "")
            delta = (
                f" (shifted from {prev_rva})"
                if prev_rva and prev_rva.upper() != rva_hex.upper()
                else ""
            )
            ok(f"  [OK]     {name:44s} -> {rva_hex}{delta}")
            new_sigs[name] = {"rva": rva_hex, "pattern": pattern, "status": "ok"}
            continue

        if autoheal and name in synced_rvas:
            target_rva = synced_rvas[name]
            info(
                f"  [HEAL]   {name}: broken AOB, regenerating from dump RVA 0x{target_rva:X}..."
            )
            regen_pat, regen_hits = aob_gen.generate_unique_aob(
                asm_path, target_rva, exec_sections, image_base=image_base
            )
            if regen_pat and regen_hits == 1:
                ok_count += 1
                healed_sigs[name] = regen_pat
                rva_hex = f"0x{target_rva:X}"
                ok(
                    f"  [HEALED] {name:44s} -> {rva_hex} ({len(regen_pat.split())} bytes)"
                )
                new_sigs[name] = {"rva": rva_hex, "pattern": regen_pat, "status": "ok"}
                continue
            warn(
                f"  [FAIL]   {name}: could not auto-generate unique AOB (hits={regen_hits})"
            )

        broken_count += 1
        reason = "0 matches" if len(hits) == 0 else f"{len(hits)}+ ambiguous matches"
        warn(f"  [BROKEN] {name:44s} -> {reason}")
        new_sigs[name] = {
            "rva": existing_sigs.get(name, {}).get("rva", ""),
            "pattern": pattern,
            "status": "broken",
        }

    enum_appends = []
    if autoheal and dump_types:
        for rel_src, cpp_enum, ns, dump_enum in TRACKED_ENUMS:
            t = dump_types.get((ns, dump_enum))
            if not t:
                continue
            cpp_vals = parse_cpp_enum(root / rel_src, cpp_enum)
            mismatched = [
                k
                for k, v in cpp_vals.items()
                if k in t.enum_values and t.enum_values[k] != v
            ]
            new_vals = [(k, v) for k, v in t.enum_values.items() if k not in cpp_vals]
            if mismatched:
                warn(
                    f"Enum value mismatch in {rel_src} ({cpp_enum}): {mismatched} (manual review required)"
                )
            elif new_vals:
                enum_appends.append((rel_src, cpp_enum, new_vals))
                info(
                    f"New enum values in dump.cs for {cpp_enum}: {[k for k, _ in new_vals]}"
                )

    if game_version:
        cfg["version"] = game_version
    cfg.pop("cheetos-version", None)
    if build_id:
        cfg["steam-build-id"] = build_id
    cfg["assembly_sha256"] = sha256
    cfg["signatures_summary"] = {
        "total": len(all_sig_names),
        "ok": ok_count,
        "broken": broken_count,
        "used": used_count,
        "dormant": dormant_count,
    }
    cfg["signatures"] = new_sigs

    print("")
    info(
        f"Summary: total={len(all_sig_names)}, ok={ok_count}, broken={broken_count}, "
        f"used={used_count}, dormant={dormant_count}"
    )

    if getattr(args, "check", False):
        info("--check mode: no files were modified.")
        return 1 if broken_count > 0 else 0

    config_path.write_text(json.dumps(cfg, indent="\t") + "\n", encoding="utf-8")
    ok(f"Updated {config_path.relative_to(root)}")

    if autoheal or getattr(args, "sync_offsets", False):
        rva_c, off_c, sig_c = patch_offsets_h(
            offsets_h, synced_rvas, synced_offs, healed_sigs
        )
        if rva_c or off_c or sig_c:
            ok(
                f"Patched {offsets_h.relative_to(root)}: "
                f"{rva_c} RVA(s), {off_c} field offset(s), {sig_c} regenerated AOB(s)."
            )
        else:
            info(f"All entries in {offsets_h.relative_to(root)} already up to date.")

        for rel_src, cpp_enum, new_vals in enum_appends:
            added = append_enum_members(root / rel_src, cpp_enum, new_vals)
            if added:
                ok(f"Appended {added} new member(s) to {cpp_enum} in {rel_src}")

    if getattr(args, "build", False):
        if broken_count > 0:
            warn("Skipping --build because broken signatures remain.")
            return 1
        return _run_build(
            root,
            debug=getattr(args, "debug", False),
            with_injector=not getattr(args, "no_injector", False),
        )

    return 1 if broken_count > 0 else 0
