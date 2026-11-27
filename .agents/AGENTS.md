# Project: PG2-Cheetos (PixelGun2)

## Tech Stack
- **Language:** C++20 (`CMAKE_CXX_STANDARD 20`), C11 (`CMAKE_C_STANDARD 11`)
- **Build:** CMake 3.20+, Ninja, cross-compiled on Linux with MinGW-w64 (`x86_64-w64-mingw32-g++`) or natively with MSVC on Windows
- **Vendor Libraries:** ImGui (`src/vendor/ImGui/`, Win32 + DX11 + DX12 backends), MinHook (`src/vendor/minhook/`)
- **Target:** Windows x64 DLL (`PG2-Cheetos.dll`)

## Essential Directories & Level 2 Context (Load Selectively)

- **Game Binaries (Steam AppID `3456950`, auto-discovered via Steam `libraryfolders.vdf` or `$PG2_GAME_DIR`):**
  - `GameAssembly.dll`: `<SteamLibrary>/steamapps/common/Pixel Gun 2/`
  - `global-metadata.dat`: `<SteamLibrary>/steamapps/common/Pixel Gun 2/Pixel Gun 2_Data/il2cpp_data/Metadata/`
- **Registries & Local Artifacts:**
  - `config.json`: Project name, numeric game `version` (no leading `v`), `steam-build-id`, `assembly_sha256`, and Tier 1 AOB signature registry (`CMakeLists.txt` tracks it via `CMAKE_CONFIGURE_DEPENDS`).
  - `resources/dumped/dump.cs` (git-ignored): IL2CPP metadata dump used for offset/RVA/enum verification and auto-healing.
  - `todo.md` (git-ignored): Local task & feature roadmap (`New Feature`, `Fix Feature`, `Improve Feature`).

## Build & Workflow Commands
Only `Release` and `Debug` build types are supported (choose one, you cannot build both in sametime):

- **Wrapper build script:**
  ```bash
  ./resources/scripts/build release  # Release DLL without injector (passed)
  ./resources/scripts/build debug --injector     # Debug DLL (ENABLE_DEBUG_LOG) + Injector
  ```
- **Direct CMake (Release default, logging stripped):**
  ```bash
  cmake -B build -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DBUILD_INJECTOR=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build
  ```
- **Direct CMake (Debug, `ENABLE_DEBUG_LOG` enabled):**
  ```bash
  cmake -B build -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DBUILD_INJECTOR=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build
  ```
- **Post-Patch Maintenance (`resources/tools/`, full runbook in `.agents/skills/update-project/SKILL.md`):**
  ```bash
  ./resources/tools/updater.py doctor                              # 9-point read-only health check (--json for CI)
  ./resources/tools/updater.py autoheal --build                    # Sync config.json, heal broken AOBs, sync offsets.h & enums, rebuild
  ./resources/tools/updater.py gen-aob PlayerVisuals::Present      # Generate unique Tier 1 AOB by symbol or 0xRVA
  ./resources/tools/aob_gen.py --verify "0F B6 81 B8 01 00 00 C3"  # Verify AOB uniqueness across executable PE sections
  ```

## Architecture & File Responsibilities
Keep each layer focused and dependencies one-way (`features` -> `utils`/`ui`; `core` -> `ui`/`utils`; `utils` depends on nothing else):

- `src/core/` — Process lifecycle & graphics hooking
  - `main.cpp`: `DllMain`, `MainThread` init/shutdown orchestration (`Config::SetConfigPath`, `SettingsRegistry` → `Menu` population, `Config::Load`/`Save`, `IL2CPP::Init()` before feature init), `END` unload loop.
  - `renderer.h` / `renderer.cpp`: DX11 & DX12 dummy-device vtable hooking (`Present`, `Present1`, `ResizeBuffers`, `ExecuteCommandLists`), per-frame backend auto-detection, ImGui frame rendering, and optional `OverlayCallback`.
- `src/ui/` — Presentation & input
  - `menu.h` / `menu.cpp`: ImGui context/font setup, `WndProc` hook (`INSERT` toggle, arrow/Enter navigation), `MenuItem` model (`Header`, `Toggle`, `Slider`, `Choice`, `Action`), and menu rendering.
- `src/utils/` — Pure reusable infrastructure
  - `logger.h` / `logger.cpp`: Thread-safe debug logger (`Logger::Init`, `Logger::Log`, `Logger::Shutdown`). Active only when `ENABLE_DEBUG_LOG` is defined (`Debug`); inline no-ops in `Release`.
  - `settings_registry.h` / `settings_registry.cpp`: Centralized `SettingsRegistry` (`AddGroup`, `AddBool`, `AddFloat`, `AddInt`, `AddChoice`, `AddAction`, `Clear`, `GetGroups`) driving both `Menu` population and `Config` persistence.
  - `config.h` / `config.cpp`: Thread-safe INI persistence (`Config::SetConfigPath`, `Config::Save`, `Config::Load`) reading/writing `featureconfig.txt` next to the DLL.
  - `aob.h` / `aob.cpp`, `il2cpp.h` / `il2cpp.cpp`, `resolver.h` / `resolver.cpp`: 3-tier runtime resolution — Tier 1 AOB scan across all `IMAGE_SCN_MEM_EXECUTE` PE sections (`.text`, `il2cpp`), Tier 2 IL2CPP metadata lookup, Tier 3 static RVA/offset fallback.
  - `offsets.h`: Centralized registry for all IL2CPP `fields`, `offsets`, `RVA`, and `signatures` (never hardcode raw RVAs/offsets/signatures in feature `.cpp` files; always reference `offsets.h`).
