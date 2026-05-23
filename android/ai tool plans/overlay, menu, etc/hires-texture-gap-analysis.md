# Hi-Res Texture Gap Analysis

## Status
Analysis complete for both D1 and D2. All fixable D2 pack naming issues
have been addressed in convert_d2xxl_textures.ps1 (Rename-D2xxlTextures
function) and the merge pass in convert_all.ps1. D1 has no naming
convention mismatches -- all unmatched entries are addon/expansion content
not in the base PIGfile. Re-run convert_all.ps1 to regenerate packs.

## Current Status (128px pack, pre-fixup)
- **396 KTX2 names** in the pack match D2 PIGfile bitmap names
- **394 textures uploaded** (2 skipped due to RGB-only ETC2 lacking alpha: cockpit, metl154)
- **73 unmatched** KTX2 names in the pack (pack naming issues, not game code issues)
- ogl_cache takes ~2 seconds for 2590 bitmaps

## Expected improvement after fixup rebuild
- boss2_01-15 -> boss02#0-14: +15 textures (512px archive only, merge adds to 256/128)
- arw01!0 -> arw01#0: +1 texture (256px archive, merge adds the rest)
- targ -green renamed to base name: +20 matched textures; -red left unmatched
- D1-only rock/metl, redundant base names, addon textures: left in archive
- Net: ~36 more usable textures loaded; unmatched entries remain harmlessly

