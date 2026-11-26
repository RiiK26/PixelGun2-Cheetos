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

"""Shared ANSI console formatting helpers for PG2-Cheetos tooling."""

import sys

RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
CYAN = "\033[0;36m"
PURPLE = "\033[0;35m"
BOLD = "\033[1m"
NC = "\033[0m"


def header(title: str) -> None:
    print(f"\n{PURPLE}{BOLD}  === {title} ==={NC}\n")


def section(title: str) -> None:
    print(f"\n{BOLD}  [{title}]{NC}")


def info(msg: str) -> None:
    print(f"  {CYAN}[*]{NC} {msg}")


def ok(msg: str) -> None:
    print(f"  {GREEN}[+]{NC} {msg}")


def warn(msg: str) -> None:
    print(f"  {YELLOW}[!]{NC} {msg}")


def err(msg: str) -> None:
    print(f"  {RED}[x]{NC} {msg}", file=sys.stderr)
