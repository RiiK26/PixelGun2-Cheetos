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

"""C++ header/enum parsing, patching, and dump.cs target loading."""

from pathlib import Path
import re
from typing import Dict, List, Set, Tuple

import dump_parser

from .registry import TRACKED_ENUMS, TRACKED_FIELDS, TRACKED_METHODS


def parse_offsets_h(
    offsets_h: Path,
) -> Tuple[Dict[str, str], Dict[str, int], Dict[str, int], Dict[str, str]]:
    """Returns (fields, offsets, rvas, signatures) from src/utils/offsets.h."""
    txt = offsets_h.read_text(encoding="utf-8")

    def extract_ns(ns: str) -> str:
        m = re.search(
            rf"namespace\s+{ns}\s*\{{(.*?)\}}\s*//\s*namespace\s+{ns}", txt, re.S
        )
        return m.group(1) if m else ""

    fields_map: Dict[str, str] = {}
    for m in re.finditer(
        r'inline\s+constexpr\s+const\s+char\*\s+(\w+)\s*=\s*"([^"]*)";',
        extract_ns("fields"),
    ):
        fields_map[m.group(1)] = m.group(2)

    offsets_map: Dict[str, int] = {}
    for m in re.finditer(
        r"inline\s+constexpr\s+int32_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+);",
        extract_ns("offsets"),
    ):
        offsets_map[m.group(1)] = int(m.group(2), 0)

    rvas_map: Dict[str, int] = {}
    for m in re.finditer(
        r"inline\s+constexpr\s+uintptr_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+);",
        extract_ns("RVA"),
    ):
        rvas_map[m.group(1)] = int(m.group(2), 0)

    sigs_map: Dict[str, str] = {}
    for m in re.finditer(
        r"inline\s+constexpr\s+const\s+char\*\s+(\w+)\s*=\s*((?:\"[^\"]*\"\s*)+);",
        extract_ns("signatures"),
        re.S,
    ):
        sigs_map[m.group(1)] = "".join(re.findall(r'"([^"]*)"', m.group(2))).strip()

    return fields_map, offsets_map, rvas_map, sigs_map


def parse_cpp_enum(cpp_path: Path, enum_name: str) -> Dict[str, int]:
    if not cpp_path.is_file():
        return {}
    txt = cpp_path.read_text(encoding="utf-8", errors="ignore")
    m = re.search(
        rf"enum\s+(?:class\s+)?{re.escape(enum_name)}\b[^{{]*\{{(.*?)\}};", txt, re.S
    )
    if not m:
        return {}
    out: Dict[str, int] = {}
    for em in re.finditer(
        r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(-?(?:0x[0-9A-Fa-f]+|\d+))",
        m.group(1),
        re.M,
    ):
        out[em.group(1)] = int(em.group(2), 0)
    return out


def append_enum_members(
    cpp_path: Path, enum_name: str, new_members: List[Tuple[str, int]]
) -> int:
    """Safely appends newly added enum entries before `};`. Returns count appended."""
    if not cpp_path.is_file() or not new_members:
        return 0
    txt = cpp_path.read_text(encoding="utf-8")
    m = re.search(
        rf"(enum\s+(?:class\s+)?{re.escape(enum_name)}\b[^{{]*\{{)(.*?)(\n[ \t]*\}};)",
        txt,
        re.S,
    )
    if not m:
        return 0

    body = m.group(2)
    indent_m = re.search(r"^([ \t]+)[A-Za-z_][A-Za-z0-9_]*\s*=", body, re.M)
    indent = indent_m.group(1) if indent_m else "  "

    body_stripped = body.rstrip()
    if (
        body_stripped
        and not body_stripped.endswith(",")
        and not body_stripped.endswith("{")
    ):
        body_stripped += ","

    additions = "\n".join(f"{indent}{name} = {val}," for name, val in new_members)
    new_body = f"{body_stripped}\n{additions}"
    new_txt = txt[: m.start(2)] + new_body + txt[m.end(2) :]
    cpp_path.write_text(new_txt, encoding="utf-8")
    return len(new_members)


