# XL checkpoint and asset-stack integration

This branch combines existing work only. It does not enable soft bases in ordinary world rendering, change gameplay admission, add another asset feature, or include PR131's independent CI deployment-record cleanup.

## Pinned inputs and ancestry

- Checkpoint `xl_more`: `d5f78cedadae973238f5075407f488965a3d6841`
- Asset stack top, PR136: `471cd45fc129d6b7d3bbddcbefe7f8387d75c3b6`
- Their common ancestor: `5c35f11ccb0e9ea52234107d1902a5f781cf3513`
- Final browser, PR124: `8acea2079c8c7f5724da52796854cee7e9f03bc1`
- Package study, PR126: `d2a284497e9927623118d94e10def3a03219f377`, which contains that browser tail

The checkpoint and asset stack had forked before four final browser instrumentation fixes. Importing the study history also preserves those fixes and its documentation. Two normal merge commits preserve the checkpoint, study/browser and complete asset history. No published history is rewritten.

## Deliberate conflict resolutions

The browser test keeps the checkpoint's original-package evidence checks and directional accessibility actions, plus the final browser's bounded touch fallback, settled-position checks, explicit return-to-list synchronization and 240-second runner budget. No assertions are removed to accommodate the new package catalog.

The runtime keeps both classic companion-lighting preparation and the asset stack's centralized `xl_preview_clear_mission`. Committed teardown releases classic ambient colors as well as XL source, prepared resources, dynamic buffers and pilot identity. A read-only failed level preflight retains the still-current classic lighting. The original Dragon Pit ownership regression checks every ambient color after that rejection and still checks actual `free_mission` cleanup, failed companion preparation, foreign-shadow rejection, recovery, restart and native save/restore. This resolves the checkpoint's eager preflight release against the stack's validate-before-commit contract; it is not a blanket clearing of a valid prior mission.

The package audit retains the explicit imported-D1 ownership-versus-D2-texture-capability assertion and all checkpoint lighting/destruction/save assertions. `xl_runtime_active` may describe the explicit classic-lighting preview, but is not used as texture authorization. Validated mission texture capability, imported-D1 and multiplayer exclusions, and developer-probe build restrictions remain intact.

Automatically combined changes retain visibility source registration, both sets of test registrations, package metadata's distinction between file candidates and gameplay support, the companion/WAV ownership path, and the checkpoint's render/sound diagnostics. The ordinary world renderer still does not call PR136's soft-base primitive. No core save format is changed.

## Required combined validation

Run native contracts, original Dragon Pit ownership/lighting restart/save/restore, Observatory/Robotech/Counterstrike lifecycle checks, original import-to-resolution and package-catalog JVM tests, both Android engine builds and instrumentation compilation. Run the existing synthetic GPU texture campaign and browser dialog campaign, then ordinary engine/ASan, tooling, LAN and replay workflows on the combined exact head. Preserve private mission bytes locally; no original-artwork GPU or full mission-loading GPU claim follows from synthetic fixtures.

Existing parent-head successes do not qualify this combined tree. Independent integration review and terminal combined-head results precede consolidation into a destination branch. PR131 remains a separate decision; no new feature work is part of this integration.

## Local integration evidence

The combined native engine builds. Nine contracts, the three existing original-resource lifecycle checks, Dragon Pit's original lighting decoder and its full ownership/lighting restart/save/restore audit pass (14 distinct checks). The first Dragon Pit attempt failed before testing because its private output directory lacked the imported-D1 retail PIG/HOG; staging the already available original files resolved that setup error without changing assertions. Scoped formatting passes. Android compilation, JVM/catalog checks, GPU/dialog campaigns and ordinary CI are still required on the published combined head.

The first combined engine CI exposed a checkpoint test-harness dependency gap: extracted automation parser types now use fixed-point vectors, and terminal/load fragments call still-view restoration. The standalone host test did not include those types or observe that cleanup seam. The integration fixes the harness using real engine headers and a counted platform sink, with valid-boundary/malformed-coordinate and replacement/failure/completion cleanup assertions. The D2 CMake test and a D1-header build of the same extracted production-fragment test pass. No production automation behavior is changed by this fix. Combined-head CI must rerun after it.
