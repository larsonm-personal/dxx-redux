# D1/D2 Overlay Mod Compatibility Work Plan

## Goal

Support overlay mods as extensively and correctly as practical across:

- native D1 missions in the D1 engine
- native D2 missions in the D2 engine
- D1 missions played inside the D2 engine

The main correctness target is that the selected base game identity controls the
meaning of palettes, bitmap IDs, names, robot assets, sounds, cockpit art, and
animated effects before any user-selected overlay pack is applied.

For example, when Trine 2 is played as a D1 mission inside D2, the base layer
should behave like D1. D2 assets should not win accidentally just because the
D2 executable is running. Optional high resolution packs and cockpit packs
should then apply in a deterministic and inspectable order.

## Current State

### Native D2

The D2 path uses D2 base data and then applies D2-style mission replacements:

1. Load level and palette.
2. Restore any D1 compatibility overlays from the previous level.
3. Load D2 level bitmap replacements through `load_bitmap_replacements()`.
4. Load level robot replacements through `load_level_robots()`.
5. Runtime texture loading can pick up PNG replacements by bitmap name when
   they are visible through PhysFS.

This is the most mature path. Overlay packs that provide D2-named PNGs or
D2-style POG data mostly align with existing expectations.

### D1-in-D2

The D1 compatibility path currently has several separate layers:

1. Select the D1 default palette when the level is a D1 compiled level.
2. Load D1 wall texture slots from `descent.pig` into their mapped D2 slots.
3. Apply D1 animated effect metadata and effect frames.
4. Apply D1 vclip and powerup frames.
5. Load D1 custom level data through `.pg1` and `.dtx` support.
6. Load D2-style robot replacement metadata.
7. Overlay D1 robot info, polymodels, joints, and object bitmap references.

This now gets core D1 level textures and powerups much closer to D1, but it is
not yet a complete overlay system. The main weakness is that some compatibility
layers run after custom mod layers, so they can overwrite user or mission
overrides. The other weakness is that not every asset family has a D1-aware
base mapping yet.

### Existing High Resolution Path

PNG replacement textures are looked up by internal bitmap name at render time.
This works well when the runtime bitmap name is the name the overlay pack was
authored for. It becomes ambiguous in D1-in-D2 because:

- D1 and D2 can share names that point to different art.
- Some D1 art is loaded into D2 slots, so a D2-named replacement can override a
  D1 compatibility texture unless the lookup is mode-aware.
- Animated frames and robot object bitmaps can be remapped after an earlier
  replacement pass.

## Desired Precedence Model

The engine should apply assets in stable layers. Later layers win over earlier
layers, but the layer type should be deliberate, not an accident of function
call order.

Recommended default order:

1. Engine defaults and compiled tables.
2. Selected base game data.
   - D1 engine: D1 base files.
   - D2 engine: D2 base files.
   - D1-in-D2: D1 base files and D1 semantic mappings.
3. Global enhancement packs for the selected base game.
   - High resolution D1 packs apply to D1 and D1-in-D2.
   - High resolution D2 packs apply to native D2.
   - Cross-game packs only apply if explicitly marked compatible.
4. Mission-authored custom data.
   - D1 `.pg1`, `.dtx`, PIG1, and POG-ish data.
   - D2 `.pog` and `.hxm`.
   - Mission ZIP contents mounted for that mission.
5. Explicit user per-mission overlay packs.
   - These are opt-in patches for a specific mission or mission family.
   - They can intentionally override mission-authored data.
6. Debug or developer overrides.

This default protects mission author intent. A global high resolution pack
should improve base art, but it should not silently replace custom textures
bundled with a mission. If we later want more flexibility, the launcher can
offer an "enhancement pack wins over mission art" expert setting, but that
should not be the default.

## Asset Families To Cover

### Palettes

Palette identity is foundational. Paletted bitmaps, model textures, powerups,
effects, cockpit art, and many custom mission assets depend on it.

Required behavior:

- D1-in-D2 must use the D1 palette for D1 compiled levels unless the level or
  mission explicitly supplies another palette.
- D2 base palettes must not be inferred when a D1 default palette is active.
- Overlay packs need a palette assumption:
  - truecolor PNG/JPG/TGA overlays are palette-independent
  - paletted PIG/POG/PG1/DTX overlays are palette-dependent
  - raw 8-bit assets need the source game palette or a declared palette

Work items:

