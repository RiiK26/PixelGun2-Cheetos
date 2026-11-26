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

"""Standalone AOB signature generator and verifier for GameAssembly.dll.

Uses objdump to disassemble target RVAs, wildcards rel32 and RIP-relative
displacements, and searches for the shortest unique IDA-style byte pattern
across all executable PE sections (.text, il2cpp, etc.).
"""

import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
from typing import List, Optional, Tuple

IMAGE_SCN_MEM_EXECUTE = 0x20000000


def get_pe_image_base_and_exec_sections(
    pe_bytes: bytes,
) -> Tuple[int, List[Tuple[str, int, bytes]]]:
    """Returns (image_base, [(sec_name, virtual_address, raw_bytes), ...])."""
    e_lfanew = struct.unpack_from("<I", pe_bytes, 0x3C)[0]
    if pe_bytes[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        raise ValueError("Invalid PE signature.")

    num_sections = struct.unpack_from("<H", pe_bytes, e_lfanew + 6)[0]
    opt_hdr_size = struct.unpack_from("<H", pe_bytes, e_lfanew + 20)[0]
    opt_off = e_lfanew + 24
    magic = struct.unpack_from("<H", pe_bytes, opt_off)[0]
    if magic == 0x20B:  # PE32+ (x64)
        image_base = struct.unpack_from("<Q", pe_bytes, opt_off + 24)[0]
    else:
        image_base = struct.unpack_from("<I", pe_bytes, opt_off + 28)[0]

    sec_off = opt_off + opt_hdr_size
    exec_sections: List[Tuple[str, int, bytes]] = []
    for i in range(num_sections):
        off = sec_off + i * 40
        name = pe_bytes[off : off + 8].rstrip(b"\x00").decode("ascii", errors="ignore")
        _, vaddr, rsize, rptr = struct.unpack_from("<IIII", pe_bytes, off + 8)
        chars = struct.unpack_from("<I", pe_bytes, off + 36)[0]
        if chars & IMAGE_SCN_MEM_EXECUTE:
            exec_sections.append((name, vaddr, pe_bytes[rptr : rptr + rsize]))

    return image_base, exec_sections


def compile_aob_regex(pattern: str) -> bytes:
    parts = []
    for tok in pattern.strip().split():
        if tok in ("?", "??"):
            parts.append(b".")
        else:
            parts.append(re.escape(bytes([int(tok, 16)])))
    return b"".join(parts)


def scan_pattern(
    exec_sections: List[Tuple[str, int, bytes]],
    pattern: str,
    max_hits: int = 3,
) -> List[int]:
    """Scans all executable PE sections for an IDA-style AOB pattern. Returns matching RVAs."""
    rx = compile_aob_regex(pattern)
    hits: List[int] = []
    for _, vaddr, buf in exec_sections:
        for m in re.finditer(rx, buf, re.DOTALL):
            hits.append(vaddr + m.start())
            if len(hits) >= max_hits:
                return hits
    return hits


def _wildcard_instruction(bhex: List[str], asm: str) -> List[str]:
    bs = list(bhex)
    if re.match(
        r"^(call|jmp|je|jne|jz|jnz|jg|jge|jl|jle|ja|jae|jb|jbe|js|jns|jo|jno)\s+0x",
        asm,
    ):
        if len(bs) >= 5:
            bs[-4:] = ["?"] * 4
        elif len(bs) == 2:
            bs[1] = "?"
    elif re.search(r"\[rip[+-]0x", asm):
        if re.search(r"\[rip[+-]0x[0-9a-f]+\],\s*0x", asm):
            upper = asm.upper()
            if "BYTE PTR" in upper:
                bs[-5:-1] = ["?"] * 4
            elif "DWORD PTR" in upper or "QWORD PTR" in upper:
                bs[-8:-4] = ["?"] * 4
            else:
                bs[-4:] = ["?"] * 4
        else:
            bs[-4:] = ["?"] * 4
    return bs


def generate_unique_aob(
    dll_path: Path,
    rva: int,
    exec_sections: List[Tuple[str, int, bytes]],
    image_base: int = 0x180000000,
    max_bytes: int = 256,
    min_len: int = 16,
) -> Tuple[Optional[str], int]:
    """Generates the shortest unique AOB pattern for `rva`.

    Returns (pattern_str, match_count). If unique, match_count == 1 and pattern_str != None.
    """
    objdump = shutil.which("objdump") or shutil.which("x86_64-w64-mingw32-objdump")
    if not objdump:
        raise RuntimeError("objdump not found in PATH (required for AOB generation).")

    va = image_base + rva
    cmd = [
        objdump,
        "-d",
        "-M",
        "intel",
        f"--start-address={hex(va)}",
        f"--stop-address={hex(va + max_bytes)}",
        str(dll_path),
    ]
    out = subprocess.check_output(cmd, text=True, stderr=subprocess.DEVNULL)

    tokens: List[str] = []
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s)+)\s*(.*)", line)
        if not m:
            continue
        bhex = [b.upper() for b in m.group(2).strip().split()]
        asm = m.group(3).strip()
        if asm.startswith("int3"):
            break
        tokens.extend(_wildcard_instruction(bhex, asm))
        if asm == "ret":
            break

    if not tokens:
        return None, 0

    # If the entire function is shorter than min_len (e.g. tiny getter), test full function directly
    if len(tokens) < min_len:
        sub = list(tokens)
        while sub and sub[-1] == "?":
            sub.pop()
        if not sub:
            return None, 0
        pat = " ".join(sub)
        hits = scan_pattern(exec_sections, pat, max_hits=3)
        return (pat, 1) if (len(hits) == 1 and hits[0] == rva) else (None, len(hits))

    # Try increasing prefix lengths (step 2 bytes) for the shortest unique pattern
    for length in range(min_len, len(tokens) + 1, 2):
        sub = list(tokens[:length])
        while sub and sub[-1] == "?":
            sub.pop()
        if len(sub) < 8:
            continue
        pat = " ".join(sub)
        hits = scan_pattern(exec_sections, pat, max_hits=3)
        if len(hits) == 1 and hits[0] == rva:
            return pat, 1

    # Final attempt with full token list
    sub = list(tokens)
    while sub and sub[-1] == "?":
        sub.pop()
    pat = " ".join(sub)
    hits = scan_pattern(exec_sections, pat, max_hits=3)
    if len(hits) == 1 and hits[0] == rva:
        return pat, 1
    return None, len(hits)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate or verify unique IDA-style AOB signatures for GameAssembly.dll."
    )
    parser.add_argument(
        "--dll",
        default=None,
        help="Path to GameAssembly.dll (auto-discovered via Steam libraryfolders.vdf or PG2_GAME_DIR if omitted)",
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--rva", help="Target method RVA in hex (e.g. 0x1223510)")
    group.add_argument(
        "--verify", help="Verify an IDA-style AOB pattern and print matching RVAs"
    )
    parser.add_argument(
        "--max-bytes",
        type=int,
        default=256,
        help="Max disassembly bytes (default: 256)",
    )
    args = parser.parse_args()

    if args.dll:
        dll_path = Path(args.dll)
    else:
        tools_dir = Path(__file__).resolve().parent
        if str(tools_dir) not in sys.path:
            sys.path.insert(0, str(tools_dir))
        from lib.game_env import locate_game

        try:
            game_dir, _ = locate_game(None)
        except FileNotFoundError as exc:
            print(f"[!] {exc}", file=sys.stderr)
            return 1
        dll_path = game_dir / "GameAssembly.dll"

    if not dll_path.is_file():
        print(f"[!] GameAssembly.dll not found at: {dll_path}", file=sys.stderr)
        return 1

    pe_bytes = dll_path.read_bytes()
    image_base, exec_sections = get_pe_image_base_and_exec_sections(pe_bytes)

    if args.verify:
        hits = scan_pattern(exec_sections, args.verify, max_hits=10)
        print(f"Matches: {len(hits)}")
        for h in hits:
            print(f"  RVA 0x{h:X}")
        return 0 if len(hits) == 1 else 1

    rva = int(args.rva, 16)
    pat, count = generate_unique_aob(
        dll_path, rva, exec_sections, image_base=image_base, max_bytes=args.max_bytes
    )
    if pat:
        print(f"[+] UNIQUE (1 match) at RVA 0x{rva:X} ({len(pat.split())} bytes):")
        print(f'    "{pat}"')
        return 0

    print(
        f"[!] Could not generate a unique AOB at RVA 0x{rva:X} (hits={count} within {args.max_bytes} bytes)."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
