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
- [x] The centered phone tap in `debuglog_20260418_201431.txt` still snapshots only the partial `seg=80 side=0` merged face (`rock346 + ceil025`) with `center_hits=0`, while the earlier approach frames show `door45#9` only as a later `mwall_coverbox` event attached to the full `seg=82 side=0` face
- [x] Root cause of the missing centered-tap door diagnostics: `mwall_coverbox` previously ignored projected-only tracked faces, so partial faces could never record later coverbox overlaps and `mwall_snapshot_partial_cover` could only report the exact self-cover
- [x] After patching coverbox overlap matching to fall back to projected bboxes, the rebuilt emulator repro writes `face_box=projected` coverbox logs for door covers in the latest exported debug log, confirming the new path is live for the next phone capture
- [x] With the correct bare `run_test.ps1 -ScriptName test_door45_pose_repro.json5` invocation, the latest emulator validation now emits `mwall_coverbox ... cover_bot=door45#0 ... face_box=projected`, `mwall_snapshot_partial_cover kind=bbox`, and `mwall_tex tag=snapshot_coverbox_cover ... name=door45#0`, so the centered repro now captures the door cover's texture metadata
- [x] The new phone log `debuglog_20260418_204724.txt` from build `11551 (a604d72)` now shows the same centered-tap door path on-device: `mwall_coverbox ... cover_bot=door45#0 ... face_box=projected`, `mwall_snapshot_partial_cover kind=bbox`, `mwall_tex tag=snapshot_coverbox_cover ... name=door45#0`, and `mwall_tex tag=snapshot_cover ... tex=780 name=door45#0`
- [x] The captured phone metadata for `door45#0` looks ordinary rather than obviously corrupt: `64x64`, `bytes=8192`, `mip=1`, `wrap=10497`, `internal=0x1907`, `format=0x1907`, `tex_flags=0x0`, `is_png=0`, `mask_handle=0`, while the centered merged face remains the expected partial `rock346 + ceil025` case
- [x] The new renderer-side logging is now live in the emulator validation log `debuglog_20260418_213145.txt`: `mwall_cover_live` captures the single-cover GL state for `door45#0`, and `mwall_cover_src` plus `mwall_cover_src_row` dump the raw 64x64 source bytes with hash `0x272e5021`
- [x] The validated centered snapshot currently reports `mwall_cover_live kind=bbox shader=single ... program=3 tex0=69 tex1=67 tex2=0 tex_min=0x2600 tex_mag=0x2600 wrap_s=0x2901 wrap_t=0x2901 depth=1 blend=1 cull=1 poly=0 viewport=0,120,640,240 overlap=280.5`, which is the emulator baseline to compare against the next phone log
- [x] The newest phone log `debuglog_20260418_212930.txt` confirms the same centered `door45#0` capture on the actual device, with matching source hash `0x272e5021` and live cover state showing the correct texture bound on `tex0`
- [x] The phone draw differs from the prior emulator baseline mainly in filtering state: `tex_min=0x2701` / `tex_mag=0x2601`, which means the texture object is configured for mipmapped linear minification even though that alone does not prove the sampler chose a lower mip level for this pixel
- [x] The phone snapshot geometry is still heavily minified in one axis despite the camera being close: the centered `door45#0` bbox spans roughly `599x1.6` screen pixels, so mip selection remains plausible for an edge-on door unless deeper logging proves the draw is clamped to level 0

## Work Items
- [x] Add a D2 Counterstrike level 1 automation script that uses `pose_view` with the logged pose
- [x] Run the pose script once on the emulator and capture the current blocker: stale APK rejects `pose_view`, and the script now asserts the target segment and coordinates so stale builds fail instead of silently passing
- [x] Build a fresh debug APK with `assembleDebug`, reinstall it, and rerun the pose script against the rebuilt native library
- [x] Confirm that the rebuilt APK accepts `pose_view`; widen the script's position ranges to account for the snapped in-mine pose so the repro remains stable across rebuilds
- [x] If the pose still yields `no_projected_faces`, extend merged-wall snapshot logging to report level info and a partial-face fallback when `projected > 0` but `bbox_valid == 0`
- [x] Add texture-detail logging for the selected cover door face so the logs include texture handle, size, upload flags, and cached-premerge handle metadata for the corrupt door path
- [x] Re-run the pose repro with launcher-side texture log prefs enabled and confirm the new debuglog markers are written to the exported debug log file
- [x] Patch coverbox overlap logging so projected-only tracked faces can emit `mwall_coverbox` events and flow into `mwall_snapshot_partial_cover`
- [x] Rebuild the Android debug APK and rerun the door45 repro to confirm the new `face_box=projected` marker appears in exported logs for door covers
- [x] Add cover texture-detail logging for snapshot-time coverbox events and validate that the centered repro logs `snapshot_coverbox_cover` for `door45#0`
- [x] Capture a fresh phone debug log with the projected-coverbox patch and compare whether the centered tap now yields `mwall_snapshot_partial_cover` / `mwall_tex` lines for the actual `door45` cover
- [x] Add deeper phone-side cover draw-state logging so the centered snapshot path records the live single-cover GL state and the raw `door45#0` bitmap bytes alongside the existing texture metadata
- [x] Rebuild the Android debug APK and rerun the canonical bare `run_test.ps1 -ScriptName test_door45_pose_repro.json5 -Game d2` validation to confirm the new `mwall_cover_live`, `mwall_cover_src`, and `mwall_cover_src_row` markers appear in the exported debug log
- [x] Add focused mip diagnostics for `door45#0` / `door45#9` upload paths so the logs distinguish stock `glGenerateMipmap`, prebuilt KTX2 mip uploads, and any anisotropy-driven filter upgrade from the user's explicit TexFilt setting
- [x] Extend the live cover logger with the active graphics settings and an approximate screen-space LOD estimate from the actual door UV span and projected bbox so the next phone/emulator comparison can tell whether mip selection is expected or unlikely
- [ ] Rebuild and rerun the canonical door45 repro to confirm the new `mwall_mip_upload` / `mwall_cover_lod` markers appear in the exported debug log

## Validation Target
- The new script should reliably launch D2 level 1 and fail unless it lands at segment 80 near the snapped replay coordinates without manual steering
- The next diagnostic pass should make the level context explicit on the snapshot request path and emit the new partial/cover texture diagnostics into `files/debuglogs/debuglog_*.txt`
- The next phone-side comparison should check whether `mwall_cover_live` and `mwall_cover_src` still match the emulator baseline, since the centered bad-face tap now already produces projected `mwall_coverbox`, `mwall_snapshot_partial_cover`, full `door45#0` texture-detail lines, and the raw 64x64 source dump