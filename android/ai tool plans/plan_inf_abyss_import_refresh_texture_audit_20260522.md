# Infinite Abyss import refresh and texture-loading audit

## Status

- [x] Reproduce the reported symptoms from logs and code paths
- [x] Trace setup-screen readiness refresh after disc/music import dialogs close
- [x] Trace hash/version display refresh after required file imports
- [x] Investigate slow texture-loading logs and storage-location implications
- [x] Audit Infinite Abyss over-extraction and app-storage list scalability
- [x] Summarize causes and propose a fix work plan
- [x] Refresh music readiness and required-file version labels without restart
- [x] Filter Android ISO/data-track extraction to skip installer/runtime junk
- [x] Move app storage inspector scanning off the UI thread and cap initial rendering
- [x] Reduce dead Android replacement-directory texture probes in d1 and d2
- [x] Build a shared in-memory PhysFS texture file index for replacement lookups
- [x] Prefer the remaining unfinished action over Done in the disc-import controller focus order
- [x] Show cue plus bin-count summaries for SAF-linked CD sources
- [x] Label 1-byte case-variant helper symlinks in App Storage Files and sort them next to their companion files
- [ ] Add follow-up runtime coverage for direct Infinite Abyss import and storage inspector behavior

## Notes

- Scope: research and work-plan pass for four reported issues after Infinite Abyss import
- Primary evidence: `android/temp_game_logs/game_logs_inf_abyss_import_problems_and_slow_texture_loading.txt`, launcher setup/import code, storage-location code, and native extraction filters

## Findings

- Music readiness: `MusicInfoSection` keeps a remembered `AudioSourceManager` whose `sources` list is loaded once in the manager constructor. The disc import dialog registers audio through a different manager instance, so `onRefresh()` can fire but the section calls `getSources()` on stale cached data. Opening/re-entering the music UI or restarting constructs a fresh manager and shows ready
- Music readiness secondary detail: SAF disc audio registration calls `onChanged()` before `enableRedbookInConfig()`, so a refresh can also race ahead of the `music_mode=cd` preference write
- Required-file version display: setup startup hashing/pruning is keyed only on `activeSetName`. After disc import, `checkFiles()` can mark files found, but `AssetManifest` entries are not created until the startup/active-set effect runs again, so `FileStatusRow` has no `versionDisplay` to show
- Texture loading: the log has 2,623 `type=texture` samples totaling about 165.9 s. About 40.6 s is KTX2 lookup time, 118.7 s is PNG/JPG/TGA lookup time, and only 3.7 s is GL upload. This points to repeated failed replacement-texture probes through PhysFS, not slow stock texture upload
- Storage location: the active set path in the log is `/storage/6634-3535/Android/data/com.dxxredux.app/files/imported/sets/default`, and PhysFS puts that SD-card path first in the game search path. Slow removable storage makes every missing replacement-texture probe expensive
- Infinite Abyss over-extraction: local extracted `data_tracks` has 476 files, 368 under `winsetup`. The junk is only about 15.6 MiB, but it creates hundreds of irrelevant entries and slows import/UI inspection. Native Android ISO extraction currently passes a null extension filter, so it writes everything from the ISO data track
- App storage files crash risk: `AdvancedSettingsPage` builds the full recursive storage file list synchronously during composition, calls `length()` for every file, and then renders every row in a dialog. On a slow SD-backed import root with hundreds of files, this can block the UI thread or overload Compose/accessibility

## Proposed Fix Plan

- Refresh/music: make `AudioSourceManager` reloadable or make `MusicInfoSection` construct a fresh manager on `refreshTrigger`; move `enableRedbookInConfig()` before `onChanged()` in disc audio registration; call `onChanged()` after both extraction and audio registration finish
- Refresh/hash: change the setup prune/hash effect to also run when `refreshTrigger` changes, or introduce a dedicated post-import audit trigger. After hashing writes `assets.json`, bump refresh once more so `FileStatusRow` recomputes with manifest/version data
- Texture performance phase 1: add a negative cache or precomputed replacement-texture index for Android texture lookup so missing `*.ktx2`, `*.png`, `*.jpg`, and `*.tga` probes are skipped after first miss. Mirror in both `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- Texture performance phase 2: add diagnostics that attribute replacement lookup time by PhysFS search root or compare an internal-storage import root run against the SD-card run. This will confirm how much comes from removable storage vs. miss count
- Extraction filtering: make Android ISO extraction use a game/disc-asset extension allowlist before writing files. Include `.sow` so post-processing still expands installer archives, plus game data/movie/mission/demo extensions. Keep desktop test extraction broad if needed, but keep Android user imports filtered
- App storage inspector: move recursive file scanning to `Dispatchers.IO`, show a loading/progress state, cap or group rows by directory by default, add stable LazyColumn keys, and avoid making every file row focusable/clickable at once. Add a summary that shows hidden/omitted rows when the list is large
- Tests: add a JVM/unit test for audio manager reload or music readiness recomputation, add a launcher integration script that imports a CD image and asserts music ready plus version hints after the dialog closes, add native extract tests proving ISO filter skips Windows installer files while retaining `.sow` and required game files, and add a storage-inspector unit test for large list shaping

## Implementation Progress

- Done: `SetupActivity.kt` now rebuilds the music summary on `refreshTrigger`, reruns the manifest prune/hash audit on setup refresh, and writes `music_mode=cd` before the SAF audio-registration refresh fires
- Done: Android JNI ISO/data-track extraction now uses a gameplay-oriented extension allowlist instead of extracting the full installer tree; the launcher-side extension metadata was updated to document the same disc-extract policy
- Done: `AdvancedSettingsPage.kt` now scans app/import storage on `Dispatchers.IO`, shows a loading state, uses stable list keys, and renders file rows incrementally via `Show More` instead of dumping the full recursive tree into one dialog pass
- Done: `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` now cache whether `textures/d1` or `textures/d2` exists and skip those dead replacement-directory probes when the roots are absent, reducing miss overhead on stock installs
- Done: `android/app/src/main/cpp/shared/pngfile_stb.c` now builds one in-memory PhysFS index of `*.ktx2`, `*.png`, `*.jpg`, and `*.tga` files after each cache clear and resolves candidate texture paths against that index before calling `PHYSFS_openRead()`. That turns most replacement lookups into hash probes plus at most one file open for a real hit, instead of repeated expensive PhysFS misses across every variant/extension pair
- Done: `SetupActivity.kt` now keeps controller focus on the remaining unfinished disc-import action and only falls back to `Done` once no other action remains; the dialog also requests keyboard input mode before moving focus so controller highlight follows the new target reliably
- Done: `AdvancedSettingsPage.kt` now shows cue/bin-count summaries for SAF-linked CD sources, detects 1-byte case-variant helper symlink stubs by pairing them with their larger case-insensitive companion file, labels them in both the file list and details view, and keeps them adjacent in name sort order
- Validation: `:app:compileDebugKotlin`, `:app:externalNativeBuildDebug`, and the scoped `android/run-code-quality.ps1 -Fix` pass all succeeded after the edits
- Validation gap: the direct Infinite Abyss emulator smoke remains blocked by a broader `android/tests/test_extract.ps1` app-private staging issue for setup-command source files. The helper was partially hardened for quoted paths, but the runtime copy path still needs a dedicated fix before that test can cover multitrack direct imports reliably