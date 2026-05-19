# DXA Mod Hash Integration Plan

## Goal
Integrate .dxa texture/sound packs into the hash-based file loading system so they're
loadable by the game scripting deps system. Add a test that loads mods and verifies
enhanced files are used.

## Phases

### Phase 1: Update generate_game_data_index.ps1
- [x] Add `.dxa` to `$GameExtensions`
- [x] Add `game_data/mods` to `$SearchDirs`

### Phase 2: Build DXA files from source archives
- [x] Run convert_d2xxl_sounds.ps1 -Game d2 (produces d2xxl-hires-sounds-d2.dxa)
- [x] Run convert_d2xxl_textures.ps1 -Game d2 (produces d2xxl-hires-textures-d2.dxa)
- [x] Run generate_game_data_index.ps1 to hash all DXAs

### Phase 3: Add mod introspection to game_introspect.cpp
- [x] Add PhysFS search path listing to introspection JSON
- [x] Report mounted mod paths so tests can assert mod loading worked

### Phase 4: Create test script
- [x] Add DXA mod deps with target "files/mods" to test_fire_primary.json5 (or new test)
- [x] Add write_config step to create mods/mod_manifest.json and d2x-redux/.active_mod_paths
- [x] Add assertion that mod files appear in search path via introspection

### Phase 5: Build and lint
- [x] Gradle build (BUILD SUCCESSFUL)
- [x] run-code-quality.ps1 (all checks passed)

### Phase 6: Run test on emulator
- [x] Deploy APK
- [x] Fixed injectTapAt not triggering Compose onClick -- added performAccessibilityClick
- [x] Fixed mission select timeout -- added `optional` flag to select action
- [x] Fixed $dep.target PSCustomObject indexing in test_helpers.ps1
- [x] Fixed write_config parent dir creation in LauncherScriptExecutor.kt
- [x] Fixed mkdir -p for non-external targets in Resolve-GameDataDeps
- [x] test_mod_loading.json5: PASS (20 steps, 7028ms)
- [x] DXA pushed to files/mods/, mod_manifest.json written, mounted_mods assertion passed

### Phase 7: Update plan file with results
- [x] Done

## Files changed
- game_data/generate_game_data_index.ps1 -- added .dxa extension, game_data/mods search dir
- game_data/game_data_index.txt -- regenerated with 4 new DXA entries
- game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1 -- fixed nested Join-Path for PS 5.1
- game_data/mods/d2x-xl/convert_d2xxl_textures.ps1 -- fixed nested Join-Path for PS 5.1
- android/app/src/main/cpp/shared/game_introspect.cpp -- added mounted_mods from PhysFS
- android/app/src/main/cpp/shared/game_automate.cpp -- added `contains` assertion op, `optional` select flag
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt -- added performAccessibilityClick
- android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt -- use performAccessibilityClick, fix write_config mkdirs
- android/test_helpers.ps1 -- fixed $dep.target, added mkdir -p for non-external targets
- android/game_scripts/test_mod_loading.json5 -- new test script
