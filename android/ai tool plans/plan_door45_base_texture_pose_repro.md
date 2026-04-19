# Plan: D2 L1 door45 Base-Texture Corruption Repro

## Goal
Use the new phone log to determine whether the door45 corruption is already diagnosable, and if not, create a repeatable emulator repro anchored to the exact logged camera pose.

## Findings From debuglog_20260418_170054.txt
- [x] The log contains an exact automation-ready pose at `[mwall_snap_pose]`: `segment=80 x=-107.764038 y=-72.492294 z=147.704697 pitch=661 bank=488 heading=15072`
- [x] The early texture-log frames show the suspect cover door behind a transparent merged wall: `cover_bot=door45#9` on `cover_seg=80 cover_side=1 cover_face=0`, with `wall_type=2 wall_state=2 wall_clip=39`
- [x] The request-time snapshot does not isolate the bad face. By frame 258/259 the tracker only has seg80 side0 and side3, and the front face logs `projected=2 bbox_valid=0`, so snapshot selection falls through to `no_projected_faces`
- [x] The log does not stamp the current level on the snapshot request line, so the D2 level 1 context is external knowledge unless we add it to the logging
- [x] A clean emulator rerun now resolves the script path correctly as `/data/user/0/com.dxxredux.app/files/test_door45_pose_repro.json5`; the earlier `game_scriptstest_...` path was from the bad invocation, not the new SetupActivity flow
- [x] The currently installed debug APK still logs `Unknown action: pose_view`, so the new script is blocked on a fresh `assembleDebug` build rather than launcher path handling
- [x] After a fresh `assembleDebug` build and reinstall, `pose_view` executes and snaps the phone pose to a nearby valid in-mine pose: `segment=80 x=-107.827438 y=-73.607056 z=147.644165 pitch=660 bank=487 heading=15072`
- [x] The rebuilt emulator run still lands on `mwall_snapshot stage=frame ... tracked=2 center_hits=0 cover_events=2` followed by `no_projected_faces`, so the original log behavior reproduces closely enough to justify more snapshot instrumentation
- [x] The final tightened emulator repro now passes with stable snapped-pose assertions and still ends in `merged_wall_snapshot.status = no_projected_faces`, with introspection at `segment=80 x=-107.840179 y=-73.832001 z=147.631989`
- [x] `set_debug texture_log=1` only enables the native-side gate. To actually persist the new merged-wall diagnostics into `files/debuglogs/debuglog_*.txt`, the launcher-side `dlog_texture_enabled` preference must also be set before launch
- [x] After rebuilding the native code and rerunning with launcher-side texture logging enabled, the exported debug log now includes level-aware snapshot lines, partial-face fallback diagnostics, richer `mwall_tex` texture metadata, and snapshot-frame `mwall_cache` reuse entries for the `merge_cached` path
- [x] The current emulator pose still centers the nearby `rock346 + ceil025` case rather than the original `door45` cover, but the new logging is now live and ready for comparison against a fresh phone capture of the real bad view

## Work Items
- [x] Add a D2 Counterstrike level 1 automation script that uses `pose_view` with the logged pose
- [x] Run the pose script once on the emulator and capture the current blocker: stale APK rejects `pose_view`, and the script now asserts the target segment and coordinates so stale builds fail instead of silently passing
- [x] Build a fresh debug APK with `assembleDebug`, reinstall it, and rerun the pose script against the rebuilt native library
- [x] Confirm that the rebuilt APK accepts `pose_view`; widen the script's position ranges to account for the snapped in-mine pose so the repro remains stable across rebuilds
- [x] If the pose still yields `no_projected_faces`, extend merged-wall snapshot logging to report level info and a partial-face fallback when `projected > 0` but `bbox_valid == 0`
- [x] Add texture-detail logging for the selected cover door face so the logs include texture handle, size, upload flags, and cached-premerge handle metadata for the corrupt door path
- [x] Re-run the pose repro with launcher-side texture log prefs enabled and confirm the new debuglog markers are written to the exported debug log file

## Validation Target
- The new script should reliably launch D2 level 1 and fail unless it lands at segment 80 near the snapped replay coordinates without manual steering
- The next diagnostic pass should make the level context explicit on the snapshot request path and emit the new partial/cover texture diagnostics into `files/debuglogs/debuglog_*.txt`