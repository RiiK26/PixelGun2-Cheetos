# Project: PG2-Cheetos (PixelGun2)

This is still game Demo, but the project keep using standard (non-demo) name because it will released later on steam

## Tech Stack
- **Language:** C++20 (`CMAKE_CXX_STANDARD 20`), C11 (`CMAKE_C_STANDARD 11`)
- **Build:** CMake 3.20+, Ninja, cross-compiled on Linux with MinGW-w64 (`x86_64-w64-mingw32-g++`) or natively with MSVC on Windows
- **Vendor Libraries:** ImGui (`src/vendor/ImGui/`, Win32 + DX11 + DX12 backends), MinHook (`src/vendor/minhook/`)
- **Target:** Windows x64 DLL (`PG2-Cheetos.dll`)

## Essential Directories & Level 2 Context (Load Selectively)
- Game Binaries:
    GameAssembly.dll: /data/SteamLibrary/steamapps/common/Pixel Gun 2 Demo/
    global-metadata.dat: /data/SteamLibrary/steamapps/common/Pixel Gun 2 Demo/Pixel Gun 2 Demo_Data/il2cpp_data/Metadata/

- Registries & Local Artifacts:
    config.json: for Signature registry
    resources/dumped/dump.cs (git-ignored):

## Build Commands
Only `Release` and `Debug` build types are supported:

- **Release (default, logging stripped):**
  ```bash
  cmake -B build -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Release && cmake --build build
  ```
- **Debug (`ENABLE_DEBUG_LOG` enabled):**
  ```bash
  cmake -B build -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Debug && cmake --build build
  ```

## Architecture & File Responsibilities
Keep each layer focused and dependencies one-way (`features` -> `utils`/`ui`/`data`; `core` -> `ui`/`utils`; `utils` depends on nothing else):

- `src/core/` — Process lifecycle & graphics hooking
  - `main.cpp`: `DllMain`, `MainThread` init/shutdown orchestration (`Config::SetConfigPath`, `SettingsRegistry` → `Menu` population, `Config::Load`/`Save`), `END` unload loop.
  - `renderer.h` / `renderer.cpp`: DX11 & DX12 dummy-device vtable hooking (`Present`, `Present1`, `ResizeBuffers`, `ExecuteCommandLists`), per-frame backend auto-detection, ImGui frame rendering, and optional `OverlayCallback`.
- `src/ui/` — Presentation & input
  - `menu.h` / `menu.cpp`: ImGui context/font setup, `WndProc` hook (`INSERT` toggle, arrow/Enter navigation), `MenuItem` model (`Header`, `Toggle`, `Slider`, `Action`), and menu rendering.
- `src/utils/` — Pure reusable infrastructure
  - `logger.h` / `logger.cpp`: Thread-safe debug logger (`Logger::Init`, `Logger::Log`, `Logger::Shutdown`). Active only when `ENABLE_DEBUG_LOG` is defined (`Debug`); inline no-ops in `Release`.
  - `settings_registry.h` / `settings_registry.cpp`: Centralized `SettingsRegistry` (`AddGroup`, `AddBool`, `AddFloat`, `AddInt`, `AddAction`, `Clear`, `GetGroups`) driving both `Menu` population and `Config` persistence.
  - `config.h` / `config.cpp`: Thread-safe INI persistence (`Config::SetConfigPath`, `Config::Save`, `Config::Load`) reading/writing `featureconfig.txt` next to the DLL.
  - `aob.h` / `aob.cpp`, `il2cpp.h` / `il2cpp.cpp`, `resolver.h` / `resolver.cpp`: Pattern scanning, IL2CPP helpers, and runtime resolution utilities.
  - `offsets.h`: Centralized registry for all IL2CPP `fields`, `offsets`, `RVA`, and `signatures` (never hardcode raw RVAs/offsets/signatures in feature `.cpp` files; always reference `offsets.h`).
- `src/data/` — Game data definitions & 1:1 enums (`ability_card.hpp`, `character.hpp`, `chest.hpp`, `currency.hpp`, `loadout.hpp`, `skin.hpp`).
- `src/features/` — Category-separated feature modules (`anticheat/`, `currency/`, `player/`, `visual/`, `weapon/`).
  - **1:1 feature `.cpp` + 1 category `.hpp` orchestrator:** Each individual feature lives in its own `.cpp` file (e.g., `visual/body_esp.cpp`, `visual/box_esp.cpp`, `visual/name_esp.cpp`, with shared hooks/caches in `*_shared.cpp`), and each feature category folder has a single `.hpp` orchestrator header (e.g., `visual/visual.hpp`) that exposes the category lifecycle and coordinates its features.
  - **Single-responsibility toggles:** Never add redundant master toggles; each feature toggle controls its own behavior independently.
  - **Config load/save:** Auto-load `featureconfig.txt` on attach so saved settings override defaults; never auto-save on unload (`Save Config` is explicit only).
- `src/injector/` — Standalone injector (`mainInjector.cpp`), excluded from the DLL target.
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
- Always verify changes compile cleanly (`cmake --build build`) before reporting completion.
