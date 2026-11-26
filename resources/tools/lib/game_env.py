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

"""Steam game discovery and binary metadata inspection."""

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
from typing import List, Optional, Tuple

from .registry import REQUIRED_IL2CPP_EXPORTS, STEAM_APP_ID


def find_project_root() -> Path:
    here = Path(__file__).resolve().parent
    for parent in [here, *here.parents]:
        if (parent / "config.json").is_file() and (parent / "CMakeLists.txt").is_file():
            return parent
    raise RuntimeError("Could not locate project root (config.json + CMakeLists.txt).")


def collect_steam_libraries() -> List[Path]:
    home = Path.home()
    roots = [
        home / ".local/share/Steam",
        home / ".steam/steam",
        home / ".var/app/com.valvesoftware.Steam/.local/share/Steam",
        home / "Library/Application Support/Steam",
        Path("C:/Program Files (x86)/Steam"),
        Path("C:/Program Files/Steam"),
    ]
    seen = set()
    libraries: List[Path] = []

    def add_lib(p: Path) -> None:
        try:
            rp = p.resolve()
        except OSError:
            return
        if rp.is_dir() and rp not in seen:
            seen.add(rp)
            libraries.append(rp)

    for root in roots:
        if not root.is_dir():
            continue
        add_lib(root)
        vdf = root / "steamapps/libraryfolders.vdf"
        if vdf.is_file():
            txt = vdf.read_text(encoding="utf-8", errors="ignore")
            for m in re.finditer(r'"path"\s+"([^"]+)"', txt):
                add_lib(Path(m.group(1).replace("\\\\", "/")))

    return libraries


def locate_game(game_dir_override: Optional[str] = None) -> Tuple[Path, Optional[Path]]:
    """Returns (game_dir, appmanifest_path_or_None)."""
    override = game_dir_override or os.environ.get("PG2_GAME_DIR")
    if override:
        gd = Path(override).resolve()
        if not gd.is_dir():
            raise FileNotFoundError(f"Specified game directory does not exist: {gd}")
        manifest = gd.parent.parent / f"appmanifest_{STEAM_APP_ID}.acf"
        return gd, manifest if manifest.is_file() else None

    for lib in collect_steam_libraries():
        manifest = lib / "steamapps" / f"appmanifest_{STEAM_APP_ID}.acf"
        if manifest.is_file():
            txt = manifest.read_text(encoding="utf-8", errors="ignore")
            m = re.search(r'"installdir"\s+"([^"]+)"', txt)
            if m:
                candidate = lib / "steamapps/common" / m.group(1)
                if candidate.is_dir():
                    return candidate, manifest

        fallback = lib / "steamapps/common/Pixel Gun 2"
        if fallback.is_dir():
            return fallback, manifest if manifest.is_file() else None

    raise FileNotFoundError(
        "Could not locate Pixel Gun 2 Steam installation. Pass --game-dir <path> or set PG2_GAME_DIR."
    )


def extract_steam_build_id(manifest: Optional[Path]) -> str:
    if not manifest or not manifest.is_file():
        return ""
    txt = manifest.read_text(encoding="utf-8", errors="ignore")
    m = re.search(r'"buildid"\s+"(\d+)"', txt)
    return m.group(1) if m else ""


def extract_exe_version(exe_path: Path) -> str:
    """Extracts numeric ProductVersion/FileVersion from Windows PE VS_VERSION_INFO."""
    if not exe_path.is_file():
        return ""
    data = exe_path.read_bytes()
    for key in ("ProductVersion", "FileVersion"):
        needle = (key + "\x00").encode("utf-16le")
        idx = data.find(needle)
        if idx == -1:
            continue
        pos = idx + len(needle)
        while pos + 2 <= len(data) and data[pos : pos + 2] == b"\x00\x00":
            pos += 2
        chars = []
        while pos + 2 <= len(data):
            ch = data[pos : pos + 2]
            if ch == b"\x00\x00":
                break
            chars.append(ch)
            pos += 2
        val = b"".join(chars).decode("utf-16le", errors="ignore").strip().lstrip("vV")
        if val:
            return val
    return ""


def compute_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def check_il2cpp_exports(dll_path: Path) -> Tuple[List[str], List[str]]:
    """Returns (present_exports, missing_exports)."""
    objdump = shutil.which("objdump") or shutil.which("x86_64-w64-mingw32-objdump")
    if not objdump:
        return [], list(REQUIRED_IL2CPP_EXPORTS)
    out = subprocess.check_output(
        [objdump, "-p", str(dll_path)], text=True, stderr=subprocess.DEVNULL
    )
    lines = out.splitlines()
    present, missing = [], []
    for sym in REQUIRED_IL2CPP_EXPORTS:
        if any(line.strip().endswith(f" {sym}") for line in lines):
            present.append(sym)
        else:
            missing.append(sym)
    return present, missing
