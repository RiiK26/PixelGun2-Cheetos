## Summary
<!-- Briefly describe what this PR does and why. -->

## Changes
<!-- List the key changes made in this PR. -->
-

## Verification
- [ ] `Debug` build with `-DBUILD_INJECTOR=ON` compiles cleanly
- [ ] `Release` build with `-DBUILD_INJECTOR=ON` compiles cleanly
- [ ] `clang-format` 23 applied to modified `src/` files (excluding `src/vendor/`)
- [ ] `./resources/tools/updater.py doctor` passes (if offsets/AOBs/enums changed)
- [ ] No hardcoded local host paths or edits under `src/vendor/`
- [ ] Tested in-game (if applicable)
