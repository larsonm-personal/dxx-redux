# D1-In-D2 Full Support Design Study

## Goal
Design a robust path for playing D1 missions inside the D2 executable, with correct D1 base assets, D1 custom texture packs, and compatibility with D1 level pack conventions.

## Checklist
- [x] Trace D2 mission selection and `EMULATING_D1` activation
- [x] Trace D2 PhysFS search-path and mission HOG mounting behavior
- [x] Compare D1 and D2 base asset loading responsibilities
- [x] Compare D1 custom texture/robot loaders against D2 replacement loaders
- [x] Identify D1 level pack variants and texture formats to support
- [x] Design base-data routing so D2 can prefer D1 assets when appropriate
- [x] Define implementation phases, risks, and tests

## Short Answer

The D1-in-D2 path is the cleaner route for the immediate goal of playing Trine 2 with the Guide-Bot. D2 already has:

- Guide-Bot spawning through `escort_spawn_at_player()` and Android overlay integration.
- D1 mission recognition through `Current_mission->descent_version == 1`.
- D1 level texture-number conversion through `convert_d1_tmap_num()`.
- Stock D1 texture replacement from `descent.pig` through `load_d1_bitmap_replacements()`.
- Android mission ZIP staging that already enables D1 mission ZIPs when launching D2.

The main missing piece is D1 custom texture loading inside the D2 executable. Trine 2 sets `custom_textures = yes` and ships ten per-level `.dtx` files in `trine2.hog`. The D1 executable loads `.pg1`, `.dtx`, and `.hx1` in `d1/main/custom.c`, but the D2 executable only loads stock D1 texture replacements for `EMULATING_D1` and skips those per-level D1 custom texture files.

The base-data request should be implemented as a D1 asset overlay inside D2, not as a literal replacement of D2 base files. D2 still needs D2 base data for the D2 executable, object definitions, sounds, UI, and Guide-Bot support. When a D1 mission is run under D2, D1 `descent.hog` and `descent.pig` should be preferred for D1 mission resources, palettes, briefings, exit assets, and stock D1 texture replacements.

## Current Runtime Shape

### Native D2 Startup

`d2/main/inferno.c` hard-requires `descent2.hog` or `d2demo.hog` at startup, then later initializes D2 game data and loads `groupa.pig`:

- `PHYSFSX_contfile_init("descent2.hog", 1)` or `PHYSFSX_contfile_init("d2demo.hog", 1)`.
- `load_text()`.
- `PHYSFSX_addArchiveContent()`.
- `gamedata_init()`.
- `piggy_init_pigfile("groupa.pig")`.

This means D2 cannot simply be pointed at only D1 files without larger engine surgery. A minimal, robust design keeps the D2 base set available and overlays D1 files when the selected mission is D1.

### Mission ZIP And Mod Mounting

Android `ModManager.writeEnabledModPaths(game)` writes enabled mission ZIP stage roots and DXA paths to:

- `d1x-redux/.active_mod_paths` for D1 launches.
- `d2x-redux/.active_mod_paths` for D2 launches.

For D2 launches, D1 mission ZIP mods are intentionally included:

```kotlin
(game == "d2" && kind == MOD_KIND_MISSION_ZIP && this.game == "d1")
```

`d2/misc/physfsx.c` reads `d2x-redux/.active_mod_paths` and mounts those paths during PhysFS init. For mission ZIPs, this makes the staged mission root visible to the D2 mission loader. Large/extracted mission ZIPs are also represented as a durable extraction root, so this path scales beyond small ZIPs.

### D1 Mission Activation In D2

`d2/main/mission.h` defines:

```c
#define EMULATING_D1 (Current_mission->descent_version == 1)
```

`d2/main/mission.c::load_mission()` mounts `descent.hog` when `EMULATING_D1`:

```c
if (EMULATING_D1) {
    if (!PHYSFSX_contfile_init("descent.hog", 1))
        Warning("descent.hog not available...");
    if (!d_stricmp(Current_mission_filename, D1_MISSION_FILENAME))
        return load_mission_d1();
}
```

For non-builtin missions, the loader opens the `.msn`, then swaps the extension to `.hog` and mounts that sibling HOG. This matches Trine 2's layout after mission ZIP staging:

- `trine2.msn`
- `trine2.hog`

### D1 Level Loading In D2

`d2/main/gameseq.c::LoadLevel()` currently does:

```c
load_level(level_name);
load_palette(Current_level_palette, 1, 1);
load_endlevel_data(level_num);

if (EMULATING_D1)
    load_d1_bitmap_replacements();
else
    load_bitmap_replacements(level_name);

load_level_robots(level_num);
piggy_load_level_data();
ogl_cache_level_textures();
```

This is the key behavior gap:

- D2 missions load D2 `.POG` replacements via `load_bitmap_replacements(level_name)`.
- D1 missions in D2 load stock D1 replacements from `descent.pig` via `load_d1_bitmap_replacements()`.
- D1 missions in D2 do not load D1 per-level `.pg1`, `.dtx`, or `.hx1`.

Trine 2 relies on those `.dtx` files, so the level is structurally playable but visually wrong.

## Current Texture Systems

### D2 Stock D1 Texture Support

`d2/main/gamemine.c` detects whether `descent.pig` is available:

```c
d1_pig_present = PHYSFSX_exists(D1_PIGFILE, 1);
```

`convert_d1_tmap_num()` maps D1 texture numbers to D2 texture numbers. If `descent.pig` is present, unique D1 textures map to dedicated D2 slots so D2 can preserve D1 visuals instead of falling back to similar D2 textures.

`d2/main/piggy.c::load_d1_bitmap_replacements()` opens `descent.pig`, reads D1 bitmap headers and the D1 palette, remaps them to the active D2 palette, and writes the D1 bitmaps into the D2 bitmap slots selected by the conversion table.

This already covers stock D1 textures reasonably well.

### D2 Per-Level Texture Support

`d2/main/piggy.c::load_bitmap_replacements(level_name)` only handles D2 `.POG` files:

- Extension searched: `.POG`.
- Format: `DPOG` signature, version 1.
- Replacement addressing: bitmap indices.

It does not handle D1 `.DTX` or `.PG1`, and it is not called at all for `EMULATING_D1`.

### D1 Per-Level Custom Support

`d1/main/custom.c::load_custom_data(level_name)` does:

```c
custom_remove();
load_pigpog("<level>.pg1");
load_pigpog("<level>.dtx");
load_hxm("<level>.hx1");
```

The D1 loader supports two texture-file families:

- Old D1 PIG-like files, used by Trine 2 `.dtx`.
- POG-like files with `PPIG` or `DPOG` signatures.

The old D1 PIG-like path replaces by bitmap/sound name using:

- `AllBitmapsNames`
- `AllDigiSndNames`

It also keeps original bitmap/sound state in `BitmapOriginal` and `SoundOriginal` so a later level can restore cleanly.

## Trine 2 Specific Finding

Local files:

- `game_data/mission_files/trine2.zip`
- extracted staging sample under `game_data/mission_files/trine2`
- `game_data/mission_files/trine2/trine2.msn`

`trine2.msn` contains:

```text
name = Trine - Episode 2
type = normal
num_levels = 9
custom_music = yes
custom_textures = yes
```

The HOG contains:

- 10 `.rdl` levels
- 10 `.dtx` texture sets
- 10 `.txb` text files
- 23 `.bbm` images
- 14 `.ogg` music files
- `descent.sng`

The first `.dtx` starts with D1-style PIG-like counts and texture names such as `rock263`, `misc034`, and `misc035`. That strongly points to the `d1/main/custom.c::load_pig1()` path, not D2 `.POG`.

## Proposed Architecture

### Principle: D2 Engine, D1 Asset Overlay

For "D1 levels played within D2", do not replace D2's base files globally. Instead:

1. D2 launches with the normal D2 required files.
2. D1 mission ZIPs/mods are staged and mounted into the D2 search path.
3. When a loaded mission has `descent_version == 1`, D2 enters D1 asset overlay mode.
4. D1 base files are used preferentially for D1 mission assets:
   - `descent.hog` for D1 briefings, bitmaps, exit assets, and built-in D1 mission files.
   - `descent.pig` for D1 texture bitmap data and palette conversion.
   - `palette.256`, `bitmaps.tbl`, or `bitmaps.bin` when required by older/shareware D1 PIG layouts.
5. D2 base files stay loaded for D2 engine definitions and Guide-Bot functionality.

This is the smallest design that gets correct D1 visuals without weakening D2 guidebot support.

### Base File Readiness

Current Android launch readiness checks are per executable:

- D1 launch requires `descent.hog` and `descent.pig`.
- D2 launch requires D2 files such as `descent2.hog`, `descent2.ham`, `groupa.pig`, and sounds.