- Add diagnostics that report the active palette source for the current level.
- Record whether each replacement source is palette-dependent.
- Reject or warn on obvious cross-palette use, such as D2 POG data applied to
  a D1-in-D2 mission without an explicit compatibility declaration.

### Wall Textures

Wall texture IDs are the most visible problem and the most mature part of the
current D1-in-D2 work.

Required behavior:

- D1-in-D2 maps D1 wall bitmap IDs to the correct D2 runtime slots.
- D1 mission custom textures override D1 base wall textures.
- D2 mission custom textures continue to override D2 base wall textures.
- PNG replacements should use the selected base game's bitmap name namespace.

Work items:

- Keep D1 base wall mapping as the first compatibility layer.
- Extend D1 custom loading to sniff and accept `.pog` when the file contents
  are compatible, not only `.pg1` and `.dtx`.
- Add a trace entry for every wall replacement:
  source file, source layer, source ID or name, target bitmap slot, final name.
- Add a test fixture that verifies a D1-in-D2 mission can override a D1 base
  wall texture and that a D2 global replacement does not override it by default.

### Animated Effects and Destroyed Wall Textures

Effects are coupled metadata plus bitmap frames. A correct wall texture can
still animate into the wrong frame sequence or destroyed texture.

Required behavior:

- D1-in-D2 uses D1 effect metadata for D1 effect slots.
- D1 effect frames are loaded as a group, not one frame at a time with mixed
  D2 frames.
- Destroyed texture destinations use D1 semantics for D1 missions.
- Mission-authored effect changes win over base compatibility data.

Work items:

- Treat effect metadata as a first-class overlay family instead of a helper
  side effect of wall loading.
- Track every eclip source and destination in diagnostics.
- Ensure final mission overlays can replace both the changing texture and the
  destroyed destination texture after D1 base compatibility is applied.
- Add a Trine 2 validation case for the early destructible panel that used to
  select the wrong blown texture.

### Vclips, Powerups, Weapons, and Explosions

Powerups are partly fixed by D1 vclip overlay support, but the same pattern
affects explosions, weapon visuals, robot fire effects, and other vclip users.

Required behavior:

- D1-in-D2 uses D1 vclip tables and D1 bitmap frames for D1 assets.
- D2 vclips are restored when leaving D1-in-D2.
- Custom mission or overlay vclips can override base compatibility vclips.

Work items:

- Expand diagnostics from "powerup slots replaced" to all vclip families.
- Verify object classes that use vclips pick up D1-in-D2 replacements:
  powerups, explosions, weapons, debris-like effects where applicable.
- Add a synthetic overlay test that replaces an energy and shield powerup and
  confirms the final runtime bitmap source is the overlay, not D1 base.

### Robots and Polymodel Textures

Robot assets are the highest-risk family because they involve table metadata,
polymodels, joints, object bitmap tables, and per-model texture references.

Required behavior:

- D1-in-D2 uses D1 robot data and D1 robot texture references when the mission
  is a D1 mission.
- D2 robot table sizes must remain safe for the D2 runtime.
- Mission HXM data and D1 robot compatibility must not clobber each other.
- Robot texture overlay packs should be able to replace the final robot bitmap
  names or slots after the D1 robot mapping is known.

Work items:

- Split robot handling into two clear phases:
  - base semantic phase: select D1 or D2 robot tables and model references
  - final art overlay phase: apply mission and user robot texture overrides
- Completed 2026-06-14: preserve D2-only D1 robot AI tuning fields
  (`behavior`, `lightcast`) before overlaying D1 robot tables, so D1-in-D2
  robots do not inherit corrupted zero tuning from the D1 PIG import path.
- Completed 2026-06-14: keep imported D1 robot `aim` at the D2 maximum byte
  value, but force D1-in-D2 firing spread to D1's exact scale at shot time.
  D1 robot data has no per-type aim byte, and D2's maximum byte still expands
  the error cone slightly.
- Completed 2026-06-14: import D1 weapon records for the first 30 weapon
  slots during D1-in-D2 robot asset loading, while keeping D2-only weapon
  fields on old-data defaults. D1 robot weapon IDs now use D1 matter,
  homing, speed, damage, model, vclip, and timing data instead of D2's
  approximations.
- Keep the crash fix invariant: do not shrink global D2 model or object bitmap
  counts below values the D2 runtime can reference.
- Add trace entries for:
  robot type, model slot, model source, object bitmap pointer source, texture
  bitmap slot, final replacement source.
- Add a validation test for Trine 2 robot model texture sources.
- Add a small synthetic robot overlay pack that changes one robot texture and
  verifies it survives D1-in-D2 robot compatibility application.

