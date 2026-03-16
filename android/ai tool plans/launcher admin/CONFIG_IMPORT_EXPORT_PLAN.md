# Plan: Config Export/Import and JSON-Based Defaults

## TL;DR
Move hard-coded touch and controller defaults out of Kotlin/C source into human-readable JSON files bundled in `assets/configs/`. Add import/export of full and partial configs via SAF picker and filesystem scan. Touch preset JSON files are scanned at startup so new presets can be added by dropping in files. All JSON uses human-readable names (not raw integer constants) with a translation layer for import/export.

Bundled presets are read-only templates: they're copied into `filesDir/` on first launch or reset. All editing happens on the user's copy.

---

## Phase 1: Human-Readable JSON Format & Translation Layer

**Goal**: Define a human-readable JSON dialect for configs that maps binding names to/from internal integer constants.

### Steps

1. **Add name-lookup tables to `TouchBindings.kt`** -- add reverse maps (`Int -> String`, `String -> Int`) for button bindings, axis indices, and meta actions. These already partially exist (`BUTTON_LABELS`, `AXIS_LABELS`) but need bidirectional lookup and coverage for all binding types including `KeyEvent.KEYCODE_*` used in radial menus.
   - `bindingToName(id: Int): String` -- returns e.g. `"Fire Primary"`, `"KEYCODE_1"`, `"Meta: Quick Save"`
   - `nameToBinding(name: String): Int` -- reverse
   - `axisToName(id: Int): String` / `nameToAxis(name: String): Int`

2. **Create `HumanReadableConfig.kt`** -- a utility object with:
   - `touchLayoutToHumanJson(layout: TouchLayout): JSONObject` -- converts a TouchLayout to JSON where all `binding` and `axis*` fields use string names instead of integers
   - `humanJsonToTouchLayout(json: JSONObject): TouchLayout` -- parses human-readable JSON back, translating names to integers. On unknown name: logs error with field name, skips element, continues loading
   - `controllerConfigToHumanJson(bindings: Map<String,String>, inverts: Set<String>): JSONObject` -- controller config is already mostly human-readable (uses function labels). Just ensure format consistency
   - `humanJsonToControllerConfig(json: JSONObject): Pair<Map<String,String>, Set<String>>` -- parse with validation
   - Error reporting: return a `ParseResult<T>` with the parsed value (or null) plus a list of warning/error strings

3. **JSON format spec** (human-readable touch layout example):
   ```json
   {
     "type": "touch_layout",
     "version": 1,
     "name": "Simple",
     "globalOpacity": 0.7,
     "sticks": [{
       "id": "move",
       "x": 18.0, "y": 75.0,
       "axisX": "Left Stick X",
       "axisY": "Left Stick Y"
     }],
     "buttons": [{
       "id": "fire1",
       "binding": "Fire Primary"
     }],
     "radialMenus": [{
       "segments": [{"label": "Energy", "binding": "KEYCODE_1"}]
     }]
   }
   ```

4. **JSON format spec** (human-readable controller config example):
   ```json
   {
     "type": "controller_config",
     "version": 1,
     "bindings": {
       "A": "Fire Primary",
       "B": "Fire Secondary",
       "RS_X": "Turn L/R"
     },
     "inverts": ["RS_Y"]
   }
   ```
   Note: the controller config `bindings` field already uses human-readable strings. The main addition is the `"type"` field for distinguishing config types on import.

