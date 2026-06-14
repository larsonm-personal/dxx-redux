# Extract D1-in-D2 Compatibility Code

## Goal

Move the new D1-in-D2 compatibility overlay code out of 1996-era D2 source
files and into a focused module, tentatively:

- `d2/main/d1_in_d2.c`
- `d2/main/d1_in_d2.h`

The intent is to keep old source diffs small and readable. Legacy files should
only contain small calls at natural seams, not large D1 compatibility parsers or
state machines.

## Current Dirty Diff Survey

Current dirty D1-in-D2 compatibility work is concentrated in four files:

- `d2/main/gamemine.c`
- `d2/main/gamemine.h`
- `d2/main/gameseq.c`
- `android/ai tool plans/asset management/plan_d1_trine2_texture_precedence_20260614.md`

The code changes are:

- `gamemine.c`
  - Adds D1 table constants for robot/effect/vclip parsing.
  - Adds D1 effect overlay state and readers.
  - Adds D1 powerup/vclip overlay state and readers.
  - Adds D1 robot asset overlay state and readers.
  - Adds `open_d1_registered_pig()`.
  - Adds `seek_d1_vclip_table()`.
  - Adds `skip_d1_vclips_and_effects()`.
  - Adds `read_d1_robot_info()`.
  - Adds `apply_d1_effects()`.
  - Adds `apply_d1_powerup_vclips()`.
  - Adds `apply_d1_robot_assets()`.
  - Adds a few includes only needed by the new overlay code:
    `polyobj.h`, `bm.h`, and `robot.h`.
- `gamemine.h`
  - Adds public declarations for the three overlay entry points.
- `gameseq.c`
  - Calls the D1 compatibility overlay functions while loading a level.
  - Restores D2 robot assets when leaving D1 emulation.

## Existing D1-in-D2 Code That Should Not Move In This Pass

Some D1-in-D2 support predates this robot/powerup/effect work and sits at
natural ownership points. Moving it now would make the refactor wider than
needed.

Keep in `gamemine.c` for now:

- `d1_pig_present`
- `convert_d1_tmap_num()`
- `d1_tmap_num_unique()`
- compiled mine texture conversion in `load_mine_data_compiled()`

Reason: these are tightly coupled to D1 mine loading and texture-map conversion.
They are older, broad, and called from existing mine-load code.

Keep in `piggy.c` for now:

- `load_d1_bitmap_replacements()`
- `load_d1_bitmap_frame()`
- D1 PIG bitmap loading helpers
- D1 palette colormap helpers

Reason: these operate directly on the bitmap cache and PIG paging internals.
The new module can call their public functions instead of owning bitmap-cache
mechanics.

Keep in `d1_custom.c` for now:

- D1 custom `.pg1`, `.dtx`, and POG-ish mission data loading.

Reason: this is already isolated in its own file and is a good precedent for
the new module.

Keep existing scattered game-mode checks in:

- `mission.c`
- `titles.c`
- `gamesave.c`
- `switch.c`
- `net_udp.c`

Reason: those are behavior seams for mission listing, briefing, save/load, and
gameplay rules. They are not part of this asset-overlay extraction.

## Proposed New Module Interface

Create `d2/main/d1_in_d2.h`:

```
#ifndef _D1_IN_D2_H
#define _D1_IN_D2_H

void d1_in_d2_apply_effects(int active);
void d1_in_d2_apply_powerup_vclips(int active);
void d1_in_d2_apply_robot_assets(int active);

#endif
```

Potential later cleanup:

```
void d1_in_d2_prepare_level_assets(char *level_name);
void d1_in_d2_restore_d2_assets(void);
void d1_in_d2_apply_post_robot_assets(void);
```

Do not add this wrapper in the first move unless it clearly reduces the
`gameseq.c` diff. The first extraction should be mechanical and easy to review.

## Proposed Ownership After Extraction

`d1_in_d2.c` owns:

- D1 serialized table constants used only by compatibility overlays.
- D1 effect overlay cached state.
- D1 powerup/vclip overlay cached state.
- D1 robot overlay cached state.
- D1 robot-specific fixed high bitmap range.
- D1 compiled property table navigation helpers.
- D1 robot info field reader.
- The three public apply/restore functions.

`gamemine.c` owns:

- D1 mine texture-map conversion.
- D1 tmap number conversion.
- The global `d1_pig_present` variable.

`piggy.c` owns:

- Loading D1 bitmaps into specific D2 bitmap slots.
- Loading D1 base wall bitmap replacements.
- Palette and colormap remapping used by D1 bitmap loading.

`gameseq.c` owns:

- Only the load-order calls.
- Include `d1_in_d2.h` instead of relying on `gamemine.h` for overlay
  declarations.