### Cockpit, Gauges, HUD, Briefing, and Menu Art

These assets are often replaced by cockpit graphics mods and are not fully
covered by the current D1-in-D2 compatibility layer.

Required behavior:

- D1-in-D2 uses D1 cockpit and gauge art when it is rendering D1 gameplay.
- D2 cockpit art should not leak into D1-in-D2 unless the user selects a D2
  cockpit overlay that declares D1-in-D2 compatibility.
- High resolution cockpit overlays should work by name where possible.

Work items:

- Inventory D1 and D2 gauge/cockpit bitmap tables and name mappings.
- Add D1-in-D2 base compatibility mapping for cockpit and gauge slots.
- Ensure render-time PNG lookup sees D1-compatible names for D1-in-D2 cockpit
  and gauge assets.
- Add diagnostics for cockpit/gauge source files.
- Add a test with a tiny cockpit overlay that replaces one gauge/cockpit image.

### Sounds

Sound overlays can come from D1 base files, D2 base files, mission data, or
global high resolution sound packs.

Required behavior:

- D1-in-D2 should prefer D1 base sounds for D1 missions.
- D2 sounds should be restored for native D2.
- External sounds in `Sounds/` should override the selected base game's sound
  by name when the overlay is compatible.
- WAV, R22, and RAW search order should remain deterministic.

Work items:

- Add diagnostics for sound source and selected sample rate.
- Confirm D1-in-D2 base sounds are loaded before any compatible overlay pack.
- Ensure high resolution sound packs can be tagged D1, D2, or both.
- Add a test that replaces one D1 sound and one D2 sound and verifies the
  current game mode selects the expected one.

### HAM, HXM, and Other Tables

Table overlays change the interpretation of later art references. Their order
needs to be explicit.

Required behavior:

- Metadata that changes slots or references must apply before final art
  overlays that target those final slots.
- D1 compatibility tables should establish D1 semantics in D1-in-D2.
- Mission-authored HXM or equivalent data should apply in a clearly documented
  point relative to D1 compatibility tables.

Work items:

- Document all table-like loaders and their current call order.
- Decide per table whether it is base semantic data or mission override data.
- Move final art overlay passes after all metadata that can change texture
  references.

## Proposed Architecture

### Asset Layer Registry

Add a small registry that records replacement attempts and final winners. This
does not need to own all bitmap memory in the first version. It can begin as a
diagnostic and ordering helper.

Each entry should include:

- asset family: wall, effect, vclip, robot, cockpit, sound, table
- target key: bitmap slot, bitmap name, sound name, robot model slot, etc.
- selected base game: D1, D2, or D1-in-D2
- source layer: base, global enhancement, mission, per-mission user overlay
- source file and archive path
- source palette assumption if known
- result: applied, skipped, unsupported, missing target, superseded

This gives us a way to answer user questions like "why did this texture win"
without reverse-engineering every loader call.

### Central Layer Application Point

Replace the scattered D1-in-D2 load sequence with an explicit high-level flow:

1. Reset previous level overrides.
2. Load selected base game semantic data.
3. Apply compatibility base art for the selected mode.
4. Apply global enhancement layers compatible with the selected mode.
5. Apply mission-authored metadata.
6. Apply mission-authored art.
7. Apply explicit per-mission user overlays.
8. Finalize object and texture caches.

This can be implemented incrementally. The first step can simply wrap the
existing calls in named phases and move the most problematic calls to the right
phase.

### Overlay Manifest

Support implicit legacy overlays first, then add optional manifests for richer
behavior.

Legacy behavior:

- `.pog`, `.pg1`, `.dtx`, `.hxm`, PNG files, and `Sounds/` files continue to
  work by filename and PhysFS search priority.

Optional manifest behavior:

```
{
  "id": "example-d1-hires",
  "name": "Example D1 High Resolution Pack",
  "type": "global-enhancement",
  "games": ["d1", "d1-in-d2"],
  "assetFamilies": ["walls", "robots", "cockpit", "sounds"],
  "palette": "truecolor",
  "priority": 100
}
```

The manifest lets the launcher and engine avoid accidental cross-game
application. A D2 high resolution pack should not silently affect D1-in-D2
unless it declares that compatibility.

## Edge Cases

### Cross-Game Name Collisions

D1 and D2 can use the same bitmap name for different art, or similar art with
different palette assumptions. The final lookup key should include the selected
base game namespace when the asset comes from a global overlay pack.