For D1-in-D2, readiness should become contextual:

- Launching D2 with only D2 missions enabled: require D2 base files.
- Launching D2 with D1 mission ZIPs enabled: require D2 base files, and strongly prefer D1 `descent.hog` plus `descent.pig`.
- If D1 files are missing, allow launch only with an explicit degraded-mode warning, because D2 can fall back for many stock textures but not all D1 resources will be right.

Recommended user-facing state:

- "D1-in-D2 ready": D2 base files present, D1 `descent.hog` present, D1 `descent.pig` present.
- "D1-in-D2 degraded": D2 base files present, D1 base files missing or partial.
- "D1-in-D2 blocked": D2 base files missing.

### Mission Selection UX

The launcher already enables D1 mission ZIPs for D2. The missing UX is making that intentional and visible.

Add an explicit compatibility option, probably in advanced/settings first:

- "Run D1 missions in D2 when possible"
- Detail: enables D2-only features such as Guide-Bot, while using D1 base assets when present.

This option can default off globally, but the immediate workflow can document or provide a per-mission launch action:

- "Play in D1"
- "Play in D2 with Guide-Bot"

The second action should run the D2 executable and keep the D1 mission ZIP enabled in `d2x-redux/.active_mod_paths`.

## Proposed Native Texture Loading

### Add A D2 D1-Custom Loader

Add a D2-side loader derived from `d1/main/custom.c`, scoped to `EMULATING_D1`.

Possible file placement:

- Prefer `d2/main/d1_custom.c` and `d2/main/d1_custom.h` for isolation.
- Reuse local D2 primitives from `piggy.c`, `gr.h`, `digi.h`, `hash.h`, and `u_mem.h`.
- Avoid importing D1-only gameplay structures.

Core API:

```c
void load_d1_custom_data(char *level_name);
void d1_custom_close(void);
void d1_custom_remove(void);
```

Initial behavior:

```c
d1_custom_remove();
try <level>.pg1;
try <level>.dtx;
```

Later behavior:

```c
try <level>.hx1;
possibly try <level>.hxm as alias;
```

### Integrate Into D2 Level Loading

Change `d2/main/gameseq.c::LoadLevel()` from:

```c
if (EMULATING_D1)
    load_d1_bitmap_replacements();
else
    load_bitmap_replacements(level_name);
```

to:

```c
if (EMULATING_D1) {
    load_d1_bitmap_replacements();
    load_d1_custom_data(level_name);
} else {
    load_bitmap_replacements(level_name);
}
```

Loading order matters:

1. Load stock D1 replacements from `descent.pig`.
2. Load per-level D1 custom replacements from `.pg1` and `.dtx`.
3. Page in/cached textures for the current level.

That lets level-local textures override stock D1 textures.

### Restore State Between Levels

The D1 loader calls `custom_remove()` before loading a new level. D2 needs equivalent restore behavior so custom textures from one D1 level do not bleed into the next.

The D2 D1-custom loader should track only replacements it made and restore:

- `GameBitmaps[]`
- `GameBitmapOffset[]`
- `GameBitmapFlags[]`
- OGL texture data through `gr_set_bitmap_data()` / `gr_free_bitmap_data()` patterns already used in the codebase

Use the D1 `BitmapOriginal` approach, adapted for D2. If sound replacement support is included, also track `GameSounds[]`.

### Replacement Addressing

For D1 PIG-like `.dtx` and `.pg1`, use bitmap names:

- Read the 8-char D1 bitmap name.
- Add `#frame` for animated ABM entries, matching D1 behavior.
- Resolve through D2's `AllBitmapsNames`.
- If the name is unresolved, log and skip.

Open risk: D1 custom files are named against D1 bitmap tables, while D2 is operating after D1-to-D2 texture conversion. Many names are shared between D1 and D2, and Trine 2's observed names look compatible, but unique D1 slots may need special mapping.

Mitigation:

- Add debug counters:
  - files found
  - bitmap entries parsed
  - bitmap replacements applied
  - unresolved bitmap names
  - sound replacements applied/skipped
- Log unresolved names with level filename.
- Test against Trine 2 first.
- If names do not resolve well enough, build a D1-name-to-D2-slot table from `d1_tmap_nums`, `convert_d1_tmap_num()`, and `descent.pig` headers.

### Sound Replacements

