# High-resolution texture release and runtime visibility

## Objective

Restore supported D1/D2 texture packs as recommended GitHub downloads, preserve authored mip chains, and make actual in-game pack use explicit

## Work

- [x] Rename the legacy D1 presentation pack and verify D1-in-D2 presentation support, coordinating with current sound/presentation worktree changes
- [x] Replace blanket 2048-pixel archive rejection with bounded, device-aware texture loading and authored-mip fallback
- [x] Upload and filter all supplied KTX2 mip levels in both renderers, tracking only successful uploads
- [x] Expose active texture pack identity, resident counts and reasons for inactivity in Video Info and introspection
- [x] Validate enabled/disabled packs through normal launcher import in D1, D2 and D1-in-D2, plus actual GLES mip upload/filter tests
- [x] Prepare reproducible versioned assets, checksums, credits and release notes; publish a dedicated GitHub release and wire recommended downloads
- [x] Run scoped formatting, relevant builds and registered integration tests

## Initial findings

- All six May 18 texture archives exist and the D2 hashes match the existing automation dependencies
- Five packs include images larger than 2048, rejected by current launcher and native reader even when the device supports them
- Both renderers upload only mip zero despite supplied mip chains
- Current Hires counter aggregates replacement bitmaps without source attribution
- Existing mod-loading automation writes an enabled manifest, bypassing normal import validation
- Concurrent worktree changes add sound/presentation provenance and scoped D1-in-D2 presentation loading; preserve them

## Verification requirements

Use renderer evidence for successful authored mip uploads, supported dimensions, filtering and context recreation. Use launcher import instead of hand-written enabled manifests for release acceptance. Compare disabled/enabled/disabled engine runs and ensure status reflects actual uploads and multiplayer policy. Check D1 presentation in native D1 and D1-in-D2. Treat mounted archives and old plan checkmarks as insufficient evidence

## Evidence collected

- Both native engines built for x86_64 and passed 48 real GLES fixture cases each, including sampled authored mip colors, RGB/RGBA, filtering changes, context recreation, malformed chains and a real 4096px pack entry adapted to a 2048px cap. Final trace: `android/temp/texture-labels/run_b45ca5ebfa284068ac89393c8441bb60`
- Isolated APK build and 12 targeted scanner/status/overlay layout tests passed, including the follow-up build for canonical installed names and visible-panel automation (`android/temp/hires-isolated-package.log`)
- All six original archives passed CRC, path, ETC2 layout, mip bounds and credit validation. Published GitHub asset sizes and SHA-256 digests match exactly
- Scoped code quality, native dimension checks and both automation catalogs passed
- All 21 fresh-process baseline/import/removal scenarios passed across D1, D2 and D1-in-D2 at 128/256/512. Engine assertions and the visible Video Info panel agreed on active/none-loaded status. Captured JSON, logcat and screenshots: `android/temp/hires-release/run_9046347e510d4ef1abe3bd8538887031`
- Recommended 256px level-one samples: D1 loaded 12 mipmapped replacements, D2 loaded 21 and D1-in-D2 loaded 13. Every removal returned to zero. The D1 presentation archive supplied five high-resolution fonts and 640x480 images in both D1 modes and did not leak into D2
- Release: https://github.com/larsonm-personal/dxx-redux/releases/tag/texture-packs-v1 (six DXAs, checksums and machine-readable manifest; 256px recommended). Existing archive bytes and authored mip chains were preserved
- This is an assets-only release. Application fixes are in the worktree and tested APK; they still need inclusion in the next distributed Android APK. The texture release is not marked as the latest application release

## Follow-up: verified mods and repeatable publication

- [x] Read the completed DXA sound/presentation task and rerun its host payload/mixer verifier against all four modes and the current archives
- [x] Extend release preparation to include D1/D2 sounds, D1 menus/fonts and Mac D1 missing sounds with checksums, content metadata and compatibility notes
- [x] Add an explicit publish option that updates the same release, replaces changed assets, skips identical files and verifies remote hashes
- [x] Run scoped code quality, publish the additional mods, verify public downloads and rerun publication to confirm stable release/asset identities

Follow-up evidence:

- `verify_dxa_asset_usage.py` passed against the current archives and all four recorded modes: D1 (98), D2 (183), D1-in-D2 (98) and Mac D1 (98) matching resident samples, with playback and off/on/off restoration
- Scoped code quality passed (`android/temp/mod-release-format.log`)
- Publication uploaded six new/changed assets and retained seven unchanged assets, expanding the same release to ten DXAs and three metadata files. All thirteen remote sizes and SHA-256 digests matched (`android/temp/mod-release-publish.log`)
- Immediate republish reported zero changed assets and thirteen unchanged. Release ID, asset IDs, asset hashes/timestamps and local metadata modification times stayed identical (`android/temp/mod-release-republish.log`, `mod-release-after-first.json`, `mod-release-after-second.json`)
- Compared with `android/temp/mod-release-before.json`, all six original texture asset IDs stayed intact and the checksums asset was replaced, exercising overwrite behavior on the existing release
- All four added public download URLs returned HTTP 200 with the expected archive lengths. The latest application release remains `android-v1.0.0`
