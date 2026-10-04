# Repeatable Play Store assets

## Goal

Generate real Android launcher and engine captures from the current build into
ignored `android/temp/store-assets_*` directories. Keep the generator, capture
recipe and documentation in source control; keep game data and media out of git.

## Implementation plan

1. Inspect existing launcher automation, demo playback and local prerequisites
2. Provision an isolated emulator with a >= 2K landscape framebuffer and fresh
   launcher/pilot defaults, using existing local D1/D2 game data
3. Seed actual saves through the engine, capture launcher top and bottom, and
   record the launcher/data selection/D2 pilot/campaign/briefing/level flow
4. Replay existing D1/D2 recordings with current default HUD and touch overlay;
   capture PNG candidates every ten seconds and retain source recordings
5. Compose a 30-second H.264 review video: opening flow <= 10 seconds, three
   five-second late-demo clips, then D1 fly-out ending exactly at 30 seconds
6. Write a machine-readable capture manifest, contact sheets and review index
7. Run the generator end to end, inspect captures and video timing, run relevant
   build/integration checks and scoped formatting, then document rerun commands

## Capture decisions and acceptance

- Use a dedicated emulator, not the connected personal phone
- Drive semantic launcher commands and engine automation/introspection; avoid
  timing-only menu navigation and fixed-coordinate taps where possible
- Preserve the input-demo simulation settings; change presentation only where
  needed to show current defaults, and report replay failures honestly
- Capture the real touch overlay, never paste a synthetic overlay onto frames
- Validate output resolution, decoded duration, source clip ranges and populated
  launcher save controls; inspect representative images and ending frames
- Preserve failed capture diagnostics and use the existing retention helper
- Publication/upload is outside this task; all outputs are local review assets

## Progress

- Implemented the isolated emulator runner, actual file-picker flow, seeded
  saves, ten-second demo capture and offline editor/gallery
- Added shared debug introspection for replay progress and a geometry-derived
  exit-tunnel capture option; production game behavior is unchanged
- Completed the clean integrated run in ignored
  `android/temp/store-assets_20261004_review`
- D1 levels 5 and 18 and D2 level 9 all match their recorded final results
- Produced three 1080x2400 launcher images and 31 2400x1080 gameplay candidates
- Produced a silent 2400x1350 video with exactly 900 frames / 30 seconds:
  nine seconds of opening flow, three five-second demo clips and six seconds
  of D1 fly-out; inspected the final frame to exclude the score-screen transition
- Passed the Android x86_64 build for both engines, scoped code-quality checks,
  both automation catalog tests and `test_store_asset_pipeline.ps1` against the
  completed run
- Preserved the exploratory D1 level 7 mismatch diagnostics and documented why
  that recording is excluded from the default selection
- Rerun and offline editing instructions are in `android/store-assets.md`

## Review revision: action selection and frame pacing

1. Discard the low-action D2 exit excerpt and choose an active replacement
2. Measure source-frame timestamps and repeated frames, not just encoded FPS
3. Test lower-overhead emulator capture and recapture the D1 fly-out; target
   30 real frames per second, accepting 25 only if capture throughput requires it
4. Check the other gameplay excerpts with the same motion/pacing measurements
5. Regenerate the review video, preserve the original and document the capture
   settings and measured results in the reusable workflow

### Revision results

- Revised gallery/video: `android/temp/store-assets_20261004_revision`; original
  review assets remain intact
- Replaced D2's stalled exit approach with combat at simulation seconds 48-53
- Profiled the emulator: fly-out rendering sometimes exceeded 100 ms/frame;
  lowering resolution and changing recorders alone did not resolve this
- Added a guarded single-player exit capture clock at one-eighth speed, recorded
  with the emulator host recorder, then restored normal speed in the edit
- Restored gameplay's recorded 25 Hz cadence from the original VFR captures;
  a host-recorder gameplay experiment was discarded after timing analysis
- Final visible updates/sec: D2 24.77, D1 level 18 24.97, D1 level 5 24.97,
  fly-out moving section 30.00 (previously 9.66); maximum held frame 100 ms
- Verified host capture at its physical 1080x2400 framebuffer size with a
  counterclockwise rotation in the edit, preserving landscape proportions
- Added content-based motion validation so a nominal 30 fps container no longer
  suffices; no motion interpolation is used
- Both Android engines built successfully; updated APK's D2 replay result
  matched its recording; the revised asset integration test and formatting pass
