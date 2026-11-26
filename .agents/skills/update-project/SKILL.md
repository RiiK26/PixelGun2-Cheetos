---
name: update-project
description: Post-patch update runbook for PG2-Cheetos. Use when Pixel Gun 2 updates on Steam to diagnose drift, heal broken AOBs, sync offsets/RVAs/enums, update config.json, and verify Release/Debug builds.
---

# Update Project Workflow (PG2-Cheetos)

Use this workflow whenever **Pixel Gun 2** (Steam AppID `3456950`) receives a Steam update or when verifying offsets, AOB signatures, and enums against a fresh IL2CPP dump.

## Prerequisites
- Game installed in Steam (auto-discovered via `libraryfolders.vdf`, or override via `$PG2_GAME_DIR` / `--game-dir`).
- Fresh IL2CPP dump at `resources/dumped/dump.cs` (git-ignored). If `GameAssembly.dll` is newer than `dump.cs`, re-dump before running `autoheal` (or configure `PG2_DUMPER_CMD` and pass `--dump`).
- Never hardcode local host paths anywhere in code, scripts, configs, or docs.

## Step-by-Step Procedure

### 1. Run Read-Only Diagnosis
```bash
./resources/tools/updater.py doctor          # Human-readable 9-point report
./resources/tools/updater.py doctor --json   # Machine-readable JSON report (CI/scripts)
```
Review the 9-point report:
- **Metadata drift** (`version`, `steam-build-id`, `assembly_sha256` in `config.json`)
- **`dump.cs` freshness** (must be newer than `GameAssembly.dll` after a patch)
- **IL2CPP exports** (all 8 required `il2cpp_*` exports present)
- **Tier 1 AOB signatures** (`[OK]`, `[SHIFT]`, `[BROKEN]`, or `DORMANT`)
- **Tier 2/3 methods & field offsets** vs `dump.cs` (`[SHIFT]`, `[MISSING TYPE/METHOD/FIELD]`)
- **Enum drift** (`TypeVisualEffects`, `TeamRelation`, `CurrencyEnum`, `ResourceEnum`)

### 2. Auto-Heal & Rebuild
If `doctor` reports only metadata drift, shifted RVAs/offsets, or broken AOBs (no renamed C# types/fields):
```bash
./resources/tools/updater.py autoheal --build
```
This automatically:
1. Updates `config.json` (`version` numeric-only without `v`, `steam-build-id`, `assembly_sha256`, `signatures_summary`, `signatures`).
2. Regenerates any `[BROKEN]` Tier 1 AOBs from `dump.cs` + `GameAssembly.dll` via `resources/tools/aob_gen.py` (verified unique across all `IMAGE_SCN_MEM_EXECUTE` PE sections: `.text`, `il2cpp`).
3. Patches all shifted `RVA::k*`, `offsets::k*`, and healed `signatures::k*` in `src/utils/offsets.h`, and safely appends newly added enum members when no existing values conflict.
4. Rebuilds `PG2-Cheetos.dll` and `PG2-Cheetos_Injector.exe` via `./resources/scripts/build release --injector` (`CMakeLists.txt` auto-reconfigures on `config.json` changes via `CMAKE_CONFIGURE_DEPENDS`).

### 3. Handle Major Refactors (If `doctor` Reports Missing Symbols or Enum Drift)
When a patch renames a C# class/namespace/field/method or alters enum values:
1. Inspect the replacement type/member in `resources/dumped/dump.cs`.
2. Check for **same-arity overloads** (e.g. `MatchPresentation::TryGetGameModePlayerData(EntityRef, ...)` vs `(PlayerId, ...)`): bind via unique Tier 1 AOB + Tier 3 Static RVA and skip Tier 2 name-only lookup so the wrong overload is never bound.
3. Update:
   - `src/utils/offsets.h` (`fields::`, `offsets::`, `RVA::`, `signatures::`)
   - Affected feature files under `src/features/` or enums under `src/data/`
   - `resources/tools/lib/registry.py` (`TRACKED_METHODS`, `TRACKED_FIELDS`, `TRACKED_ENUMS`)
4. Generate/verify any new Tier 1 AOBs:
   ```bash
   ./resources/tools/updater.py gen-aob ClassName::MethodName
   ./resources/tools/aob_gen.py --verify "<IDA pattern>"
   ```
   *(Skip Tier 1 AOB for Unity icall boilerplate wrappers and tiny thunks whose wildcarded assembly is non-unique; rely on Tier 2 IL2CPP + Tier 3 Static RVA for those.)*
5. Re-run `./resources/tools/updater.py autoheal --build` and confirm `./resources/tools/updater.py doctor` reports 0 critical issues.

### 4. Final Verification
- Confirm both `Release` and `Debug` builds compile cleanly if C++ code changed:
  ```bash
  ./resources/scripts/build debug --injector
  ./resources/scripts/build release --injector
  ```
- Confirm `config.json` `"version"` is numeric-only (no leading `v`) and `git diff` contains no hardcoded local host paths.

