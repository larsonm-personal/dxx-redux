# Xfing Minimal DXA Conversion 20260518

## Goal
Create replayable scripts that convert the extracted Xfing D1 and D2 texture packs into minimal DXA archives containing only redistributable patch data, then run and verify the generated outputs.

## Tasks
- [x] Review existing conversion helpers and temporary analysis scripts
- [x] Define a compact patch-data layout for D1 and D2 DXAs
- [x] Write replayable conversion scripts under `android`
- [x] Write verification so generated DXAs can be checked without launching the game
- [x] Run the conversion scripts against `uud1tp` and `uud2tp`
- [x] Run verification and record the generated archive contents and sizes

## Notes
- Generated DXAs must not include full PIG, HAM, HOG, MN2, or RDL files
- Scripts should derive patch data from the existing extracted packs and local baseline files
- Keep intermediate files under the pack `tmp` directories or `game_data/mods/xfing/dxx_tp/tmp`
- This tranche creates packaging scripts and archives only; engine support for consuming the patch format can be a later tranche

## Output
- Scripts:
	- `game_data/mods/xfing/convert-xfing-minimal-dxa.ps1`
	- `game_data/mods/xfing/verify-xfing-minimal-dxa.ps1`
	- `game_data/mods/xfing/xfing_minimal_dxa_lib.ps1`
- Generated DXAs:
	- `game_data/mods/xfing/dxx_tp/tmp/minimal_dxa/xfing-uud1tp-minimal.dxa`
	- `game_data/mods/xfing/dxx_tp/tmp/minimal_dxa/xfing-uud2tp-minimal.dxa`

## Verification Results
- `xfing-uud1tp-minimal.dxa`: 225001 bytes, 123 entries, 120 bitmap payloads, 355494 payload bytes, 1 metadata byte-range delta, 72 level surface deltas
- `xfing-uud2tp-minimal.dxa`: 1355233 bytes, 911 entries, 908 bitmap payloads, 2207978 payload bytes, 26 HAM semantic deltas, 4 skipped optional/no-baseline source files
- `game_data/mods/xfing/verify-xfing-minimal-dxa.ps1` passed for both generated DXAs
- `android/run-code-quality.ps1 -Fix` passed for the new scripts and plan

## Follow-Up
- Add launcher or engine support to apply `xfing-minimal` patch-data DXAs before or during asset loading
- If a baseline exists for UUD2's D1-palette `DESCENT.PIG`, rerun `game_data/mods/xfing/convert-xfing-minimal-dxa.ps1 -D2DescentBaselinePig <path>` to include those deltas