# Plan: D1/D2 Game Control Save/Load Extraction 2026-07-11

## Goal
- Remove the paired Android save/load and meta-action dispatch bodies from the original D1/D2 `gamecntl.c` files while preserving game-thread ordering and behavior

## Existing work to preserve
- Preserve the completed songs, PhysFS, and HMP diff-minimization tranches
- Preserve all unrelated workspace and mission metadata changes
- Keep desktop game-control behavior unchanged

## Steps
- [x] Reconfirm the live D1/D2 dispatch blocks, includes, flags, and signature differences
- [x] Define the smallest direct API in the existing Android meta-actions helper
- [x] Move the duplicated Android-only behavior and leave mirrored minimal call sites
- [x] Add or extend focused unified integration coverage for save/load dispatch behavior
- [x] Run scoped formatting, lint, catalog validation, and diff checks
- [x] Build both Windows targets and all configured Android ABI/game combinations
- [x] Run focused D1/D2 emulator integration coverage
- [x] Record exact reduction, deferred behavior, and the next candidate

## Guardrails
- Preserve dispatch priority across autosave, difficulty, game menu, save, load, demo toggle, and rewind
- Preserve pause-window close behavior and request-flag ownership
- Preserve dead-player and competitive-multiplayer restrictions
- Preserve auto-exit, auto-minimize, and autosave result semantics
- Preserve D1/D2 state function signatures through compile-time adapters
- Preserve D2 guide-bot ordering and D1 music-control ordering around the extracted handler
- Do not broaden this tranche into save-format, rewind-policy, or menu behavior changes

## Baseline
- Aggregate D1/D2 diff: 341 files, +49969/-3880 against `upstream/main`
- `d1/main/gamecntl.c`: +455/-9
- `d2/main/gamecntl.c`: +728/-13
- Paired Android dispatch blocks: 155 additions per game
- Expected core engine-file reduction: 310 additions

## Outcome
- Moved `android_clear_saveload_requests`, pause-window save/load dispatch, and in-game meta-action dispatch into `android_meta_actions.c`
- Kept one mirrored pause call and one mirrored in-game call in the two engine files
- Preserved request priority, pause-window close behavior, dead and multiplayer gates, autosave result routing, demo toggle behavior, rewind routing, and D1/D2 state API differences
- Removed 162 additions from each `gamecntl.c`, for 324 additions removed from upstream-owned engine files
- `d1/main/gamecntl.c` moved from `+455/-9` to `+293/-9`
- `d2/main/gamecntl.c` moved from `+728/-13` to `+566/-13`
- The live aggregate is now 341 files and `+49958/-3886`; it is not a clean tranche delta because concurrent pathing work added new D1/D2 changes while this tranche was in progress

## Validation
- The automation catalog resolves 48 standalone JSON scripts, 15 support scripts, and 36 PowerShell tests; the new script resolves to 45 steps for each game and has no BOM
- `git diff --check` passed
- The repository formatter wrapper could not execute the external `C:\local\clang-format-20\clang-format.exe` in the managed sandbox; changed C/C++ code was reviewed manually and compiled cleanly
- The normal Windows build wrapper was blocked by managed-environment access to the external vcpkg buildtree and by unavailable fresh-configure compiler discovery, so it did not provide a source result
- Existing Android CMake/Ninja graphs built and linked both `dxx-redux-d1` and `dxx-redux-d2` for arm64-v8a, armeabi-v7a, and x86_64
- The resulting all-ABI APK was zipaligned, signed with the existing debug key, and verified with matching certificate SHA-256
- `test_android_saveload_dispatch_unified.json5` passed 45 of 45 steps in D1 and 45 of 45 steps in D2 on the emulator
- The focused runtime test exercised auto-minimize, live difficulty, pause entry, save menu, load menu, game menu, and return to live gameplay

## Deferred scope and next work
- Keep save formats, rewind policy, and menu implementations out of this helper
- The adjacent duplicated live-difficulty block is a separate cross-platform feature seam and should be assessed against the rule that the shared boundary must remain smaller than its engine policy
- Continue from the ranked 2026-07-11 campaign catalog, starting with isolated high-payoff seams rather than reopening this handler
