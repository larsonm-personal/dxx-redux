# Vertigo and Vignettes routing

Add the requested CD Vertigo source and Vignettes archive to the shared routing development set, including metadata source selection and discovery assertions (eight missions, 203 levels). Preserve the prior Plutonia work. Reproduce failures with the current engine, select one from the new missions, diagnose with targeted logs, implement a general correction in source files, and validate with focused repeats and the expanded corpus. TEW 9 and Counterstrike secret -5 remain deferred; do not edit outstanding_bugs.md.

## Completed work

- Added the canonical CD Vertigo metadata file and `Vignettes.json` to the manifest. The CD has 23 campaign levels and one tutorial; Vignettes has 27 levels
- Forwarded `descent-ii-vertigo-usa` to host metadata generation. Explicit CD selection now works alongside an archive filter, instead of being silently discarded
- The short simulation selection now matches manifest-relative filenames exactly. General filename filtering had also selected `d2xxl_downloads/vignettes.json`, incorrectly adding another 27 levels
- Selected Vignettes 8. Baseline timed out at frame 5127 after collecting blue and red keys, with actor in segment 201 and reactor in segment 272
- Diagnostic ray tests showed the stopped actor 544505 fixed-point units (8.3 world units) from the firing point: its ray hit wall 271:5, while the ray from the planned firing point hit reactor object 56. The ordinary arrival tolerance stopped movement before the actual firing line cleared
- The simulator now continues approaching a boss/reactor firing point while its actual shot is obstructed, even when inside the ordinary waypoint arrival tolerance. It retains real collision checks and normal objective activation; no mission constants or header implementation were added
- New repeat integration `android/tests/test_vignettes_level8_firing_position.ps1` completes blue key, red key, reactor and exit in 1949 frames, with identical raw results and agreement with metadata
- Refreshed normalized simulation data for all eight missions. Metadata regeneration for the two new sources produced byte-identical existing metadata, so no metadata rewrite was needed

## Validation

- Full eight-mission corpus: 178/203 pass, versus baseline 177/203. Castaway 10/10, Vertigo 17/24, Counterstrike 29/30, First Strike 30/30, Obsidian 18/18, Plutonia 23/32, TEW 31/32, Vignettes 20/27. No previously passing level changed to a failure
- Corpus output: `android/temp/core_eight_vignettes_fixed`; focused repeat log: `temp/vignettes8_integration.log`
- All 49 native tests pass: `temp/eight_native_tests.log`
- Mission discovery/count and regeneration stage tests pass: `temp/eight_selection_test.log`, `temp/eight_stages_test.log`
- Real combined archive/CD metadata run passes both sources: `android/temp/eight_metadata_sources`
- Scoped formatting/lint passes: `temp/eight_quality.log`
- Additional metadata wiring test exposed a pre-existing stale assertion for five build calls. Confirmed HEAD already contains four Windows/Linux build/fallback calls and updated the assertion accordingly
- Final Windows D1/D2 builds pass (`temp/eight_final_build.log`), the post-format repeat integration passes (`temp/vignettes8_final_integration.log`), and the corrected metadata wiring test passes (`temp/eight_final_metadata_wiring.log`)

The other new-mission failures remain available for later work. TEW 9 and Counterstrike secret -5 were included in the corpus but not investigated.
