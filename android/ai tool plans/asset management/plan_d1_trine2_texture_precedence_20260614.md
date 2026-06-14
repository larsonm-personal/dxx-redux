# D1 Trine 2 Texture Precedence Investigation

- [x] Trace D1 texture conversion for animated overlays, including frame `#0` handling.
- [x] Compare D1 and D2 eclip metadata for `misc066` and the destroyed texture orientation.
- [x] Identify why many D1 textures still resolve through D2 texture/effect metadata.
- [x] Implement the smallest safe correction.
- [x] Run focused validation.

## Notes

- User observed Trine 2 in D2 cycling through `misc066#0`, while D1-style frames appear to be `#1` through `#6`.
- User also observed the blown texture mirrored left to right and many other incorrect textures, suggesting D2 texture/eclip metadata is still taking precedence over D1 metadata.
- D1 eclip 24 uses 9 `misc066` frames and destroys to D1 texture 350, while D2 eclip 24 uses 7 frames and different D2 artwork/destination.
- The old D1 replacement path only replaced textures considered unique to D1. That intentionally left many same-name but visually different textures using D2 bitmap data.
- The fix now replaces all D1 texture-mapped bitmaps while emulating D1 and reloads D1 effect frame bitmaps into the D2 frame slots used by the matching effect.
- Local D1 PIG size estimate for mapped textures plus effect frames is about 2 MB; the D1 replacement buffer was increased to 5 MB.
- Scoped code quality passed. Android native build `:app:externalNativeBuildDebug` passed.

## Follow-Up

- User playtest found the broad all-mapped D1 replacement path makes most textures worse, with many green/palette-wrong surfaces.
- The useful part appears to be animation consistency, not blanket replacement of same-name base textures.
- Follow-up root cause is that D1 levels were still falling back to D2's `groupa.256` palette. Broad D1 bitmap replacement must be paired with D1's `palette.256`.
- D1's `palette.256` lives in `descent.hog`; D1 levels now select it when visible through PhysFS, and the D2 palette loader skips the nonexistent inferred `palette.pig`.
- Re-ran scoped code quality and Android native build after the D1 palette change.
- User playtest now reports wall/level textures look good, but robot and powerup textures or palettes look wrong.
- Likely cause: wall/effect bitmaps are replaced with D1 art, while powerups and robots still use D2 object/vclip/polymodel bitmap tables under the D1 palette.
- Current pass applies D1 vclip metadata and frame bitmaps while D1 emulation is active. This should fix powerups and other sprite/vclip art first; robot polymodel/object bitmap replacement remains a larger follow-up if robot art is still wrong.
- User saw no visible change from the vclip pass.
- Local registered D1 `descent.pig` stores `Num_vclips` as 0 even though the fixed 70-entry vclip table follows and D1 reads all 70 entries. Treat 0 as `D1_VCLIP_MAXNUM` for D1 compiled properties.
- User confirmed shield and energy powerups are now correct, so the D1 vclip path is active.
- Robot textures still use D2 robot model/object bitmap tables. Next pass overlays D1 robot info, joints, polygon models, object bitmaps, and object bitmap pointers while D1 emulation is active.
- Binary sanity check against local registered D1 data reads plausible counts with the overlay layout: 24 robot types, 500 robot joints, 30 weapons, 28 powerups, and 78 polygon models.
- Important serialized sizes: D1 `robot_info` is 486 bytes via field reader, and `jointpos` is 8 bytes via `jointpos_read_n`.
- Crash in newest build: `ASSERT FAIL: model_num < N_polygon_models` in `polyobj.c` while running Trine 2 in D2.
- Root cause: D1 robot overlay replaced the polygon model table and shrank `N_polygon_models` to the D1 count, but other runtime objects can still reference preserved D2 model slots. Keep the D2 model/object bitmap counts and only replace the D1 slots.
- Newest playtest still shows extremely wrong robot textures after the crash fix.
- Static analysis found a likely storage bug: D1 robot art was loaded into the existing D2 `ObjBitmaps[i]` bitmap slots. Those slots can alias the same underlying `GameBitmaps[]` entries, so multiple D1 robot textures could overwrite each other before rendering.
- Current pass gives D1 robot object bitmap entries a private registered range of runtime bitmap slots and points the D1 `ObjBitmaps` table at those slots. Failed or zero D1 object bitmap entries now resolve to bitmap 0 instead of stale D2 art.
- Scoped code quality passed. Android native build `:app:externalNativeBuildDebug` passed.
- User playtest improved to a few correct robots, but most robot textures are blue.
- Offline parsing of registered D1 `DESCENT.PIG` shows model-used D1 robot object bitmap entries are valid: 171 model texture pointers, 152 unique object slots, 0 bad pointers, 0 zero D1 bitmap refs, and 0 out-of-range D1 bitmap refs.
- The next likely fault was allocator overlap. The prior private range used `Num_bitmap_files`, but D2 extra object bitmaps are allocated with the separate `extra_bitmap_num` cursor. Exit/model/extra bitmap slots can already exist above `Num_bitmap_files`, so the D1 robot range could overlap or be overwritten.
- Current pass moves D1 robot replacement art to a fixed high private range inside `GameBitmaps[]`, avoiding both `Num_bitmap_files` and `extra_bitmap_num` while keeping ownership with the D1 replacement buffer.
- User playtest now reports the vast majority of robot textures are correct, with the class 1 driller ears still blue.
- Static trace of D1 model 30/31 shows class 1 driller uses object bitmap slot 32 for `glow02#0`. D1 effect 32 animates object bitmap slot 32 with `glow02` frames.
- The D1 effect overlay had copied D1 wall-changing effects but left `changing_object_texture` from D2. Current pass copies valid D1 `changing_object_texture` values too, so D1 object glow animations drive the D1 robot object slots.
