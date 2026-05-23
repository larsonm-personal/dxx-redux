# CD audio SAF multi-BIN follow-up plan

## Goal
- Fix the post-readd SAF CD source issues where source-level removal is confusing, tree permissions appear as separate redundant rows, and multi-BIN SAF sources misidentify or fail preview/playback

## Plan
- [completed] Confirm the controlling SAF CD import and playback path, then choose the smallest root fix
- [completed] Patch SAF CD registration so multi-BIN sources normalize into a launcher-local single-BIN source while single-BIN sources keep the in-place SAF path
- [completed] Tighten the Advanced SAF viewer so CD entries present as source-level rows and suppress redundant owned permission rows
- [completed] Add focused launcher unit tests for the new CD source normalization/viewer filtering helpers where practical
- [completed] Run focused Kotlin compile and unit tests, then run the Android code-quality pass on the touched paths

## Notes
- Current local hypothesis: SAF CD registration only persists the first BIN URI, while fingerprinting and preview later assume one BIN handle, so multi-BIN cue sheets are stored incompletely and get bad chromaprint results or non-working previews
- Cheap disconfirming check: verify whether the native preview/game paths can already consume multiple BIN files from the stored source model. Readout confirmed they do not; both collapse to one BIN handle

## Result
- Added launcher-side normalization for multi-BIN SAF disc imports by merging the selected BIN files into a local single BIN and writing a rewritten local CUE with cumulative track offsets
- Kept single-BIN SAF imports on the existing in-place path, so the change only widens support where the old source model was incomplete
- Changed the Advanced SAF viewer to show CD entries at source granularity and suppress tree-permission rows that are already covered by tracked child URIs
- Updated CD-source visibility checks so local path-backed sources are not hidden by the SAF-only broken-link filter
- Fixed merged-local CD preview playback to treat absolute local BIN paths as local files instead of trying to reopen them as SAF URIs
- Added merged-local import cleanup to release no-longer-needed persisted SAF permissions after the local BIN/CUE copy is complete
- Updated the Advanced SAF viewer to surface merged-local CD sources as removable source entries so source removal is no longer gated on the separate music-page `x`

## Validation
- `android\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest --tests com.dxxredux.app.SafUriPermissionsTest --tests com.dxxredux.app.CdAudioSourceVisibilityTest --tests com.dxxredux.app.CdAudioSourceNormalizationTest`
- reran the same focused Gradle command after `android\run-code-quality.ps1 -Fix` rewrote files
- `android\run-code-quality.ps1 -Fix` was invoked for the touched launcher files, but the wrapper treated the provided `-Paths` list as missing and fell back to a repo-wide pass before reporting the same existing non-autofixable max-line-length warnings in `SetupActivity.kt`
- reran the same focused Gradle command after the merged-local preview and permission-cleanup follow-up changes and again after the formatter pass
