# Autoselect Only Once backport

Upstream: `8537b01e90e683fcde9aa34ed8b1574e8651bc2a`

- [x] Import upstream pickup behavior, per-ship resets, pilot setting, and native menus for both games
- [x] Persist pickup flags through saves/rewind/checkpoints and capture the setting in input recordings
- [x] Add integration coverage for pickup, firing, fallback, reset, and persistence behavior
- [x] Run scoped quality checks, both host builds/tests, and Android native builds

The setting defaults off and is available in the engine Misc Options menu. Its upstream meaning is one first pickup per category per ship, including pickups that do not switch weapons. Keep upstream names and patch structure. Add local persistence alongside existing save runtime extensions; preserve legacy upstream saves and D2 secret-level inventory semantics. The Android launcher continues to edit weapon ordering independently.

## Implementation and merge notes

- Import pickup suppression, per-local-ship resets, native Misc Options checkboxes, default-off configuration, and the `autoselectonlyfirstweaponpickup` pilot-text key from upstream `8537b01e90e683fcde9aa34ed8b1574e8651bc2a`
- Retain upstream names and behavior: first successful pickup of a different primary/secondary consumes that category even when priority or firing policy prevents a switch; duplicates/current-weapon refills do not consume it. Empty selected secondary ammo still allows autoselection. Manual selection and empty-weapon autoselection remain governed by their existing rules
- Adjust native menu indices around the existing local Original Homing entries without losing their callbacks
- Append four integers to the existing save runtime extension: the two pickup flags and the two queued firing-release selections. Save versions are D1 17 / D2 31. Shared validation and serialization live in `android/app/src/main/cpp/shared/autoselect_runtime.h`; the small state.c hooks cover normal saves, in-memory rewind, and input-demo checkpoints
- Older saves lack pickup history, so ordinary restores reset it and the pending selections. D2 secret restores consume the extension without applying it, keeping the surviving ship's history (or the reset history after death)
- Capture and apply the option in input-demo settings. Add pickup flags to runtime diagnostics and their hash
- Expose the setting through the native Options > Misc Options menu, consistent with upstream. The launcher ordering editor continues to use its existing engine-owned ordering API

## Validation

- D1/D2 MSVC builds succeeded; the final integration-target rebuild introduced no compiler warnings
- All 107 CTest cases passed (D1 50, D2 57)
- Extended the actual-engine upstream compatibility target with pickup behavior, independent categories, default-off behavior, manual selection, missile fallback, current/duplicate/below-cutoff pickups, firing suppression, local versus remote ship resets, pilot-text roundtrip, and queued selection roundtrip through both PhysFS files and rewind memory buffers
- Runtime reader tests cover byte swapping, truncated records, invalid queued indices, and read-without-apply behavior used by D2 secret returns. These exercise the production extension helper; full rendered save/secret-level transitions were not playtested
- Input-demo fixture and recorder tests exercise the new setting's JSON roundtrip and recording output
- Scoped mixed-language quality checks passed; the new integration source was also formatted with clang-format
- Android arm64 D1/D2 native libraries built successfully with `:app:externalNativeBuildDebug -Pandroid.injected.build.abi=arm64-v8a`

## Launcher preset follow-up

Both Original Descent and Restore Defaults now list Autoselect Only Once as Off and reset it when the confirmed preset is saved. The preset-only flag follows the existing launcher save transaction into both games' native pilot writers; ordinary preference saves preserve the existing value. Pilot text format details remain in the shared native playsave helper.

Validation: Android debug Kotlin compilation and both arm64 native libraries succeeded; all 20 targeted settings/config tests and four existing host playsave text/transaction tests passed. Scoped formatting and lint checks passed. Device UI was not exercised.