## 256px Pack Test Results
- **485 KTX2 names** match D2 PIGfile bitmap names (89 more than 128px)
- **479 textures uploaded** (6 skipped for alpha: muzl02#0-09#0 muzzle flashes)
- **124 MB** on disk (vs 5.7 MB for 128px)
- **~15 MB GPU memory** (vs ~3 MB for 128px)
- **~2 second load time** -- no performance concern
- **No code changes needed** -- works out of the box with current loading system

## Unmatched KTX2 Textures -- resolution status

### Boss animation frames (15 textures) -- FIXED
Pack: `boss2_01` through `boss2_15`
Game: `boss02#0` through `boss02#14`
Fix: Rename-D2xxlTextures renames boss2_NN -> boss02#(NN-1)

### Targeting reticle color variants (20 textures) -- FIXED (green renamed)
Pack: `targ01b#0-green`, `targ01b#0-red`, etc
Game: `targ01b#0` (color is applied via PlayerCfg.ReticleRGBA tinting)
Fix: Rename-D2xxlTextures renames -green to base name (overwrites base).
-red variants left as-is (unmatched, harmless in archive)

### Animation base names without frame suffix (3 textures) -- rename only
Pack: `arw01`, `arw01!0`, `door01`
Game: `arw01#0`-`arw01#5`, `door01#0`-`door01#10`
Fix: Rename-D2xxlTextures: '!' -> '#'. Base names (arw01, door01) left
as-is after strip split (unmatched, harmless in archive)

### Lava zero-padding (1 texture) -- NOT FIXED (benign)
Pack: `lava6#0` (only in 256px archive)
Game: `lava06#0`
The 512px archive already has the correct `lava06` name. The merge pass copies
from 512 to 256 so the correct version ends up in all packs. The bad `lava6`
entry remains harmlessly (no matching bitmap, wastes a few KB).

### D1-only texture names not in D2 PIGfile (10 textures) -- left in archive
`rock001`, `rock002`, `rock006`, `rock007`, `rock265`, `metl131`-`metl135`
These are D1 bitmap names included in the D2 archive. D2's PIGfile doesn't
contain these names. Left as-is (no matching bitmap, harmless in archive)

### Non-PIGfile addon/effect textures (24 textures) -- NOT FIXED (engine limitation)
`blast`, `bubble`, `corona`, `fire`, `flare`, `glare`, `halo`, `halfhalo`,
`shield`, `smoke`, `sparks`, `thrust2d`, `thrust3d`, `deadzone`, `joymouse`,
`monsterball`, `mballmask`, `bullcase`, `bulletcase`, `cockpitbx2`, `statusbx2`,
`hires-cockpit`, `pwupicon`, `rboticon`
These are d2x-xl addon bitmaps not registered in the PIGfile.
`piggy_game_bitmap_name()` returns NULL, so KTX2 lookup is skipped entirely.
Left in pack harmlessly. Would need engine code changes to support.
TODO: add KTX2 lookup to addon bitmap loading paths if desired.

## Alpha-Skipped Textures

### 128px pack (2 skipped)
- `cockpit` -- cockpit gauge background (needs transparency for HUD overlay)
- `metl154` -- metal wall texture with transparency

### 256px pack (6 skipped)
- `muzl02#0`, `muzl03#0`, `muzl05#0`, `muzl06#0`, `muzl08#0`, `muzl09#0` -- muzzle flash effects

All skipped because KTX2 is GL_COMPRESSED_RGB8_ETC2 (no alpha channel) but the
bitmap has BM_FLAG_TRANSPARENT or BM_FLAG_SUPER_TRANSPARENT set.
Fix: regenerate these KTX2 files with GL_COMPRESSED_RGBA8_ETC2_EAC format.

## Memory Budget for Larger Packs

| Pack | Disk Size | GPU Memory (est) | Files | Matched | Load Time |
|------|-----------|-------------------|-------|---------|-----------|
| 128px | 5.7 MB | ~3 MB | 469 | 396 | ~2s |
| 256px | 124 MB | ~15 MB | 568 | 485 | ~2s |
| 512px | 455 MB | ~62 MB | 589 | ~500? | ~3-4s? |

## Recommendations

### Pack Quality -- DONE in convert scripts
1. DONE: Boss animation frame naming (`boss02#0` not `boss2_01`)
2. DONE: Exclamation/hash frame syntax (`arw01!0` -> `arw01#0`)
3. DONE: Targ green renamed to base name (red left unmatched in archive)
4. Left in archive: D1-only names, redundant base names, addon textures
5. Benign: Lava zero-padding (`lava6` in 256 archive, merged from 512)

### Remaining improvements (not script-fixable)
1. Regenerate alpha-needing textures with RGBA ETC2 format (~8 textures)
2. Add KTX2 lookup for addon bitmaps loaded outside PIGfile (~24 textures)
3. For 512px: consider per-level KTX2 loading (skip BM_FLAG_PAGED_OUT in pre-cache)
4. For very large packs: measure and log total GPU texture memory usage

## Data Files
Texture name dumps saved for reference:
- `temp/ktx2_names_128.txt` -- KTX2 names in 128px D2 pack
- `temp/ktx2_names_256.txt` -- KTX2 names in 256px D2 pack
- `temp/game_bitmap_names.txt` -- all 2590 D2 PIGfile bitmap names
- `temp/d1_pigfile_names.txt` -- all 1555 D1 PIGfile bitmap names (from DESCENT.PIG)
- `temp/d1_dxa_512_names.txt` -- KTX2 names in D1 512px DXA pack
- `temp/d1_512_files.txt` -- raw TGA filenames in D1 512px source archive
- `temp/d1_256_files.txt` -- raw TGA filenames in D1 256px source archive

## D1 Texture Gap Analysis

### Summary
- D1 512px DXA: 337 entries, **276 matched** (82%), 61 unmatched
- D1 256px archive: 300 files, **256 matched** (85%), 44 unmatched
- **No naming convention mismatches** -- all unmatched are addon/expansion content

### Unmatched D1 entries (all are content not in base PIGfile)
Unlike D2, D1 has no naming convention differences (no boss underscore,
no exclamation syntax, no targ color variants). All 61 unmatched entries
are textures for content absent from the base D1 PIGfile:
- 26 high-numbered rocks (281-356) -- PIGfile stops at rock269
- 7 high-numbered doors (32-53) -- PIGfile stops at door31
- 5 misc textures (080-092) -- not in PIGfile
- 4 ceil 'b' variants (ceil024b etc.) -- d2x-xl addition, no 'b' in PIGfile
- 4 missile variants (scmiss, shmiss) -- not in PIGfile
- 3 base name duplicates (arw01, door01, flare) -- strip originals
- 3 lava textures (lava03, lava06, lava6_01) -- PIGfile only has lava02
- 3 box textures (box01a, box01b, box02a) -- no box entries in PIGfile
- 2 blown high numbers (102-103) -- PIGfile only has blown01-07
- 1 bluegoal (CTF mode, not in base D1)
- 1 exit02-256 (resolution variant of exit02)
- 1 metl145 -- PIGfile ends at metl140
- 1 water01 -- not in PIGfile
All left in archive harmlessly (no matching bitmap to bind to)
- `temp/etc2_uploaded.txt` -- 394 names successfully uploaded from 128px pack
- `temp/ktx2_not_uploaded.txt` -- 75 names in 128px pack that didn't match