- `src/features/` — Category-separated feature modules (`currency/`, `player/`, `visual/`, `weapon/`).
  - **1:1 feature `.cpp` + 1 category `.hpp` orchestrator:** Each individual feature lives in its own `.cpp` file (e.g., `visual/body_esp.cpp`, `visual/box_esp.cpp`, `visual/name_esp.cpp`, `visual/health_esp.cpp`, `visual/snapline_esp.cpp`, `visual/head_dot_esp.cpp` with shared hooks/caches/LOS raycasts in `visual/esp_shared.cpp`; `weapon/aimbot.cpp` with `weapon/weapon.hpp`), and each feature category folder has a single `.hpp` orchestrator header (`visual/visual.hpp`, `weapon/weapon.hpp`) that exposes the category lifecycle and coordinates its features.
  - **Cross-category snapshot sharing:** Non-visual consumers (such as `Weapon::Aimbot`) register via `Visual::EspShared::SetSnapshotConsumer` (atomic callback) so `EspShared` keeps publishing `PlayerSnapshot` + `Physics::Linecast` LOS visibility without introducing a circular dependency.
  - **Single-responsibility toggles:** Never add redundant master toggles; each feature toggle controls its own behavior independently.
  - **Config load/save:** Auto-load `featureconfig.txt` on attach so saved settings override defaults; never auto-save on unload (`Save Config` is explicit only).
- `src/injector/` — Standalone injector (`mainInjector.cpp`), excluded from the DLL target (built via `-DBUILD_INJECTOR=ON`).
- `resources/tools/` — Post-patch maintenance tooling (`updater.py` CLI, `aob_gen.py`, `dump_parser.py`, and `lib/` modules: `registry.py`, `game_env.py`, `source_sync.py`, `doctor.py`, `healer.py`).
- `resources/scripts/` — Build & Wine/Proton launcher wrappers (`build`, `run.sh`).
- `src/vendor/` — Third-party dependencies (`ImGui/`, `minhook/`).

## Code Conventions
- **License header:** Include the Apache 2.0 `Copyright 2026 - RiiK` header at the top of every source/header file.
- **Namespaces:** Always use explicit named namespaces (`RendererHook`, `Menu`, `Logger`, `SettingsRegistry`, `Config`, etc.). **Never use anonymous namespaces** (`namespace { ... }`) — use `static` for file-local functions and state instead.
- **Minimal headers (IWYU):** Expose only the public API in `.h`/`.hpp` files; keep internal state and helpers `static` inside `.cpp` files.
- **Logging:** Use `Logger::Log("[TAG] ...")`. Never ship logging or log strings in `Release` builds.
- **Win32 portability:** Prefer Win32 APIs (e.g., `GetLocalTime`) over CRT functions that differ between MSVC and MinGW-w64 (`localtime_s` vs `localtime_r`).

## Boundaries
- Never modify files under `src/vendor/`.
- Never enable `ENABLE_DEBUG_LOG` in `Release`.
- Never hardcode or commit local host paths (e.g. user home directories, local mount points, or custom Steam library paths) in code, scripts, configs, or docs — always use relative paths, Steam `libraryfolders.vdf` auto-discovery, or environment variables (`$PG2_GAME_DIR`).
- Keep `config.json` `"version"` numeric-only (no leading `v`), synced from `Pixel Gun 2.exe` via `resources/tools/updater.py`.
- **Tier 1 AOB by default:** Always generate and wire a unique Tier 1 AOB signature (`./resources/tools/updater.py gen-aob <SymbolOrRVA>`) into `src/utils/offsets.h` (`signatures::`) and `Resolver::ResolveMethod({ .aobPattern = ... })` for every method used by any new or updated feature. Only omit `.aobPattern` (falling back to Tier 2 IL2CPP metadata / Tier 3 static RVA) when `gen-aob` confirms a tiny or ICF-merged function has no unique byte pattern in `GameAssembly.dll`.
- **Centralized offsets & auto-tracking:** Never hardcode raw RVAs, field offsets, or AOB strings in `src/features/`. For every new feature, define its constants in `src/utils/offsets.h` (`fields::`, `offsets::`, `RVA::`, `signatures::`), register them in `resources/tools/lib/registry.py` (`TRACKED_METHODS`, `TRACKED_FIELDS`, `TRACKED_ENUMS`), and run `updater.py autoheal` to sync `config.json`.
- Always verify changes compile cleanly (`cmake --build build`) before reporting completion.