`gamemine.h` should shrink back to:

- `convert_d1_tmap_num()`
- `d1_tmap_num_unique()`

## Public/External Dependencies Needed By `d1_in_d2.c`

Headers likely needed:

- `pstypes.h`
- `inferno.h`
- `object.h`
- `gamemine.h`
- `piggy.h`
- `effects.h`
- `byteswap.h`
- `bm.h`
- `gamesave.h`
- `powerup.h`
- `robot.h`
- `vclip.h`
- `polyobj.h`

External functions/globals currently needed:

- `read_hamfile()` from existing D2 data load path.
- `free_polygon_models()` and `free_model()` from `polyobj.c`.
- `load_d1_bitmap_frame()` from `piggy.c`.
- `convert_d1_tmap_num()` from `gamemine.c`.
- `Objects[]`, `Highest_object_index`, `Robot_info[]`, `Robot_joints[]`,
  `N_robot_types`, `N_robot_joints`.
- `Effects[]`, `Vclip[]`.
- `ObjBitmaps[]`, `ObjBitmapPtrs[]`, `N_ObjBitmaps`.
- `Polygon_models[]`, `N_polygon_models`, `Dying_modelnums[]`,
  `Dead_modelnums[]`.

Watch for prototype placement:

- Prefer adding `read_hamfile()` to an existing header if one already owns it.
- If no suitable header exists, keep a local `extern int read_hamfile();` in
  `d1_in_d2.c` for the first move to avoid a wider header refactor.

## CMake Integration

Add `d1_in_d2.c` to `D2X_MAIN_SOURCES` in `d2/main/CMakeLists.txt`, near
`d1_custom.c`.

This should cover:

- desktop D2 target
- Android D2 target, because Android adds the whole `d2/main` subdirectory
- D2 headless targets, because they derive from `D2X_MAIN_SOURCES`

No Android-specific CMake source addition should be needed.

## Refactor Phases

### Phase 1: Mechanical Extraction

Status: not started.

Steps:

- Add `d1_in_d2.h`.
- Add `d1_in_d2.c`.
- Move only the new overlay code out of `gamemine.c`.
- Move new overlay-only includes from `gamemine.c` to `d1_in_d2.c`.
- Change overlay function names from `apply_d1_*` to `d1_in_d2_apply_*`.
- Include `d1_in_d2.h` in `gameseq.c`.
- Remove overlay declarations from `gamemine.h`.
- Add `d1_in_d2.c` to `d2/main/CMakeLists.txt`.

Expected old-file diff after Phase 1:

- `gamemine.c`: only the D1 object-effect fix remains if it is in moved code,
  so ideally no new overlay code remains there.
- `gamemine.h`: no overlay declarations.
- `gameseq.c`: three or four call-site lines plus one include.
- `CMakeLists.txt`: one source-list line.

Validation:

- Scoped code quality on touched files.
- Android native build `:app:externalNativeBuildDebug`.

### Phase 2: Optional Load-Order Wrapper

Status: pending.

If `gameseq.c` still feels too noisy, add wrapper functions:

- `d1_in_d2_restore_d2_assets()`
- `d1_in_d2_apply_pre_custom_assets()`
- `d1_in_d2_apply_post_robot_assets()`

This can reduce `gameseq.c` to a few higher-level calls, but it is not required
for the first mechanical extraction.

Validation:

- Same as Phase 1.
- Extra care that D1 custom data still loads between base D1 wall/vclip/effect
  setup and robot overlay.

### Phase 3: Consider Moving D1 Wall/Base Asset Orchestration

Status: deferred.

Potentially move orchestration, not implementation:

- Call `load_d1_bitmap_replacements()` through a D1-in-D2 wrapper.
- Call `d1_custom_load_data()` through a D1-in-D2 wrapper.

Do not move PIG cache or custom-data implementation yet.

## Risks

- `d1_in_d2.c` will need several old-game globals. That is acceptable because
  the goal is to isolate the compatibility code, not modernize the data model.
- Moving too much at once could obscure recent robot fixes. Keep the first pass
  mechanical.
- Some functions are not declared in clean headers. Avoid broad header cleanup
  unless the compiler forces it.
- The fixed high robot bitmap range depends on `MAX_BITMAP_FILES` remaining
  large enough for `D1_MAX_OBJ_BITMAPS`.

## Survey Conclusion

The extraction is feasible and should be low-risk if kept mechanical. The
biggest win is moving roughly 200 lines of D1 compatibility parser/state code
out of `gamemine.c`, leaving that file focused on mine loading and D1 tmap
conversion. The old-file diffs after extraction should be mostly one include,
one CMake source entry, and a few call sites in `gameseq.c`.
