# Xfing Texture Pack DXA Study 20260517

## Goal
Study the extracted Xfing D1 and D2 texture fix packs and determine the smallest legal DXA packaging path for each game.

## Tasks
- [x] Review prior DXA and hires texture conversion plans and scripts
- [x] Inventory `game_data/mods/xfing/dxx_tp/uud1tp` and `game_data/mods/xfing/dxx_tp/uud2tp`
- [x] Identify whether standalone replacement texture sources exist in the distributions
- [x] Inspect archive formats and texture records enough to explain why direct DXA packaging was not used
- [x] Prototype extraction or comparison helpers in pack `tmp` directories if existing tools are insufficient
- [x] Summarize minimal DXA contents, masking needs, and implementation gaps

## Notes
- Keep working files under `game_data/mods/xfing/dxx_tp/uud1tp/tmp` and `game_data/mods/xfing/dxx_tp/uud2tp/tmp`
- Avoid copying full commercial data into any proposed output pack
- Prefer plain replacement or small mask assets over the previous ETC2 hires texture pipeline unless investigation proves otherwise

## Findings
- Current UUD1 distribution contains a full patched `descent.pig`, three patched loose RDLs, and RTF notes. It does not contain standalone source textures.
- Current UUD2 distribution contains six full patched D2 PIGs, a patched D1-palette `DESCENT.PIG` plus `descent.256`, `pogtest` showcase files, a DXA containing only `DESCENT2.HAM`, and RTF notes. It does not contain standalone source textures.
- Existing Android Redux DXA mods are ZIP/DXA archives with root-level replacements such as `.ktx2`, `.png`, masks, sounds, and XL menu art. The current engine also mounts enabled DXAs through PhysFS before base data, so a DXA can technically override whole PIG/HAM files, but doing so would still distribute full commercial data.
- UUD1 bitmap payload comparison against the local registered D1 baseline found 118 changed bitmap records, 0 added records, and 350041 bytes of changed raw bitmap payload. The three patched levels change only small texture remaps: levels 3 and 17 replace texture id 14 `rock019` with id 8 `rock004`; level 20 replaces overlay texture id 309 `misc037` with id 246 `metl127` on two faces.
- UUD1 PIG metadata differs from the baseline by only two bytes in the pre-bitmap-data area, inside an apparently unused wall animation frame slot. The user-visible UUD1 needs are the changed bitmap payloads plus the small level texture-remap masks.
- UUD2 standard PIG comparison found 899 changed or added bitmap payload records across the six palettes, plus three missing entries in patched PIGs, for 2189940 bytes of changed raw bitmap payload. The union is 180 bitmap names, including 42 names that only appear as added records.
- UUD2 patched `DESCENT.PIG` is a D2-style palette PIG with 2619 entries and no matching workspace baseline, so it was inventoried rather than diffed.
- UUD2 HAM comparison found 18 added texture table rows, no vclip changes, 3 changed and 2 added eclips, and 2 changed and 1 added wclips. These include spider monitor setup, lava/trickle effects, door speed changes, and a new door animation.
- The original UUD2 notes say Rebirth can use the HAM from DXA but PIGs must be extracted because modified PIGs do not work from inside DXA there. Redux Android's PhysFS path likely removes that technical limitation, but legal/minimal packaging still requires patch data rather than whole PIG/HAM files.
- Best next implementation path is a small base-data patch format mounted from DXA: per-game/per-palette raw PIG bitmap payload deltas keyed by record index plus compact table deltas for D2 HAM and compact level texture-remap masks for D1. Existing root image replacement is useful for simple existing-name replacements, but it cannot add bitmap records, represent duplicate-name/index-specific records, or update HAM texture/effect/wall animation tables.