**Relevant files**:
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt` -- add bidirectional name maps
- NEW: `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt` -- translation utilities

---

## Phase 2: Extract Defaults to Bundled JSON Files

**Goal**: Move hard-coded preset definitions from Kotlin source into JSON files in `assets/configs/`, loaded at runtime. Bundled presets are read-only templates; they are copied into `filesDir/` on first launch or reset.

### Steps

5. **Create asset directory structure**:
   - `android/app/src/main/assets/configs/touch/` -- one `.json` file per touch preset
   - `android/app/src/main/assets/configs/controller/` -- one `.json` file per controller preset

6. **Generate initial JSON files** from existing presets:
   - `assets/configs/touch/simple.json` -- from `presetSimple()`, using human-readable format
   - `assets/configs/touch/advanced.json` -- from `presetAdvanced()`
   - `assets/configs/touch/claw.json` -- from `presetClaw()`
   - `assets/configs/controller/default.json` -- from `DEFAULT_BINDINGS` map in ControllerConfigPage.kt
   - The `"name"` field inside the JSON is the display name for the preset

7. **Refactor `TouchLayoutRepository.kt`**:
   - Remove `presetSimple()`, `presetAdvanced()`, `presetClaw()` factory methods and radial menu factories
   - Add `loadBundledPresets(context: Context): List<TouchLayout>` -- scans `assets/configs/touch/`, parses each JSON file via `HumanReadableConfig.humanJsonToTouchLayout()`, returns list
   - Add `loadUserPresets(context: Context): List<TouchLayout>` -- scans `filesDir/configs/touch/` for user-imported presets
   - Change `allPresets()` to `allPresets(context: Context)` -- returns bundled + user presets
   - `defaultLayout()` becomes `defaultLayout(context: Context)` -- loads first bundled preset (simple.json)
   - Keep fallback to a minimal hard-coded layout if all JSON parsing fails (defensive)

8. **Refactor controller defaults**:
   - Move `DEFAULT_BINDINGS` in `ControllerConfigPage.kt` to be loaded from `assets/configs/controller/default.json` at startup
   - `ControllerConfigPage` loads bundled controller presets for a "Reset to Default" or preset picker
   - `android_gamepad_config.cpp` C-side defaults remain as ultimate fallback (when no JSON and no controller_config.json exist), but are no longer the primary source

**Relevant files**:
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt` -- refactor preset loading
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` -- load defaults from JSON
- NEW: `android/app/src/main/assets/configs/touch/simple.json`
- NEW: `android/app/src/main/assets/configs/touch/advanced.json`
- NEW: `android/app/src/main/assets/configs/touch/claw.json`
- NEW: `android/app/src/main/assets/configs/controller/default.json`

---

## Phase 3: Export Functionality

**Goal**: Allow exporting the current config (full or single preset) to a shareable JSON file.

### Steps

9. **Add export functions**:
   - `exportTouchLayout(context, layout) -> File` -- writes human-readable JSON to cache dir for sharing
   - `exportControllerConfig(context) -> File` -- reads current controller_config.json, converts to human-readable format
   - `exportFullConfig(context) -> File` -- combined JSON with both touch + controller

10. **Combined export format**:
    ```json
    {
      "type": "full_config",
      "version": 1,
      "touch_layout": { ... },
      "controller_config": { ... }
    }
    ```

11. **Add export UI buttons** in the launcher:
    - "Export" buttons on touch config page and controller config page -- uses Android share intent
    - "Export All Settings" accessible from both pages

**Relevant files**:
- NEW: `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt` -- central import/export logic
- Touch and controller config page Composables

---

## Phase 4: Import Functionality

**Goal**: Import configs via SAF picker or filesystem scan. Handle full configs, single touch layouts, and single controller configs.

### Steps

12. **Detect config type on import** by checking `"type"` field:
    - `"full_config"` -- apply both touch + controller sections
    - `"touch_layout"` -- prompt: "Apply as active layout" or "Add as preset option"
    - `"controller_config"` -- prompt: "Apply as active config" or "Add as preset option"
    - Missing `"type"` -- try to infer from presence of `"sticks"`/`"buttons"` (touch) vs `"bindings"` (controller)

13. **Import flow**:
    - Load defaults first (from bundled JSONs)
    - Apply incoming settings as a patch on top (missing fields keep default values)
    - For "Add as preset": copy the JSON file into `filesDir/configs/touch/` or `filesDir/configs/controller/` so it appears in the preset list

14. **SAF file picker integration**:
    - Register file picker for `.json` files
    - Read content, parse, detect type, apply or prompt

15. **Directory scan import**:
    - On launcher startup, scan `filesDir/import/` for any `.json` config files
    - Process each: detect type, show notification/toast, move to appropriate location
    - Delete from `import/` after processing (or rename to `.imported`)

16. **Error handling**:
    - On parse error: show toast/dialog with specific message (e.g., "Touch config parse error: unknown binding name 'Frie Primary' in button 'fire1'")
    - On partial success: apply what parsed successfully, list warnings
    - Never crash on bad input

17. **Add import UI** in launcher:
    - "Import Config" button accessible from config pages
    - SAF picker launches, user selects file
    - Type detection -> appropriate dialog

**Relevant files**:
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- import dir scan on startup
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt` -- accept imported presets
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` -- import button + logic

---

## Phase 5: Wire Up & Test

### Steps

18. **Update preset picker in touch config UI** to show bundled + user presets loaded dynamically

19. **Update controller config "Reset to Defaults"** to load from bundled JSON

20. **Integration tests**:
    - Round-trip test: export a touch layout -> re-import -> compare equality
    - Round-trip test: export controller config -> re-import -> compare
    - Parse error test: intentionally bad JSON -> verify error message, no crash
    - Preset scanning test: place N JSON files in test assets dir -> verify N presets loaded
    - Human-readable translation test: verify all binding names survive round-trip translation

21. **Build and verify**: cmake build + Android build + code quality checks

---

## Decisions

- **Bundled presets are read-only templates**: `assets/` files are never the live config. On first launch (or reset), the selected bundled preset is *copied* into `filesDir/` as the active config. All edits happen on the `filesDir/` copy. "Reset to defaults" re-copies from the bundled template.
- **Preset selection copies, not references**: Choosing a preset from the picker copies its content into the active config file. The user then edits freely.
- **Controller defaults stay in C as ultimate fallback**: `android_gamepad_config.cpp` keeps its hard-coded defaults for the case where no JSON exists and no controller_config.json has been written.
- **The `"type"` field is added to all exported JSON** to enable automatic type detection on import.
- **Radial menu `binding` fields** that use `KeyEvent.KEYCODE_*` integers will be exported as `"KEYCODE_1"`, `"KEYCODE_2"` etc.
- **No backwards compatibility concern** per project instructions (pre-first-release).
- **Internal `touch_layout.json` keeps integer format** -- human-readable translation is only for import/export/bundled asset files.

---

## Implementation Status

### Completed

- **Phase 1**: Translation layer in `TouchBindings.kt` (bidirectional name maps) and `HumanReadableConfig.kt` (full parse/serialize)
- **Phase 2**: 4 bundled JSON preset files created (3 touch + 1 controller), `TouchLayoutRepository.kt` refactored for asset-based loading, `ControllerConfigPage.kt` loads defaults from bundled JSON
- **Phase 3**: `ConfigImportExport.kt` created with export functions (touch, controller, combined) using Android share intents and `FileProvider`
- **Phase 4**: Import functionality via SAF file picker, wired up in all 3 pages (controller config, touch editor, setup screen)
- **Phase 5**: UI buttons added to all config pages, build passes, code quality checks pass

### Files Created
- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt`
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- `android/app/src/main/assets/configs/touch/simple.json`
- `android/app/src/main/assets/configs/touch/advanced.json`
- `android/app/src/main/assets/configs/touch/claw.json`
- `android/app/src/main/assets/configs/controller/default.json`
- `android/app/src/main/res/xml/file_paths.xml`

### Files Modified
- `TouchBindings.kt` -- added reverse lookup maps and bidirectional functions
- `TouchLayoutRepository.kt` -- refactored from hard-coded presets to asset-based loading
- `TouchEditorPage.kt` -- added Export/Import buttons, SAF picker launcher
- `ControllerConfigPage.kt` -- loads defaults from bundled JSON, added Export/Import buttons
- `SetupActivity.kt` -- added Export All / Import buttons in ControllerSection
- `AndroidManifest.xml` -- added FileProvider for sharing exported files

### Not Yet Done
- Directory scan import on launcher startup (step 15)
- Automated integration tests (step 20)
- "Add as preset" flow on import (currently imports replace the active config directly)
