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

## Audio and complete in-engine briefings

1. Import the locally owned, hash-verified D2 robot movie library and recapture
   the real briefing; retain evidence of animated robot rendering
2. Record the engine's mixed PCM output using a bounded diagnostic tap after
   mixing, without replacing SDL_mixer's movie or music callbacks
3. Synchronize menu, briefing and replay audio with the captured video and
   preserve pitch when shortening footage. Record fly-out sound in a separate
   normal-speed engine pass so the slow visual capture does not stretch music
4. Regenerate into an ignored audio review folder, preserving previous output
5. Build both engines, run asset integration and scoped formatting, verify
   audio presence and motion cadence, and document reproducible commands

### Audio implementation and capture evidence

- Output: `android/temp/store-assets_20261004_audio`, ignored by git
- Briefings were always captured in-engine. The old input set omitted
  `ROBOTS-H.MVL`; the verified retail library now renders animated robots
- Retail D2 deliberately stops music for its built-in campaign briefing. The
  capture-only `music_control` briefing operation asks the existing song API to
  play `briefing.hmp`; all three captured pages report that song active
- PCM capture uses a bounded buffer after SDL mixing, preserving MIDI, effects
  and movie audio without replacing the movie player's post-mix callback
- Source WAVs carry native monotonic timestamps and engine music state. The
  editor preserves pitch, encodes one continuous AAC stream, and checks the
  decoded final audio in each menu, briefing, gameplay and fly-out section
- Added display override reset and encoder startup/output checks after a stale
  emulator configuration produced a wrong-sized still and an empty recording
- Both native engines built successfully; scoped formatting and the asset
  integration test passed. All three replay results match their recordings
- Final video: 900 frames, 30.000 seconds, 2400x1350, stereo AAC; zero decoded
  clipped samples in the checked sections after balancing excerpt loudness
- Visible updates/sec: 24.97 for all three gameplay excerpts and 30.00 for the
  moving fly-out. All three briefing pages pass the robot animation and MIDI
  checks; paired briefing frames are included in the gallery

## Capture warp cleanup

1. Clear the old window backing once inside `from_exit_tunnel`, immediately
   after repositioning the player. Reuse the existing clear helper; add no
   work to normal gameplay or the per-frame renderer
2. Build both engines and recapture the fly-out and normal-speed audio into
   `android/temp/store-assets_20261004_flyout`, preserving the audio review
3. Inspect early tunnel frames for the old room/crosshair, rebuild the edit,
   and rerun the existing media integration and motion/audio checks

### Capture warp results

- Reused the existing backing-clear helper only in the capture warp action;
  logs confirm one clear in each of the two capture passes
- Trimmed the fly-out to begin no earlier than its first active observation,
  excluding recorder startup and the pre-warp view
- Inspected final frames at 24.0-25.0 seconds: the starting-room image is gone;
  the live first-person reticle remains until the normal camera transition
- Both engines built; scoped formatting and the media integration test passed
- Final output remains 900 frames / 30 seconds with audio, no decoded clipping,
  and 30 distinct updates/sec in the moving fly-out section

## Preview timing, MIDI tempo and authentic exit follow-up

1. Clarify which opening gameplay shot to remove; give its duration to the
   launcher/file-picker shots while preserving the 30-second total
2. Capture a sample-aligned native MIDI stem before effects mixing, alongside
   the existing mixed output. Retiming applies to effects only; engine music
   stays at its original tempo in every edited section
3. Correct the debug exit fixture's stale last_pos and locate the actual exit
   trigger instead of starting at an arbitrary tunnel depth. Clear backing
   only while that debug fixture is active; no production renderer changes
4. Regenerate opening/replay audio and exit video into a new ignored review
   folder, inspect the entire tunnel and validate music tempo, motion and audio
5. Build both engines, run scoped formatting and the store asset integration

### Follow-up findings and implementation

- User confirmed removal of the one-second shot after the briefing. Launcher,
  picker and scrolling now occupy 4.6 seconds instead of 3.6; setup stays at
  nine seconds with all three five-second action clips retained
- The old pitch-preserving atempo edit accelerated D1 MIDI by about 17-21%.
  A native pre-effects stem now allows SFX retiming with unchanged MIDI tempo;
  rendered music samples are compared exactly against the native source
- The arbitrary three-segment exit placement exposed the external opening
  early. The real level-1 exit trigger is wall 6, from segment 69 into 106,
  with a 12-segment route to exterior segment 284. The fixture now uses that
  authored trigger and initializes last_pos to a consistent approach
