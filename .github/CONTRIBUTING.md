# Contributing to PixelGun2-Cheetos

Thank you for your interest in contributing! Please review these guidelines before opening an issue or pull request.

## Development Setup

### Prerequisites
- **CMake 3.20+** and **Ninja**
- **Linux / macOS (cross-compile):** `x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`, `x86_64-w64-mingw32-windres`
- **Windows (native):** Visual Studio 2022 (MSVC x64)
- **Formatting:** `clang-format` 23 (see `.clang-format`)

### Building
Only `Release` and `Debug` configurations are supported:

```bash
# Linux / macOS
./resources/scripts/build debug --injector     # Debug DLL (ENABLE_DEBUG_LOG) + Injector
./resources/scripts/build release --injector   # Release DLL (logging stripped) + Injector
```

```powershell
# Windows (MSVC)
cmake -B build -A x64 -DBUILD_INJECTOR=ON
cmake --build build --config Release --parallel
```

## Coding Standards
- **License Header:** Include the Apache 2.0 `Copyright 2026 - RiiK` header at the top of every source/header file under `src/` (excluding `src/vendor/`).
- **Namespaces:** Use explicit named namespaces; never use anonymous namespaces (`namespace { ... }`) — use `static` for file-local state and helpers.
- **Architecture:** Follow the 1:1 feature `.cpp` + single category `.hpp` orchestrator pattern under `src/features/<category>/` with one-way dependencies (`features` → `utils`/`ui`; `core` → `ui`/`utils`; `utils` standalone).
- **Offsets & Tier 1 AOBs:** Never hardcode raw RVAs, field offsets, or AOB patterns inside feature `.cpp` files. Centralize them in `src/utils/offsets.h`, register tracked symbols in `resources/tools/lib/registry.py`, generate unique Tier 1 AOBs with `./resources/tools/updater.py gen-aob <SymbolOrRVA>`, and sync `config.json` with `./resources/tools/updater.py autoheal`.
- **No Local Paths:** Never commit user home directories, custom Steam library paths, or local mount points.
- **Formatting:** Run `clang-format` 23 before pushing (`src/vendor/` is excluded).

## Pull Request Process
1. Fork the repository and create a topic branch from `main`.
2. Keep changes focused and well-scoped.
3. Verify both `Debug` and `Release` builds succeed cleanly with `-DBUILD_INJECTOR=ON`.
4. Run `./resources/tools/updater.py doctor` if you modified offsets, signatures, or enums.
5. Fill out the pull request template completely and wait for CI (`CI`, `CodeQL`, `Clang-Format`) to pass.