D1 `load_pig1()` supports sound replacements in the same file format. Initial Trine 2 evidence points to textures and music, not per-level sound effects.

Recommended phase split:

- Phase 1: support bitmap entries; parse and skip sound entries with a debug count.
- Phase 2: add sound restoration/replacement if a real D1 pack needs it.

This keeps the first native change smaller and reduces risk in the sound system.

### Robot Replacements

D2 already has `d2/main/bm.c::load_robot_replacements(level_name)` for `.HXM`. It is called unconditionally from `d2/main/gameseq.c`.

D1 custom files use `.HX1`, and D1's `load_hxm()` reads a D2-style HXM payload from that extension while discarding fields D1 does not support. D2 can probably support `.HX1` by trying the same format with a different extension, but that should be a later phase:

- First add `.HX1` alias support only for `EMULATING_D1`.
- Reuse D2's native `robot_info_read_n()` path if the file is valid D2 HXM data.
- Add strict bounds and error handling, because old D1 packs may include assumptions from D1's smaller robot tables.

This is not required for the immediate Trine 2 texture fix unless inspection finds `.hx1` files.

## Level Pack Variants To Support

### Base D1 Missions

Already recognized by HOG size constants in `d2/main/mission.h`:

- Registered PC D1 v1.4/v1.5
- D1 v1.0
- OEM
- Shareware
- Mac full/shareware variants

Texture support already accounts for several D1 PIG sizes in `d2/main/piggy.h` and `d2/main/piggy.c`.

### Add-On Mission Layouts

Support target:

- Loose `.msn` + sibling `.hog`.
- Mission ZIP containing `.msn` + `.hog`.
- Extracted bundle layout under `missions/`.
- HOG entries containing `.rdl`, `.sdl`, `.dtx`, `.pg1`, `.txb`, `.bbm`, `.pcx`, `.ogg`, `.hmp`, `.mid`.

The current mission ZIP importer and staging path already handles the first three well.

### Texture Formats

Minimum for Trine 2:

- D1 old PIG-like `.dtx` with named bitmap replacements.

Full D1 pack target:

- `.pg1` old PIG-like files.
- `.dtx` old PIG-like files.
- POG-like `PPIG` and `DPOG` variants as supported by D1 `load_pog()`.
- D2 `.POG` for D2 missions remains unchanged.

Potential later/optional:

- Loose `.bbm`/`.abm` title and briefing images are already generally handled by briefing/title code, but D1-in-D2 palette handling should be tested.
- `d2tmap.bin` conversion from the disabled D1 `D2TMAP_CONV` path is probably not needed unless a pack's POG addressing targets D2 texture numbers in a way D2 cannot infer.

### Music

Trine 2's music path appears structurally supported:

- HOG contains `descent.sng`.
- HOG contains `.ogg` tracks.
- Mission ZIP music analysis already finds HOG-contained audio.

Still test with D1-in-D2 because mission HOG mount order and song lookup must agree under D2.

## Launcher And Data Routing Design

### Do Not Make D2 Launch From D1 Base Alone

Literal "use D1 base game files instead of D2 base game files" would require replacing or emulating:

- D2 `descent2.ham` robot/object definitions.
- D2 pigfile initialization.
- D2 sounds and text resources.
- D2 Guide-Bot robot/model/AI assumptions.

That is much broader than necessary and risks breaking the reason to use D2: the Guide-Bot.

### Add A D1-In-D2 Compatibility Layer

Launcher behavior:

1. Detect D1 mission ZIPs as today.
2. When launching D2, include enabled D1 mission ZIP paths as today.
3. Add a readiness check for D1-in-D2:
   - D2 required files present.
   - D1 `descent.hog` present.
   - D1 `descent.pig` present.
4. Surface missing D1 base files as a D1-in-D2 asset warning, not a generic D2 launch failure.

Native behavior:

1. Continue D2 startup from D2 base data.
2. When `EMULATING_D1`, mount `descent.hog` as today.
3. Use `descent.pig` for stock D1 unique textures as today.
4. Add D1 per-level custom texture loading.
5. Add diagnostics so launcher/test automation can verify that D1 assets were actually used.

### Suggested Diagnostics

Add debug/introspection fields, preferably exposed through existing metadata/test hooks:

- `emulating_d1`
- `d1_pig_present`
- `d1_descent_hog_mounted`
- `d1_custom_texture_files_found`
- `d1_custom_texture_entries`
- `d1_custom_texture_applied`
- `d1_custom_texture_unresolved`
- `d1_custom_sound_entries`
- `d1_custom_sound_applied`