- Captured phases 1, 2, 3 and 4 through completion. Inspected the full native
  sequence at five frames per simulated second: the approach and look-back
  show mine geometry without the previous central stale patch
- Fresh backing is scoped to the debug fixture. No production renderer or
  normal gameplay clear behavior was changed
- Rebuilt both engines, recaptured all three replay videos and their native
  music stems, opening/menu/briefing audio and both exit passes. Existing
  screenshot candidates remain available in the new ignored review folder

### Final follow-up validation

- Review: `android/temp/store-assets_20261004_tempo/index.html`; regenerated
  `store-preview-30s.mp4` is 900 frames, 30.000 seconds, 2400x1350 with AAC
- Both engine builds, scoped code quality and the store asset integration pass
- All three replay results match; original 31 screenshot candidates and three
  launcher screenshots pass validation; all briefing robot animation checks pass
- All edited native music excerpts match their original PCM samples exactly
  at tempo 1.0; decoded final audio has no clipped samples in checked sections
- Visible updates/sec: 24.97 for all action clips and 30.00 for the moving
  fly-out, with no held frames longer than 67 ms in the checked sections
- Inspected full native exit and final-video frames, retained evidence in
  `exit-review/` and refreshed `flyout-inspection/`. Capture emulator returned
  to launcher; generated assets remain ignored by git

## Stable eight-image listing shortlist

1. Review existing action candidates and capture D1 level 7's final boss fight
2. Check in explicit launcher/demo-time/fly-out selections with descriptive
   filenames; retain source timing, hashes and replay status in a manifest
3. Add shortlist generation to the full asset workflow and an offline rerun
   command. Keep exactly eight PNGs in the review directory, with the gallery
   and source manifest alongside it
4. Inspect all eight images, check dimensions/source integrity, rerun the
   selection for stability and run scoped quality plus media integration

### Shortlist results

- Delivered `android/temp/store-assets_20261004_tempo/selected-stills/` with
  exactly eight numbered PNGs and an overview in `selected-stills-contact.jpg`
- Fixed choices in `android/store-stills.json`: launcher top/bottom; D1 level 5
  at 30.5 and 89.5 seconds; D1 level 18 at 26.0; D2 level 9 at 49.0; D1 level 7
  at 101.8 (visible, uncloaked boss); first frame of the corrected fly-out clip
- Full capture includes the supplementary boss source; compose exports the
  shortlist automatically. `-SelectStillsOnly` rebuilds it from existing media
- Fresh boss replay retains its known endlevel_completed-only mismatch. This
  is reported in the selection manifest and separate validation status; other
  final fields match. Primary video replay assertions remain strict
- Inspected all eight images; verified dimensions, source demo identity and
  exactly eight output files. Re-export produced identical SHA-256 hashes for
  every PNG. Scoped formatting and the extended media integration both passed

## Filtered video regeneration

1. Enable native trilinear texture filtering, 4x MSAA and 16x anisotropic
   filtering; select an emulator backend that exposes real AF support
2. Apply and verify this presentation profile for each engine launch, preserving
   replay simulation settings, native MIDI tempo and the approved edit timing
3. Recapture opening, action and exit sources in a new ignored review folder;
   regenerate the stable shortlist from the new recordings
4. Check effective GPU settings, replay results, motion/audio and final images;
   run scoped formatting and the existing asset pipeline integration

### Filtered capture findings

- Host GLES translation reports AF unavailable; guest ANGLE over the NVIDIA
  Vulkan driver exposes 16x AF and working 4x MSAA at 2400x1080
- Added explicit per-launch graphics configuration, real safety confirmation
  and native state validation. The runner selects guest ANGLE automatically
- Filtered replay rendering is slower than real time. Capture timeout now
  permits 12x duration plus setup margin; the integration allowance is two
  hours. Recorded simulation cadence and unchanged native MIDI remain intact
- The first D1 action clip measured 24.97 visible updates/sec and a 67 ms
  maximum hold after export, matching the demo's original 25 Hz cadence
- ANGLE's host fly-out recording failed the motion check despite correct
  native graphics settings. Switched this capture to guest recording and a
  16x debug capture clock divisor, without changing production rendering
- Guest capture retained 527 frames for the chosen fly-out interval, but its
  presentation timestamps also arrived in bursts. Evenly pacing those real
  frames over six seconds restores 30 distinct updates/sec without synthetic
  frames. The recipe records and validates the available native frame count