def find_used_signatures(src_dir: Path, sig_names: List[str]) -> Set[str]:
    used: Set[str] = set()
    for cpp in src_dir.rglob("*.cpp"):
        if "vendor" in cpp.parts:
            continue
        txt = cpp.read_text(encoding="utf-8", errors="ignore")
        for name in sig_names:
            if f"signatures::{name}" in txt:
                used.add(name)
    return used


def load_tracked_dump(dump_path: Path) -> Dict[Tuple[str, str], dump_parser.TypeEntry]:
    targets: Set[Tuple[str, str]] = set()
    for ns, cls, _, _, _ in TRACKED_METHODS.values():
        targets.add((ns, cls))
    for ns, cls, _ in TRACKED_FIELDS.values():
        targets.add((ns, cls))
    for _, _, ns, ename in TRACKED_ENUMS:
        targets.add((ns, ename))
    return dump_parser.parse_dump_targets(dump_path, targets)


def patch_offsets_h(
    offsets_h: Path,
    new_rvas: Dict[str, int],
    new_offsets: Dict[str, int],
    new_sigs: Dict[str, str],
) -> Tuple[int, int, int]:
    """Patches RVAs, field offsets, and AOB signatures in src/utils/offsets.h."""
    txt = offsets_h.read_text(encoding="utf-8")
    rva_changed = 0
    off_changed = 0
    sig_changed = 0

    def repl_rva(m: re.Match) -> str:
        nonlocal rva_changed
        prefix, name, sep, old_hex = m.group(1), m.group(2), m.group(3), m.group(4)
        if name in new_rvas:
            val_hex = f"0x{new_rvas[name]:X}"
            if old_hex.upper() != val_hex.upper():
                rva_changed += 1
                return f"{prefix}{name}{sep}{val_hex}"
        return m.group(0)

    def repl_off(m: re.Match) -> str:
        nonlocal off_changed
        prefix, name, sep, old_hex = m.group(1), m.group(2), m.group(3), m.group(4)
        if name in new_offsets:
            val_hex = f"0x{new_offsets[name]:X}"
            if old_hex.upper() != val_hex.upper():
                off_changed += 1
                return f"{prefix}{name}{sep}{val_hex}"
        return m.group(0)

    txt = re.sub(
        r"(inline\s+constexpr\s+uintptr_t\s+)(\w+)(\s*=\s*)(0x[0-9A-Fa-f]+)",
        repl_rva,
        txt,
    )
    txt = re.sub(
        r"(inline\s+constexpr\s+int32_t\s+)(\w+)(\s*=\s*)(0x[0-9A-Fa-f]+)",
        repl_off,
        txt,
    )

    if new_sigs:
        ns_match = re.search(
            r"(namespace\s+signatures\s*\{)(.*?)(\}\s*//\s*namespace\s+signatures)",
            txt,
            re.S,
        )
        if ns_match:
            sig_block = ns_match.group(2)

            def repl_sig(m: re.Match) -> str:
                nonlocal sig_changed
                prefix, name, sep, old_lit = (
                    m.group(1),
                    m.group(2),
                    m.group(3),
                    m.group(4),
                )
                if name in new_sigs:
                    old_val = "".join(re.findall(r'"([^"]*)"', old_lit)).strip()
                    if old_val != new_sigs[name]:
                        sig_changed += 1
                        return f'{prefix}{name}{sep}"{new_sigs[name]}";'
                return m.group(0)

            new_block = re.sub(
                r"(inline\s+constexpr\s+const\s+char\*\s+)(\w+)(\s*=\s*)((?:\"[^\"]*\"\s*)+);",
                repl_sig,
                sig_block,
                flags=re.S,
            )
            txt = txt[: ns_match.start(2)] + new_block + txt[ns_match.end(2) :]

    if rva_changed or off_changed or sig_changed:
        offsets_h.write_text(txt, encoding="utf-8")
    return rva_changed, off_changed, sig_changed
