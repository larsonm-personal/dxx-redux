# DXA multi-mod assessment plan

## Goal
- Answer whether Android launcher and engine support multiple DXA mods, including ordering
- Interpret xfing's rationale for not shipping the texture pack as a DXA against the current port design

## Tasks
- [x] Check launcher mod list and `.active_mod_paths` writing
- [x] Check engine PhysFS mount behavior and path precedence
- [x] Compare current plain texture DXA work with xfing's D1 monitor/template note
- [x] Summarize whether this suggests any design changes
- [x] Enforce launcher-visible DXA order in Android PhysFS mounting
- [x] Add launcher preflight detection for conflicting patch DXAs
- [x] Add expanded mod-list load order label

## Findings
- Android `ModManager` supports multiple enabled DXA files and writes all valid enabled paths to `.active_mod_paths` in saved order
- Both D1 and D2 Android PhysFS initialization read every non-empty line in `.active_mod_paths` and mount each DXA
- Engine mounts each listed mod with `PHYSFS_mount(..., append=0)`, so each mount is prepended; if multiple mods contain the same path, the later line in `.active_mod_paths` has higher priority
- The UI has move-up/move-down controls, but the effective conflict priority is likely bottom-most wins unless the launcher writes paths in reverse or the engine changes mount behavior
- Sound and texture DXAs can coexist cleanly when they do not provide the same virtual paths
- Ordinary texture replacement files compose by PhysFS path lookup, but D2 HAM metadata patches currently use one fixed path, `patches/d2/ham_patch.rfc6902.json`; multiple independent HAM-patching DXAs do not compose today
- Xfing's monitor note points at an animation-metadata issue: frame-by-frame replacement can fake a different frame count by repeating frames, while a cleaner generic solution would need semantic animation patching or an integrated upstream data fix

## Follow-up implementation
- Android D1 and D2 PhysFS initialization now reads `.active_mod_paths` in launcher order but mounts it in reverse with prepend, so the final search path gives higher rows priority
- The expanded launcher mod section now states that load order is top to bottom and higher enabled mods take priority when files overlap
- Launcher preflight now scans enabled DXAs for `patches/*.rfc6902.json` entries and manifest `compatibility.requiredBaseFiles[].patchPaths`; launch is blocked if more than one enabled mod owns the same patch path
- Archive-entry patch conflict scanning is filtered by launched game, so D2-only patch files do not block D1 launches from a `both` mod

## Validation
- `android\run-code-quality.ps1 -Fix -Paths d1/misc/physfsx.c,d2/misc/physfsx.c,android/app/src/main/java/com/dxxredux/app/ModManager.kt,android/app/src/main/java/com/dxxredux/app/SetupActivity.kt,"android/ai tool plans/plan_dxa_multi_mod_assessment_20260518.md"`
- VS Code diagnostics reported no errors for the touched Kotlin and PhysFS files
- `android\gradlew.bat ':app:compileDebugKotlin' ':app:buildCMakeDebug[arm64-v8a]' ':app:buildCMakeDebug[arm64-v8a]-2'`
- `run-windows-build.ps1`
- `ctest --test-dir buildd1 --output-on-failure` and `ctest --test-dir buildd2 --output-on-failure`; no tests registered, exit code 0
- `android\gradlew.bat ':app:compileDebugKotlin'` after the final conflict-filter tweak