- A part restart omitted an unused 1.24-second tail of level 18. Its final
  simulation result matches and every selected clip/ten-second screenshot is
  present. Validation now checks requested intervals explicitly, and the
  recorder avoids a restart when the remaining replay fits in its current part

### Filtered delivery and validation

- Delivered `android/temp/store-assets_20261004_filtered/index.html` and
  `store-preview-30s.mp4`, with refreshed candidates and the eight-image shortlist
- Final video: 2400x1350, 900 frames, exactly 30 seconds with native engine audio
- All three primary replay results match. Supplementary boss capture retains
  only the previously reported endlevel_completed mismatch
- Action clips: 24.77-24.97 visible updates/sec, longest hold 100 ms. Moving
  fly-out: 30 visible updates/sec, no repeated frames, longest hold 33 ms
- MIDI remains at tempo 1.0 with exact native sample comparison; final decoded
  audio has no clipped samples in checked sections
- Moved graphics setup before the pilot capture landmark and recaptured the
  opening, preserving its nine-second edit while showing pilot creation clearly
- Inspected final opening, action stills and the entire edited fly-out. Scoped
  formatting, test-catalog integration and the complete media integration pass
- Generated media remains ignored by git; capture emulator returned to launcher

### Featured boss image

- Add an additional stable recipe at the same D1 level 7 boss moment, 101.8s
- Use native full-screen cockpit mode, immersion/no-HUD mode and the left rear
  camera, configured through debug automation; retain touch controls
- Expose native HUD/camera settings through introspection to verify the capture
- Keep trilinear filtering, 4x MSAA and 16x AF; preserve the eight-image shortlist
- Add isolated featured regeneration and include it in full generation, export
  a separate featured PNG and provenance, and show it in the review gallery
- Build both engines, capture and inspect the new image, and run media validation
- Native D1 lacks auxiliary camera windows. Use the existing D1-in-D2 launch
  and checkpoint translator for this additional capture; compare and report the
  translated replay. Wire the Android D1-in-D2 replay startup to the translator
  flag already used by the host replay runner (Android intentionally ignores ini)
- D2 replay blocks live keyboard controls. Add validated debug setters for native
  cockpit/HUD modes and rear camera selection, with no renderer changes

### Featured image delivery

- Delivered `android/temp/store-assets_20261004_filtered/featured/featured-d1-level7-boss.png`
  at 2400x1080, with the boss visible and native left rear camera; gallery updated
- Full-screen mode 3, native No HUD mode 3, camera views [2, 0], trilinear filtering,
  4x MSAA and 16x AF verified at startup and near the selected 101.8-second frame
- Native No HUD mode retains corner indicators, weapon text, reticle and touch
  controls; the cockpit art, gauges and optional count/boss HUD helpers are hidden
- Both Android engines built successfully; scoped quality and automation catalog
  checks passed, followed by the complete store media integration validation
- Imported replay differs only in its expected D2 game/mission identities and
  the previously observed endlevel_completed flag; raw differences remain in
  `featured.json`. Existing eight-image shortlist remains separate
- `-FeaturedOnly` regenerates this capture; full generation includes it, and
  `-SelectStillsOnly` re-exports the same selection from its existing recording

### Repair filtered video audio and motion

- Treat the user's perceptual regression report as a failed capture despite the
  earlier sample-equality and pixel-change checks; compare tempo/filtered sources
- Preserve existing review videos and identify the accepted audio and fly-out
- Diagnose audio timing/mix subtraction and measure geometric motion through the
  entire fly-out, including the transition outside, rather than only pixel changes
- Combine the filtered picture with proven audio; repair or recapture filtered
  fly-out motion and retain an explicit reproducible source recipe with hashes
- Deliver a new ignored review video, validate it and document remaining limits

### Combined video delivery

- Delivered `android/temp/store-assets_20261004_combined/store-preview-30s.mp4`
  with a review gallery and self-contained, hash-pinned combination recipe
- First 24 seconds have identical decoded picture frames to the filtered version
  and the complete decoded soundtrack exactly matches the accepted tempo version
- Found that amix takes absolute weights: the old negative mix weight added the
  music stem instead of subtracting it before time compression. Replaced it with
  explicit signal inversion and verified identical non-silent stems cancel to zero
- Recaptured the native filtered fly-out with a debug-only 60 Hz simulation clock
  per rendered frame, then paced its native frames into six seconds at 30 fps