### Index-Based Mods

Some legacy formats replace by bitmap index instead of name. Index-based
replacement must be interpreted in the source game's namespace first, then
mapped to the runtime target slot.

### Frame Numbering

Animated texture packs may provide `name#0`, `name#1`, etc. Base D1 effect
metadata must define the frame group before final overlay image lookup. Do not
mix a D1 frame group with a D2 `#0` frame just because the names collide.

### Palette-Dependent Custom Art

Paletted replacement data authored for one game can look wildly wrong under
another game's palette. D1-in-D2 should treat D1 mission data as D1-paletted by
default, and should treat D2 paletted overlays as incompatible unless declared.

### Runtime Texture Cache

Replacing a bitmap slot must invalidate or refresh any GL texture already
created for that slot. The final overlay pass should happen before normal
gameplay rendering whenever possible, but level reloads and toggled overlays
still need cache invalidation.

### Memory Ownership

Current replacement paths use several ownership models. D1 base replacements,
D1 custom replacements, D2 POG replacements, and PNG runtime textures do not all
free and restore data the same way. The work plan should avoid adding another
ad hoc ownership stack. Longer term, replacement memory should be owned by the
asset layer registry or by a small set of family-specific stores.

### Shareware and Variant Data Files

D1 shareware, OEM, Mac, and patched data files can have different PIG contents.
The D1-in-D2 mapping should prefer names and table data from the actual base
files present, and diagnostics should report unknown variants.

### Multiplayer

Custom texture and model overlays can affect fairness and demo determinism. The
existing multiplayer custom texture rules should continue to apply. Any new
overlay category that affects geometry, robot behavior, weapon behavior, or hit
data needs a stricter compatibility gate than pure visual replacements.

## Implementation Phases

### Phase 1: Inventory and Trace

Status: complete.

Deliverables:

- Document every existing loader that can replace art, sound, or table data.
- Add lightweight trace logging or introspection for the current level:
  active base game, active palette, loaded overlay files, replacement counts.
- Include D1-in-D2 Trine 2 in the trace output.

Progress:

- Added D1 base wall replacement counters from `load_d1_bitmap_replacements()`.
- Added D1-in-D2 compatibility counters for effect frames, powerup vclip frames,
  robot table/model counts, robot object bitmap replacements, and robot objects
  updated.
- Added an Android debug introspection `asset_trace` object with runtime mode,
  selected base game, active palette fields, current pigfile, D1 base wall
  stats, D1 compatibility stats, and D1 custom stats.
- Left the existing flat `d1_custom_textures` introspection object in place for
  compatibility with existing scripts.

Validation:

- Launched Trine 2 in D2 with
  `android/game_scripts/test_trine2_d1_in_d2_custom_textures.json5`: passed.
- Confirmed the trace identifies D1-in-D2, D1 palette, D1 base wall
  replacements, D1 custom files, D1 effects/vclips, and robot overlay status.

### Phase 2: Stabilize Layer Order

Status: not started.

Deliverables:

- Group the current D1-in-D2 calls into named phases.
- Ensure D1 base compatibility layers run before mission and user overlays.
- Ensure robot compatibility metadata does not run after final robot texture
  overlays.
- Keep native D2 behavior unchanged except for diagnostics.

Validation:

- Trine 2 wall textures, destroyed effects, powerups, and robot textures match
  D1 visual intent.
- A native D2 mission with POG and HXM data still behaves as before.

### Phase 3: Broaden Legacy Format Support

Status: not started.

Deliverables:

- Let D1-in-D2 sniff and load compatible `.pog` files in addition to `.pg1`
  and `.dtx`.
- Make index-based replacements map through the selected source game namespace.
- Add clear skip reasons for unsupported or cross-game replacement data.

Validation:

- Synthetic D1 mission with `.pg1`, `.dtx`, and `.pog` variants all replace the
  intended final wall slot.
- D2 `.pog` data is not misapplied to D1-in-D2 unless explicitly compatible.

### Phase 4: Robot and Object Art Completeness

Status: not started.

Deliverables:

- Finish D1-in-D2 robot texture correctness.
- Add trace output for robot model and object bitmap winners.
- Add support for final robot texture overlays after D1 robot mapping.

Validation:

- Trine 2 robot textures match native D1 behavior.
- Synthetic robot texture overlay wins over base D1 robot art in D1-in-D2.
- No regression of the `model_num < N_polygon_models` crash fix.

### Phase 5: Cockpit, Gauge, HUD, and Menu Art

Status: not started.

