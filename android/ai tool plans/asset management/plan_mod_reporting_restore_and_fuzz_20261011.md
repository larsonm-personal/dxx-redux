# Mod reporting, restore safety, menu recovery and overnight coverage

## Requested scope

Implement switchable mod-usage diagnostics using an existing logging category and fix the menu crash observed after a failed Vertigo restore. Design, without implementing yet, save-safe toggling for supported presentation mods and overnight mod-combination fuzz testing

Do not edit `android/outstanding_bugs.md`

## Work plan

- [x] Gate mod-usage work and messages behind an existing category, including periodic reports and explicit introspection behavior; retain asset precedence and loading independent of logging
- [x] Trace the null window in the menu mouse path and fix its lifetime/initialization cause in both engines; reproduce and verify recovery/quit
- [x] Extend meaningful existing coverage, run relevant CMake builds/tests and scoped formatting
- [x] Specify native restore policy for changed effect definitions, preserving gameplay state and strict structural validation
- [x] Specify deterministic seeded overnight coverage using real recommended packs, launcher transitions, bounded cases and durable crash evidence

## Evidence

Build 24951 (`2c4938325`) on SM-S931U: 21 gameplay hitch windows of about 51-100 ms with 34-86 ms in the introspection stage. Mod tracking includes 208 sounds totaling 5,942,831 PCM bytes. `android_mod_usage_snapshot()` hashes every tracked sample and `android_mod_usage_flush()` runs from the game thread at one-second minimum intervals when dirty

Two restores of `Players/save_sets/single/player/d2x/player.sg8` fail at `stage=effects` after UUD2 mods are disabled. UUD2TP changes existing eclip definitions and adds effects 105 and 106. Exact rejected save record is not in the supplied logs

The supplied native tombstone shows `window_close(NULL)` via `newmenu_mouse`, with fault address 0x68, after the failed restore. The exact release binary matches build ID `da9302cf8969b8085d04d3f7a576c66028c88cff`; disassembly confirms the null menu-window argument at `0x74f1e8`

The startup fallback calls `DoMenu` while the rejected restore's `Game_wind` is still pending closure. `window_create` links the new menu before deactivating the old game. Deactivation consumes the restore failure request, closes the game and creates another menu, then `longjmp(LeaveEvents)` interrupts the first menu before assigning `menu->wind`. This leaves an incomplete extra menu that can crash on touch Quit

Both engines now enter the event loop first when startup restore leaves a game window, allowing the existing failure handler to unlink it before constructing its replacement menu

## Delivered changes and validation

Mod reporting uses the existing Game Logs category (`DLOG_GAME`). Disabled logging skips playback fingerprints, periodic report hashing/writes and explicit introspection hashing. Asset loading and precedence remain independent; the launcher labels the retained report as the last recorded use

Native CMake builds for both x86_64 engines and the Android debug APK build passed. The actual engine sound fixture passed all 11 D2 cases and its D1 case, including disabled-report playback, unchanged report bytes while disabled, and report publication after re-enabling

`test_startup_restore_failure.ps1` passed on the disposable x86_64 emulator for D1 and D2. It creates a real exit autosave, truncates the final native runtime record, resumes through the launcher's cached candidate into a fresh game process, verifies post-world runtime rejection and one remaining menu, then taps Quit using introspected geometry and verifies clean process exit. This covers the same startup recovery path without depending on an unavailable user save. Registered it in the core suite with a 480-second master timeout

Automation catalog validation and catalog integration passed. The first catalog integration attempt hit its host-side 45-second invocation timeout during the concurrent build; the subsequent run passed

Scoped mixed-language formatting/lint and `git diff --check` passed. The disposable emulator was shut down after testing; no physical-device installation was performed

Save compatibility and overnight fuzzing remain designs, in `design_mod_save_compatibility_20261011.md` and `design_overnight_mod_fuzz_20261011.md`. No overnight campaign has been started