- Geometric tunnel-wall motion checks accept the tempo and new captures and reject
  the filtered regression. New p90/median movement ratio is 1.31 versus 16.04 in
  the failed capture; no initial tunnel stalls. Inspected final fly-out contact sheet
- Both Android engines build, scoped mixed-language quality checks pass, and the
  actual PowerShell combination command and media integration test pass
- Documented full regeneration with the accepted soundtrack and offline rebuilding
  from archived inputs. Existing soundtrack imperfections remain unchanged;
  extreme action compression can still impair newly retimed sound effects
- No production renderer changes; previous review assets remain intact

### Repair featured image quality and boss health

- Replace video-frame extraction with lossless Android PNG capture. Use native
  replay pause and single-frame controls to reach the stable 101.8-second landmark
- Enable native boss health in the featured presentation and require drawn-state
  evidence at the captured frame; retain full-screen mode and the left rear camera
- Capture into a separate source collection, replace the requested featured PNG,
  and verify exact source/export equality, frame timing, graphics and presentation
- Run scoped quality checks and the featured media integration validation

### Featured repair delivery

- Replaced the requested featured PNG in the filtered review directory with an
  unchanged 2400x1080 Android screenshot; old export backed up under
  `featured-source/previous-featured.png`
- Captured native replay frame 2542 at 101.809005737 seconds, 9 ms after the
  recipe landmark. Boss health is enabled, active and drawn; full-screen mode 3,
  HUD mode 3, left rear camera and filtered graphics all verified
- The initial one-second pause margin was insufficient for automation transport;
  its timing guard rejected that attempt. The successful generator pauses five
  seconds early and batches native single-frame advances to the exact frame
- Inspected the new PNG. Source/export bytes match exactly, with no video
  compression, resizing or compositing; the visible boss bar is native
- Scoped mixed-language quality and the existing media integration runner's new
  `-FeaturedOnly` mode pass. Replay differences remain the previously documented
  game/mission identities and endlevel_completed; no new replay differences
- Generator updated for repeat runs, gallery refreshed, and capture emulator
  returned to the launcher. No native code or production renderer changes

### Match the preferred D1 boss moment

- Restore native D1 rendering, remove auxiliary cameras, and retain full-screen
  immersion mode with native boss health and robot/hostage/secret progress lines
- Preserve the D1-in-D2 rear-camera recipe as a selectable alternate
- The original still used interpolated video timing. Compare nearby native frames
  against the supplied reference and pin the actual outgoing-laser frame
- Add D1 replay pause/single-frame controls matching D2's existing controls so
  native D1 can produce exact lossless captures; no renderer changes
- Build, regenerate, inspect, and validate the featured image and stable recipe

- Native D1 calibration identified frame 2549 (102.089004517 seconds) as the
  original selected image: scene-region RGB error 5.55 versus 38.05 for the next
  closest candidate. Inspected the cockpit match and full-screen laser framing
- Native D1 immersion mode suppresses all HUD rows regardless of helper settings.
  Keep normal HUD behavior unchanged: use D1-in-D2 for the final presentation,
  with both auxiliary windows off and all three progress rows explicitly enabled
- D1 pause/frame stepping successfully captured 20 adjacent native frames and
  completed the replay with only the existing endlevel_completed difference
- Retain the earlier rear-camera configuration under `featured_alternates`; the
  primary recipe now pins frame 2549, not the original interpolated video time

### Matched featured image delivery

- Replaced `featured/featured-d1-level7-boss.png` in the filtered review folder
  with a lossless 2400x1080 capture at frame 2549. Visually confirmed the large
  outgoing lasers, boss, native boss bar and all three upper-right progress rows
- Native evidence confirms full-screen/immersion modes 3/3, cameras [0, 0],
  progress rows drawn, boss bar drawn, and trilinear/4x MSAA/16x AF
- Both engines build successfully without warnings; scoped quality checks and
  the featured media integration pass. Reference shortlist PNG hash is unchanged
- Export hit a transient Windows file-write error; rerunning the offline export
  succeeded, with source/export byte equality verified. Gallery refreshed and
  emulator returned to the launcher
- D1-in-D2 replay differences remain the expected game/mission identities and
  existing endlevel_completed flag. Earlier rear-camera image and recipe retained
- Native D1 pause/frame stepping is available for future exact-frame calibration;
  normal HUD and renderer behavior remain unchanged
