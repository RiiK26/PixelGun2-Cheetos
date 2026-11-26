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

"""IL2CPP dump.cs parser for PG2-Cheetos tooling."""

from dataclasses import dataclass, field
from pathlib import Path
import re
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class MethodEntry:
    name: str
    rva: int
    params: str
    arg_count: int
    signature: str


@dataclass
class TypeEntry:
    namespace: str
    name: str
    kind: str  # class, struct, enum, interface
    fields: Dict[str, int] = field(default_factory=dict)  # field_name -> offset
    enum_values: Dict[str, int] = field(
        default_factory=dict
    )  # member_name -> int value
    methods: List[MethodEntry] = field(default_factory=list)


_TYPE_DECL_RE = re.compile(
    r"^(?:public|internal|private|protected)\s+"
    r"(?:(?:sealed|abstract|static|readonly|ref)\s+)*"
    r"(class|struct|enum|interface)\s+"
    r"([A-Za-z_][A-Za-z0-9_.`<>]*)"
)
_FIELD_RE = re.compile(
    r"^\s*(?:public|private|protected|internal)\s+"
    r"(?!static\s)(?!const\s).+?\s+([A-Za-z_<][A-Za-z0-9_<>]*)\s*;\s*//\s*0x([0-9A-Fa-f]+)"
)
_ENUM_VAL_RE = re.compile(
    r"^\s*public\s+const\s+\S+\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(-?\d+)\s*;"
)
_RVA_RE = re.compile(r"//\s*RVA:\s*0x([0-9A-Fa-f]+)")
_METHOD_RE = re.compile(
    r"^\s*(?:public|private|protected|internal)\s+"
    r"(?:(?:static|override|virtual|abstract|extern|sealed|new|unsafe|readonly)\s+)*"
    r"(?:ref\s+)?\S+\s+([A-Za-z_.][A-Za-z0-9_.]*)\s*\(([^)]*)\)"
)


def _count_args(params: str) -> int:
    s = params.strip()
    if not s:
        return 0
    depth = 0
    count = 1
    for ch in s:
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth = max(0, depth - 1)
        elif ch == "," and depth == 0:
            count += 1
    return count


def parse_dump_targets(
    dump_path: Path,
    target_types: Set[Tuple[str, str]],
) -> Dict[Tuple[str, str], TypeEntry]:
    """Parses only the requested (namespace, type_name) entries from dump.cs in a single pass.

    Pass namespace="" to match a type in any namespace (or global namespace).
    """
    if not dump_path.is_file():
        return {}

    wanted_names = {name for _, name in target_types}
    results: Dict[Tuple[str, str], TypeEntry] = {}

    current_ns = ""
    active_key: Optional[Tuple[str, str]] = None
    active_entry: Optional[TypeEntry] = None
    pending_rva: Optional[int] = None

    with dump_path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("// Namespace:"):
                current_ns = line[len("// Namespace:") :].strip()
                pending_rva = None
                continue

            if active_entry is not None:
                if line.startswith("}"):
                    results[active_key] = active_entry  # type: ignore[index]
                    active_entry = None
                    active_key = None
                    pending_rva = None
                    continue

                if active_entry.kind == "enum":
                    em = _ENUM_VAL_RE.match(line)
                    if em:
                        active_entry.enum_values[em.group(1)] = int(em.group(2))
                    continue

                fm = _FIELD_RE.match(line)
                if fm:
                    active_entry.fields[fm.group(1)] = int(fm.group(2), 16)
                    continue

                rm = _RVA_RE.search(line)
                if rm:
                    pending_rva = int(rm.group(1), 16)
                    continue

                if pending_rva is not None:
                    stripped = line.strip()
                    if (
                        not stripped
                        or stripped.startswith("//")
                        or stripped.startswith("[")
                    ):
                        continue
                    mm = _METHOD_RE.match(line)
                    if mm:
                        mname = mm.group(1)
                        params = mm.group(2).strip()
                        active_entry.methods.append(
                            MethodEntry(
                                name=mname,
                                rva=pending_rva,
                                params=params,
                                arg_count=_count_args(params),
                                signature=stripped,
                            )
                        )
                    pending_rva = None
                continue

            # Not inside an active type — fast pre-check before regex
            if (
                "class " in line
                or "struct " in line
                or "enum " in line
                or "interface " in line
            ):
                tm = _TYPE_DECL_RE.match(line)
                if not tm:
                    continue
                kind, tname = tm.group(1), tm.group(2)
                if tname not in wanted_names:
                    continue
                if (current_ns, tname) in target_types:
                    active_key = (current_ns, tname)
                elif ("", tname) in target_types:
                    active_key = ("", tname)
                else:
                    continue
                active_entry = TypeEntry(namespace=current_ns, name=tname, kind=kind)
                pending_rva = None

    return results


def find_method_in_type(
    entry: TypeEntry,
    method_name: str,
    arg_count: int = -1,
    param_substr: Optional[str] = None,
) -> Optional[MethodEntry]:
    for m in entry.methods:
        if m.name != method_name:
            continue
        if arg_count >= 0 and m.arg_count != arg_count:
            continue
        if param_substr and param_substr not in m.params:
            continue
        return m
    return None