These make correctness testable without relying on screenshots.

## Implementation Phases

### Phase 1: Immediate Trine 2 Texture Fix

Goal: Trine 2 under D2 uses its `.dtx` files.

Native changes:

- Add D2 D1-custom bitmap loader for `.pg1` and `.dtx`.
- Scope it to `EMULATING_D1`.
- Parse D1 old PIG-like format from `d1/main/custom.c::load_pig1()`.
- Apply bitmap replacements by name through D2 `AllBitmapsNames`.
- Restore replacements between levels.
- Flush texture merge/cache state after replacements.
- Add debug counters/logs.

Likely files:

- `d2/main/d1_custom.c`
- `d2/main/d1_custom.h`
- `d2/main/CMakeLists.txt`
- `d2/main/gameseq.c`
- maybe `d2/main/piggy.h` if sharing helper declarations is needed

Risk level: moderate but well-scoped. This touches texture state in D2, but only when `EMULATING_D1`.

### Phase 2: D1-In-D2 Readiness And UX

Goal: Users can intentionally run D1 mission packs under D2 and know whether D1 assets are available.

Android changes:

- Add D1-in-D2 readiness helper.
- Show D1 base asset state when D1 mission ZIPs are enabled for D2.
- Add per-mission/per-launch action for "Play in D2 with Guide-Bot" if not already represented in the UI.
- Keep existing D1 launch path unchanged.

Likely files:

- `SetupGameFiles.kt`
- `SetupActivity.kt` / setup sections around launch controls
- `ModManagerMissionZipTest.kt`

Risk level: low to moderate. This is mostly UX/preflight and does not change native loading.

### Phase 3: Broader D1 Texture Compatibility

Goal: Support more D1 packs beyond Trine 2.

Native changes:

- Add POG-like `PPIG` / `DPOG` parsing from D1 `load_pog()`.
- Decide whether to support sound replacements.
- Add optional D1-name-to-D2-slot fallback if direct name lookup misses too often.
- Improve `descent.pig` memory allocation instead of relying on fixed `D1_BITMAPS_SIZE`.

Risk level: moderate. More formats means more malformed-file cases and more replacement-state paths.

### Phase 4: Custom Robots And `.HX1`

Goal: D1 packs with custom robots/models behave in D2 when possible.

Native changes:

- Try `<level>.HX1` when `EMULATING_D1`.
- Reuse or adapt D2 HXM reader.
- Validate object/robot/model indices aggressively.
- Add diagnostics for files found/applied/skipped.

Risk level: higher than textures. Robot data changes gameplay and can interact with D2-only AI assumptions.

### Phase 5: Regression And Automation

Goal: Protect D1, D2, and D1-in-D2 paths.

Tests:

- Unit test mission ZIP staging still writes D1 mission ZIP paths for D2 launch.
- Native metadata/headless test for Trine 2:
  - mission is detected as D1.
  - D2 runtime reports `EMULATING_D1`.
  - `descent.pig` is found when present.
  - `e2m1.dtx` is found.
  - custom bitmap replacements applied count is greater than zero.
- Negative test:
  - missing `descent.pig` warns and does not crash.
- Regression:
  - D2 `.POG` replacement still works for D2 missions.
  - D1 executable custom texture loading remains untouched.

## Feasibility And Risk Assessment

### Feasibility

This path is very feasible. The repo already contains almost all needed concepts:

- D2 can load D1 missions.
- D2 can mount D1 `descent.hog`.
- D2 can read D1 `descent.pig`.
- D1 has a proven custom texture loader.
- Android already stages D1 mission ZIPs for D2 launches.

The highest-value initial implementation is small: port/adapt D1's named custom bitmap loader into D2 and call it after `load_d1_bitmap_replacements()`.

### Main Risks

1. Name mapping mismatch

   D1 `.dtx` files name D1 bitmaps. D2 replacement slots are D2 bitmap indices after `convert_d1_tmap_num()`. Many names overlap, but unique D1 names may not all resolve through D2 `AllBitmapsNames`.

   Mitigation: instrument unresolved names, test with Trine 2, then add a D1-name-to-D2-slot map if needed.

2. Replacement cleanup

   D2 must restore previous level custom textures reliably. Texture bleed between levels would be confusing and hard to diagnose.

   Mitigation: clone D1 `BitmapOriginal` strategy, add targeted tests, and keep scope limited to `EMULATING_D1`.