Deliverables:

- Inventory D1 and D2 cockpit/gauge bitmap slots and names.
- Apply D1 cockpit/gauge base art in D1-in-D2.
- Support high resolution cockpit overlays for D1, D2, and explicitly marked
  cross-game packs.

Validation:

- D1-in-D2 cockpit and gauge art use D1 assets by default.
- A cockpit overlay pack can replace one known cockpit/gauge image in each
  supported mode.

### Phase 6: Sounds

Status: started.

Deliverables:

- Confirm and document D1-in-D2 sound source order.
- Add diagnostics for sound replacements.
- Support game-tagged global sound overlays.
- Load D1 base sound maps and raw samples before mission custom sound overlays.
- Restore native D2 sound tables and samples when leaving D1-in-D2.

Progress:

- Added a D1-in-D2 base sound layer that reads D1 `Sounds[]`/`AltSounds[]`
  mappings and raw sample headers/data from `descent.pig`.
- Rebuilds the runtime sound name table from D1 sound names, so D1 mission
  `.pg1`/`.dtx` and base sound overrides resolve against the D1 namespace.
- Added `asset_trace.d1_compat` counters for D1 sound activation, PIG presence
  and size, map entries, sample count, and loaded sample bytes.

Validation:

- D1-in-D2 uses D1 base sounds for D1 missions.
- D2 uses D2 base sounds for D2 missions.
- A compatible external sound override wins by name in the expected mode.

### Phase 7: Overlay Manifest and Launcher Policy

Status: not started.

Deliverables:

- Define a minimal optional overlay manifest format.
- Add launcher-side compatibility tagging for overlay packs.
- Hide or disable incompatible global overlays for the selected game mode.
- Keep legacy file-only overlays working for manually installed mods.

Validation:

- A D1 high resolution pack is offered for D1 and D1-in-D2, not native D2.
- A D2 high resolution pack is offered for D2, not D1-in-D2, unless its
  manifest declares cross-game compatibility.
- Per-mission user overlays can intentionally override mission art.

### Phase 8: Regression Coverage

Status: not started.

Deliverables:

- Add a low-cost integration test or introspection assertion for Trine 2 in D2.
- Add synthetic overlay packs for:
  - D1 wall replacement
  - D1 effect destroyed texture replacement
  - D1 robot texture replacement
  - D1 cockpit/gauge replacement
  - D1 sound replacement
  - D2 POG and HXM non-regression
- Prefer introspection over screenshots. Add introspection fields if visual
  asset source cannot be verified otherwise.

Validation:

- Tests report final source layer and source file for representative assets.
- Test runtime stays small enough to run during normal Android validation.

## Short-Term Code Recommendations

1. Add an asset source trace before more mapping tweaks.
   This will turn visual bug reports into concrete "this source won" answers.

2. Move D1 robot compatibility before final mission and user art overlays.
   This should prevent D1 robot base repair from clobbering overlay textures.

3. Keep D1 robot flat polygon colors in the asset plan.
   D1 robot model data stores flat colors as D1 palette indexes, while D2
   renders flat model colors as 15bpp RGB words. D1-in-D2 now normalizes base
   D1 robot model flat colors at load time; overlay model support needs the
   same treatment if it imports D1-authored polygon data.

4. Split D1 custom loading into metadata and art phases where possible.
   Metadata can establish final references; art can then apply last.

5. Add `.pog` sniffing for D1-in-D2.
   Many packs and tools use POG-like containers even when the mission is D1.

6. Add D1 cockpit/gauge compatibility.
   This is a likely next class of "everything looks like D2" reports once
   walls, powerups, and robots are fixed.

7. Make global high resolution overlays game-aware.
   A D2 texture pack should not affect D1-in-D2 by default.

## Acceptance Criteria

- Trine 2 in D2 uses D1 palette, D1 wall textures, D1 effect animations, D1
  destroyed textures, D1 powerups, D1 robot textures, D1 cockpit/gauge art, and
  D1 sounds unless an enabled compatible overlay intentionally replaces them.
- Native D2 missions continue to use D2 base data and existing D2 POG/HXM
  behavior.
- Native D1 missions continue to use D1 base data and D1 custom data behavior.
- Global high resolution packs can be enabled without silently crossing into
  the wrong game namespace.
- Mission-authored assets override global enhancement packs by default.
- Explicit per-mission user overlays can override mission-authored assets.
- Diagnostics can explain the final source for representative wall, effect,
  vclip, robot, cockpit, and sound assets.
