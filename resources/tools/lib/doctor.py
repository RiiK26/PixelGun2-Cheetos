#!/usr/bin/env pyhton3
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

"""Read-only 9-point project health check (`updater.py doctor [--json]`)."""

import argparse
import json
import os
from pathlib import Path
import shutil
from typing import Any, Dict

import aob_gen
import dump_parser

from .console import err, header, info, ok, section, warn
from .game_env import (
    check_il2cpp_exports,
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
    REQUIRED_IL2CPP_EXPORTS,
    TRACKED_ENUMS,
    TRACKED_FIELDS,
    TRACKED_METHODS,
)
from .source_sync import (
    find_used_signatures,
    load_tracked_dump,
    parse_cpp_enum,
    parse_offsets_h,
)


def run_doctor(args: argparse.Namespace) -> int:
    as_json = getattr(args, "json", False)
    if not as_json:
        header("PG2-Cheetos Doctor")

    root = find_project_root()
    config_path = root / "config.json"
    offsets_h = root / "src/utils/offsets.h"
    dump_path = (
        Path(args.dump_file) if args.dump_file else (root / "resources/dumped/dump.cs")
    )

    issues = 0
    warnings = 0
    report: Dict[str, Any] = {
        "toolchain": {},
        "game_files": {},
        "config_metadata": {},
        "il2cpp_exports": {},
        "signatures": {},
        "methods": {},
        "fields": {},
        "enums": {},
        "artifacts": {},
    }

    # 1. Toolchain
    if not as_json:
        section("1. Toolchain & Environment")
    for tool in ("cmake", "ninja", "x86_64-w64-mingw32-g++", "objdump"):
        path = shutil.which(tool)
        report["toolchain"][tool] = path or None
        if path:
            if not as_json:
                ok(f"{tool:24s} -> {path}")
        else:
            if not as_json:
                err(f"{tool:24s} -> MISSING")
            issues += 1

    dumper_env = os.environ.get("PG2_DUMPER_CMD", "")
    report["toolchain"]["PG2_DUMPER_CMD"] = dumper_env or None
    if not as_json:
        if dumper_env:
            ok(f"PG2_DUMPER_CMD           -> {dumper_env}")
        else:
            info(
                "PG2_DUMPER_CMD           -> (not set; optional for --dump auto-refresh)"
            )

    # 2. Game installation & dump freshness
    if not as_json:
        section("2. Game Files & Dump Freshness")
    try:
        game_dir, manifest = locate_game(args.game_dir)
        report["game_files"]["game_dir_found"] = True
        if not as_json:
            ok(f"Game directory           -> {game_dir}")
    except FileNotFoundError as exc:
        report["game_files"]["game_dir_found"] = False
        report["error"] = str(exc)
        if as_json:
            print(
                json.dumps(
                    {
                        "healthy": False,
                        "issues": issues + 1,
                        "warnings": warnings,
                        **report,
                    },
                    indent=2,
                )
            )
        else:
            err(str(exc))
        return 1

    exe_path = game_dir / GAME_EXE_NAME
    asm_path = game_dir / GAME_ASSEMBLY_NAME
    meta_path = game_dir / METADATA_REL_PATH
    for label, p in [
        (GAME_EXE_NAME, exe_path),
        (GAME_ASSEMBLY_NAME, asm_path),
        ("global-metadata.dat", meta_path),
        ("dump.cs", dump_path),
    ]:
        exists = p.is_file()
        report["game_files"][label] = exists
        if exists:
            if not as_json:
                ok(f"{label:24s} -> OK")
        else:
            if not as_json:
                err(f"{label:24s} -> MISSING ({p})")
            issues += 1

    if dump_path.is_file() and asm_path.is_file():
        stale = dump_path.stat().st_mtime < asm_path.stat().st_mtime
        report["game_files"]["dump_stale"] = stale
        if stale:
            if not as_json:
                warn("dump.cs is OLDER than GameAssembly.dll — re-dump recommended!")
            warnings += 1
        elif not as_json:
            ok("dump.cs freshness        -> newer than GameAssembly.dll")

    # 3. config.json metadata drift
    if not as_json:
        section("3. config.json Metadata")
    cfg = json.loads(config_path.read_text(encoding="utf-8"))
    live_ver = extract_exe_version(exe_path)
    live_bid = extract_steam_build_id(manifest)
    live_sha = compute_sha256(asm_path) if asm_path.is_file() else ""

    for key, live in [
        ("version", live_ver),
        ("steam-build-id", live_bid),
        ("assembly_sha256", live_sha),
    ]:
        cur = cfg.get(key, "")
        in_sync = cur == live
        report["config_metadata"][key] = {
            "config": cur,
            "live": live,
            "in_sync": in_sync,
        }
        if in_sync:
            if not as_json:
                ok(f"{key:18s} -> {cur[:24]}{'...' if len(cur) > 24 else ''} (in sync)")
        else:
            if not as_json:
                warn(f"{key:18s} -> DRIFT (config={cur[:16]}, live={live[:16]})")
            warnings += 1

    # 4. IL2CPP Exports
    if not as_json:
        section("4. GameAssembly.dll IL2CPP Exports")
    if asm_path.is_file():
        present, missing = check_il2cpp_exports(asm_path)
        report["il2cpp_exports"] = {
            "present": len(present),
            "total": len(REQUIRED_IL2CPP_EXPORTS),
            "missing": missing,
        }
        if not missing:
            if not as_json:
                ok(
                    f"All {len(present)}/{len(REQUIRED_IL2CPP_EXPORTS)} required il2cpp_* exports present"
                )
        else:
            if not as_json:
                err(f"Missing IL2CPP exports: {', '.join(missing)}")
            issues += len(missing)

    # 5. Tier 1 AOB Signatures
    if not as_json:
        section("5. Tier 1 AOB Signatures")
    _, header_offs, header_rvas, header_sigs = parse_offsets_h(offsets_h)
    used_sigs = find_used_signatures(root / "src", list(header_sigs.keys()))
    pe_bytes = asm_path.read_bytes()
    _, exec_sections = aob_gen.get_pe_image_base_and_exec_sections(pe_bytes)

    for name, pat in header_sigs.items():
        hits = aob_gen.scan_pattern(exec_sections, pat, max_hits=3)
        is_used = name in used_sigs
        used_tag = "used" if is_used else "DORMANT"
        if len(hits) == 1:
            rva_hex = f"0x{hits[0]:X}"
            h_rva = header_rvas.get(name)
            shifted = h_rva is not None and h_rva != hits[0]
            report["signatures"][name] = {
                "status": "shift" if shifted else "ok",
                "rva": rva_hex,
                "header_rva": f"0x{h_rva:X}" if h_rva is not None else None,
                "used": is_used,
            }
            if shifted:
                if not as_json:
                    warn(
                        f"[SHIFT]  {name:44s} -> AOB={rva_hex} vs offsets.h=0x{h_rva:X} ({used_tag})"
                    )
                warnings += 1
            elif not as_json:
                ok(f"[OK]     {name:44s} -> {rva_hex} ({used_tag})")
        else:
            report["signatures"][name] = {
                "status": "broken",
                "matches": len(hits),
                "used": is_used,
            }
            if not as_json:
                err(f"[BROKEN] {name:44s} -> {len(hits)} matches ({used_tag})")
            issues += 1
        if not is_used:
            warnings += 1

    # 6, 7 & 8. Tier 2/3 Methods, Field Offsets & Enum Drift vs dump.cs
    if dump_path.is_file():
        dump_types = load_tracked_dump(dump_path)

        if not as_json:
            section("6. Tier 2 & Tier 3 Methods vs dump.cs")
        for const_name, (ns, cls, mname, argc, psub) in TRACKED_METHODS.items():
            t = dump_types.get((ns, cls))
            if not t:
                report["methods"][const_name] = {
                    "status": "missing_type",
                    "type": f"{ns}.{cls}",
                }
                if not as_json:
                    err(f"[MISSING TYPE]   {const_name}: {ns}.{cls}")
                issues += 1
                continue
            me = dump_parser.find_method_in_type(t, mname, argc, psub)
            if not me:
                report["methods"][const_name] = {
                    "status": "missing_method",
                    "method": f"{cls}::{mname}({argc})",
                }
                if not as_json:
                    err(f"[MISSING METHOD] {const_name}: {cls}::{mname}({argc} args)")
                issues += 1
                continue
            cur_rva = header_rvas.get(const_name)
            shifted = cur_rva != me.rva
            report["methods"][const_name] = {
                "status": "shift" if shifted else "ok",
                "dump_rva": f"0x{me.rva:X}",
                "header_rva": f"0x{cur_rva:X}" if cur_rva is not None else None,
            }
            if shifted:
                if not as_json:
                    warn(
                        f"[SHIFT]  {const_name:44s} -> dump=0x{me.rva:X} vs offsets.h=0x{cur_rva or 0:X}"
                    )
                warnings += 1
            elif not as_json:
                ok(f"[OK]     {const_name:44s} -> 0x{me.rva:X}")

        if not as_json:
            section("7. Tier 2 & Tier 3 Field Offsets vs dump.cs")
        for const_name, (ns, cls, fname) in TRACKED_FIELDS.items():
            t = dump_types.get((ns, cls))
            if not t:
                report["fields"][const_name] = {
                    "status": "missing_type",
                    "type": f"{ns}.{cls}",
                }
                if not as_json:
                    err(f"[MISSING TYPE]  {const_name}: {ns}.{cls}")
                issues += 1
                continue
            if fname not in t.fields:
                report["fields"][const_name] = {
                    "status": "missing_field",
                    "field": f"{cls}::{fname}",
                }
                if not as_json:
                    err(f"[MISSING FIELD] {const_name}: {cls}::{fname}")
                issues += 1
                continue
            dump_off = t.fields[fname]
            cur_off = header_offs.get(const_name)
            shifted = cur_off != dump_off
            report["fields"][const_name] = {
                "status": "shift" if shifted else "ok",
                "dump_offset": f"0x{dump_off:X}",
                "header_offset": f"0x{cur_off:X}" if cur_off is not None else None,
            }
            if shifted:
                if not as_json:
                    warn(
                        f"[SHIFT]  {const_name:44s} -> dump=0x{dump_off:X} vs offsets.h=0x{cur_off or 0:X}"
                    )
                warnings += 1
            elif not as_json:
                ok(f"[OK]     {const_name:44s} -> 0x{dump_off:X}")

        if not as_json:
            section("8. Enum Drift vs dump.cs")
        for rel_src, cpp_enum, ns, dump_enum in TRACKED_ENUMS:
            t = dump_types.get((ns, dump_enum))
            if not t:
                report["enums"][cpp_enum] = {"status": "missing_enum"}
                if not as_json:
                    err(f"[MISSING ENUM] {ns}.{dump_enum}")
                issues += 1
                continue
            cpp_vals = parse_cpp_enum(root / rel_src, cpp_enum)
            mismatched = [
                {"name": k, "cpp": v, "dump": t.enum_values[k]}
                for k, v in cpp_vals.items()
                if k in t.enum_values and t.enum_values[k] != v
            ]
            extra_in_cpp = [k for k in cpp_vals if k not in t.enum_values]
            new_in_dump = [k for k in t.enum_values if k not in cpp_vals]
            if mismatched:
                report["enums"][cpp_enum] = {
                    "status": "mismatch",
                    "mismatched": mismatched,
                }
                if not as_json:
                    err(f"[MISMATCH] {cpp_enum}: {mismatched}")
                issues += len(mismatched)
            elif extra_in_cpp or new_in_dump:
                report["enums"][cpp_enum] = {
                    "status": "drift",
                    "extra_in_cpp": extra_in_cpp,
                    "new_in_dump": new_in_dump,
                }
                if not as_json:
                    warn(
                        f"[DRIFT]    {cpp_enum}: extra_in_cpp={extra_in_cpp}, new_in_dump={new_in_dump}"
                    )
                warnings += 1
            else:
                report["enums"][cpp_enum] = {"status": "ok", "count": len(cpp_vals)}
                if not as_json:
                    ok(f"[OK]       {cpp_enum} ({len(cpp_vals)} values exact match)")

    # 9. Build Artifacts
    if not as_json:
        section("9. Build Artifacts")
    proj = cfg.get("project", "PG2-Cheetos")
    for bin_name in (f"{proj}.dll", f"{proj}_Injector.exe"):
        bp = root / "build/bin" / bin_name
        if bp.is_file():
            size_bytes = bp.stat().st_size
            report["artifacts"][bin_name] = {"exists": True, "size_bytes": size_bytes}
            if not as_json:
                ok(f"{bin_name:28s} -> {size_bytes / (1024 * 1024):.2f} MB")
        else:
            report["artifacts"][bin_name] = {"exists": False}
            if not as_json:
                warn(f"{bin_name:28s} -> not built yet")
            warnings += 1

    if as_json:
        out = {
            "healthy": issues == 0 and warnings == 0,
            "issues": issues,
            "warnings": warnings,
            **report,
        }
        print(json.dumps(out, indent=2))
        return 1 if issues > 0 else 0

    print("")
    if issues == 0 and warnings == 0:
        ok("Doctor result: HEALTHY (0 issues, 0 warnings)")
        return 0
    if issues == 0:
        warn(
            f"Doctor result: {warnings} warning(s), 0 critical issues — run `updater.py autoheal` to sync."
        )
        return 0
    err(
        f"Doctor result: {issues} critical issue(s), {warnings} warning(s) — run `updater.py autoheal`."
    )
    return 1