3. Palette/remap differences

   Stock D1 replacements remap from D1 palette. D1 custom files may assume D1 palette data. The initial D1 custom loader in D1 appears to load raw indexed data into the active bitmap system rather than explicitly palette-remapping, so D2 behavior should be validated visually and through known texture colors.

   Mitigation: test Trine 2 with `descent.pig` and D1 palette present; if colors are wrong, apply the same D1 palette remapping helpers used by `load_d1_bitmap_replacements()`.

4. Custom robot support

   `.HX1` is not just visual. Supporting it under D2 may affect gameplay and D2 AI.

   Mitigation: defer until texture support is stable.

5. User expectations around "D1 base instead of D2 base"

   D2 still needs D2 files. Users may interpret "point at D1 files" literally.

   Mitigation: describe this as "Use D1 assets for D1 missions in D2" or "D1 asset overlay for D2 Guide-Bot mode."

## Recommended Next Step

Implement Phase 1 first. It is the shortest path to the immediate goal:

1. Keep launching Trine 2 under D2.
2. Keep D2 Guide-Bot spawn as-is.
3. Add D1 `.dtx`/`.pg1` bitmap replacement loading in D2.
4. Verify `e2m1.dtx` applies nonzero replacements.

## Implementation Tracker

- [x] Add isolated D2 D1-custom texture loader source/header.
- [x] Hook loader into D2 `LoadLevel()` only for `EMULATING_D1`.
- [x] Add the new source to D2 CMake targets.
- [x] Expose D1 custom texture counters through Android introspection.
- [x] Extend the D1 mission ZIP fixture to stage per-level `.dtx` files.
- [x] Add a Trine 2 D1-in-D2 automation script with declared data deps.
- [x] Run scoped formatting/quality checks on touched files.
- [x] Run JVM mission ZIP staging tests.
- [x] Run Android native build and debug APK build.
- [x] Run Trine 2 on emulator through the launcher into D2.
- [x] Fix automation selection after first run picked "Descent: First Strike".
- [x] Verify Trine 2 level 1 reports `d1_custom_textures.applied=6`.

After that, add the launcher readiness/UX layer so this becomes a discoverable supported mode rather than a hidden compatibility behavior.

## Phase 2 Texture Compatibility Tracker

- [x] Study D1 POG-like loader and current D2 D1-custom loader shape.
- [x] Add POG-like `PPIG` / `DPOG` parsing for D1-in-D2 custom textures.
- [x] Extend the mission ZIP fixture to cover old DTX plus POG-like custom data.
- [x] Run scoped quality checks.
- [x] Run JVM fixture tests.
- [x] Run Android native build.

## Phase 3 Launcher Readiness Tracker

- [x] Trace D2 launch readiness and enabled D1 mission ZIP state in the launcher.
- [x] Add a minimal D1-in-D2 readiness signal for D1 base assets.
- [x] Expose readiness through setup introspection for automation/debugging.
- [x] Add focused JVM coverage for the readiness signal.
- [x] Run scoped quality checks and JVM tests.

## Phase 4 D1 Custom Sound Tracker

- [x] Study D1 custom sound replacement behavior and D2 sound cache behavior.
- [x] Add old D1 PIG-like sound replacement and restore support in the D2 D1-custom loader.
- [x] Add 11 kHz D1 sound resampling to the active D2 sample rate.
- [x] Expose sound applied/unresolved counters through Android introspection.
- [x] Extend tests or automation fixtures to cover D1 custom sound entries.
- [x] Run scoped quality checks and builds.

## Phase 5 D1 Base Sound Tracker

- [x] Trace how D2 initializes D2 base sounds while D1-in-D2 uses `descent.pig` only for textures.
- [x] Add registered/OEM/raw D1 base sound sample replacement by sound name for D1 missions in D2.
- [x] Restore D2 base sounds when leaving or reloading the D1 mission path.
- [x] Expose D1 base sound counters through introspection.
- [x] Assert D1 base sounds apply in the Trine 2 D1-in-D2 automation script.
- [x] Port or adapt D1 compressed PC shareware sound decompression for shareware `descent.pig` variants.
- [x] Add Mac D1 sound resource support using `Sounds/sounds.array` and `Sounds/SNDxxxx.raw` when present.
- [x] Run scoped quality checks and builds for the edge-case sound work.
