# Regenerating store screenshots and video

Run from the repository root with PowerShell 7:

```powershell
.\android\generate-store-assets.ps1
```

This builds the current debug Android app, provisions the dedicated
`DxxStoreAssets` emulator on port 5580, resets only that emulator's app sandbox,
and writes a new ignored `android/temp/store-assets_YYYYMMDD_HHMMSS` folder.
It never selects a physical phone. The emulator is left running for inspection.
The retention helper keeps three previous generations before creating a new one.

Prerequisites: the repository's Android/JDK dependencies, PowerShell 7, Python
3.10 or newer, hardware emulator acceleration, the API 34 Google APIs x86_64
system image, and locally indexed full D1/D2 game data. The runner uses
`dependency_base.txt` and verifies data against `game_data/game_data_index.txt`.
No game data is downloaded or checked into git. A private Python environment
under `android/temp/store-assets-tools/venv` installs pinned `imageio-ffmpeg`
0.6.0 and Pillow 11.3.0; FFmpeg is supplied by the pinned wheel.

## Outputs

- `index.html`: local review gallery with clickable full-resolution PNGs and video
- `selected-stills/`: exactly eight numbered PNGs: launcher top/bottom, five
  action stills including the level-7 boss, and the corrected D1 fly-out
- `selected-stills-contact.jpg`, `selected-stills.json`: shortlist overview,
  selection reasons, source times/hashes and replay status
- `launcher/01-top.png`, `02-bottom.png`: 1080x2400 portrait launcher screenshots
- `launcher/03-save-explorer.png`: bonus screenshot with real D1/D2 save entries
- `demos/<recording>/*.png`: 2400x1080 gameplay candidates at the beginning and
  every ten seconds of simulation time, including the actual Android touch UI
- `demos/<recording>/contact-sheet.jpg`: compact candidate overview
- `store-preview-30s.mp4`: 2400x1350, 30 fps, H.264, exactly 900 frames
- `video-contact-sheet.jpg`: one thumbnail per second of the final edit
- `motion-validation.json`: measured visible frame updates and held frames for
  all gameplay excerpts and the moving portion of the fly-out
- `raw/`: original screen recordings for later edits
- `edit.json`: editable clip ranges and output durations
- `build.json`, `game-data.json`, per-demo `capture.json`, and `validation.json`:
  source identity, capture timing, replay results and verification evidence

The default review pass captures D1 levels 5 and 18 and the longer D2 level 9
recording, plus the short D1 level-7 boss recording under `still-sources/` for
the screenshot shortlist. Use `-AllDemos` to sample the full checked-in corpus. The two-second
D2 recording can produce a beginning shot but cannot supply a five-second clip.
All-corpus capture takes substantially longer, particularly the eleven-minute
D1 level 7 recording. Screen recordings are split before Android's three-minute
limit; the editor rejects a clip that crosses a recording restart.

The initial survey found an `endlevel_completed` result mismatch in
`d1_descent_level7_20260921_181652.dximdemo`. It supplies a boss still, with this
completion-flag difference explicitly recorded in `selected-stills.json` and
reported separately in validation. Its other final result fields match. It is
not used for the preview video's action clips, which retain strict final-state
comparison. Full-corpus validation still rejects mismatched primary recordings;
no simulation settings or recorded expectations are changed for the shortlist.

The curated choices live in `android/store-stills.json`. Demo selections use
fixed simulation times mapped through each new capture's timing samples, so
emulator speed changes do not move them to another point in the replay. The
fly-out selection is relative to its edited clip's start. Full generation and
video recomposition rebuild the shortlist automatically. PNGs retain their
captured HUD/touch controls and resolution, with no color enhancement or crops.

## Presentation and video timing

The runner creates actual pilots and saves through engine menus, applies the
launcher's **Restore Defaults** preset, and keeps the default touch layout.
Input replay deliberately bypasses pilot selection and restores its original
simulation settings. The runner enables the visual hostage/robot/secret
counters, boss bar and capture graphics profile on replay startup; it does not alter recorded inputs,
random-number state, physics or Guide-Bot routing to obtain a passing replay.
Primary video replay state must match the recording for validation to pass.

The opening uses the real Android file picker to import a ZIP made from the
verified local game data. It then scrolls the launcher, launches D2, creates a
pilot, selects Counterstrike, shows three briefing pages and enters level 1.
The edit removes loading waits and compresses this sequence into nine seconds.
It omits the one-second level-entry shot and gives that time to the launcher
and file picker, allowing their interactions to remain on screen longer.
The two D1 excerpts start at 90 percent of their demo durations. The D2 excerpt
uses seconds 48-53, a review-selected combat sequence; its late exit approach
was discarded for low action.
The final six seconds show D1's real exit animation, ending at the video's
30-second boundary before the score screen. A debug-only automation fixture
locates the authored exit trigger in the engine and starts the existing fly-out
with a consistent current/previous ship position and outward direction.
It advances only that single-player sequence by a fixed 1/60 second per rendered frame during capture,
and refreshes the backing while that debug fixture is active so exposed exit
pixels cannot retain previous frames. Production gameplay rendering is
unchanged. After capture, the editor restores normal speed. This lets a slow emulator render enough
real frames for 30 fps without synthesized motion frames. The clock hook is
debug-only, resets after the exit, and is disabled during replays/multiplayer.

The fly-out uses the guest recorder and evenly spaces its captured engine frames
over the final six seconds. ANGLE presentation timestamps can arrive in bursts;
using their original timing would introduce holds despite sufficient real frames.
The recipe records the native frame count and requires at least 180 frames.
Gameplay retains the
guest recorder's variable-frame-rate stream, then restores the input demo's
recorded frame cadence instead of the emulator's uneven wall-clock timestamps.
The input demos contain approximately 25 simulation updates per second; a
30 fps output therefore repeats some gameplay frames. No motion interpolation
is used. The motion report checks visible frame changes and held frames in
addition to nominal FPS. Validation requires at least 23 visible updates/sec in
each gameplay clip, 27 in the fly-out's moving first two seconds, and no holds
longer than 140 ms. Its camera deliberately settles later in the sequence.

The video includes the engine's MIDI music and sound effects, recorded directly
after SDL mixing into bounded PCM buffers, with a sample-aligned music stem
tapped before effects are added. No external MIDI arrangement or
replacement briefing is used. Raw WAV files and audio timing/source metadata sit
beside each screen recording. The editor subtracts the music stem before
retiming effects to match the picture, then mixes music at its original tempo.
It compares the rendered music samples with the native capture exactly; the
launcher and system picker remain naturally silent. The smooth fly-out visuals
use a separate normal-speed engine pass for their audio. `audio-sources.json`
records each source, effects speed, unchanged music tempo and the engine-selected music; `audio-validation.json`
checks the decoded final AAC for audible menu, briefing, gameplay and fly-out.
The editor balances excerpt loudness toward -22 dBFS RMS with at most 12 dB of
boost and 2 dB of peak headroom, preserving each excerpt's music/effects mix.
Short fades prevent clicks at cuts; the last 150 ms fade out at the ending.
Retail D2 normally uses ambient hum in its built-in campaign briefings. The
capture script explicitly requests the engine's original `SONG_BRIEFING`
(`briefing.hmp`) for this MIDI-backed preview; normal app behavior is unchanged.

The D2 opening requires the full game's `ROBOTS-H.MVL`. The generator searches
locally under `game_data` and verifies the retail library's SHA-256 when absent
from the content index. It imports this through the app together with the other
game files, so the engine plays its original robot videos. The original silent
review omitted this optional library; its briefing was real but lacked robots.
`briefing/` retains engine state and paired screenshots for inspection.

The 16:9 canvas pads the original wide phone display without cropping
the touch controls. The launcher is portrait for stills and landscape for video.
Actual status bars, HUD and touch UI are captured, not composited replacements.

## Rerun and revise

Captures use native trilinear texture filtering, 4x MSAA and 16x anisotropic
filtering. The dedicated emulator uses guest ANGLE over the host GPU because
the host GLES translator does not expose AF. The runner restarts an existing
capture AVD with that backend when needed; other devices are untouched.
The generator applies the profile on every game launch, accepts the engine's
graphics confirmation before gameplay capture, and validates the effective
MSAA buffers and AF capability. Unsupported settings fail the capture instead
of silently producing unfiltered footage. `graphics/` and demo `start-state.json`
files retain the native evidence. Menu/HUD filtering retains its normal defaults.
Filtered ANGLE replays can take several times their normal duration to capture.
The editor restores the recorded simulation frame rate, while MIDI stays at
native tempo; capture timeouts allow this slower rendering.

```powershell
# Reuse an APK already built from the current sources
.\android\generate-store-assets.ps1 -NoBuild

# Capture every checked-in input demo
.\android\generate-store-assets.ps1 -AllDemos

# After editing a run's edit.json, rebuild only the video and gallery
.\android\generate-store-assets.ps1 -ComposeOnly -OutputDirectory android\temp\store-assets_YYYYMMDD_HHMMSS

# Recreate the eight chosen PNGs from existing captures, without recapturing
.\android\generate-store-assets.ps1 -SelectStillsOnly -OutputDirectory android\temp\store-assets_YYYYMMDD_HHMMSS

# Revalidate a completed capture without touching the emulator
.\android\tests\test_store_asset_pipeline.ps1 -OutputDirectory android\temp\store-assets_YYYYMMDD_HHMMSS
```

`edit.json` intervals refer to seconds in the corresponding `raw` source file.
Each clip's `seconds` field specifies its final duration; the editor enforces
an exact 30-second total. Gameplay's `frame_rate` restores the recorded cadence;
other clips use their source interval to adjust playback speed. Per-demo manifests map
simulation frames/time to recording timestamps, so emulator slowdown does not
move the late-demo selection to the wrong part of the recording. Screenshot
polling can land a fraction of a second after a ten-second boundary; both the
target and observed replay time are recorded.

Individual recovery commands are available through
`android/helpers/generate_store_assets.py`: `seed`, `opening`, `flyout`, `replay
--demo <path>`, `compose`, `review` and `validate`, all with `--output <folder>`.
Capture commands refuse any AVD except `DxxStoreAssets`. Offline editing and
validation do not require a running emulator. The integration test is registered
as explicit because a full capture provisions an emulator and takes several
minutes. Failed runs retain their logs and partial media for diagnosis.

For a run created before the shortlist existed, first add its supplementary boss
capture, then select the stills (the dedicated capture emulator must be running
and already provisioned):

```powershell
& android/temp/store-assets-tools/venv/Scripts/python.exe android/helpers/generate_store_assets.py capture-still-sources --output android/temp/store-assets_YYYYMMDD_HHMMSS
.\android\generate-store-assets.ps1 -SelectStillsOnly -OutputDirectory android/temp/store-assets_YYYYMMDD_HHMMSS
```

For an existing run, `replay --video-only --demo <path> --output <folder>`
refreshes its video while retaining the existing PNG candidates. Regenerate
`edit.json` or update its source intervals when replacing raw recordings; old
timestamps refer to the old capture and must not be reused.

Review the PNG candidates and motion before choosing store uploads. Nothing in
this workflow publishes to Google Play or YouTube.

Full regeneration also captures `featured/featured-d1-level7-boss.png`. Its
separate `featured` recipe in `store-stills.json` pins a native replay frame
matching the reviewed boss/large-laser moment. It uses full-screen cockpit mode
and immersion mode, with native boss health and robot/hostage/secret progress
explicitly enabled and no rear camera. Touch controls and corner indicators
remain visible. D1-in-D2 supplies this presentation because native D1's immersion
mode suppresses all HUD rows. Native D1 is used to calibrate the reference frame.
The previous D1-in-D2 rear-camera recipe is preserved under
`featured_alternates.d1-in-d2-rear-camera`; copy that object into `featured` to
regenerate that variant. Its native checkpoint translation support is retained.
Filtering, 4x MSAA and 16x AF are verified through native state.
The eight-image shortlist remains separate. `featured.json` records the recipe,
source timestamp, hashes and replay comparison, including any mismatch.
The featured capture pauses the native replay just before the landmark, advances
one simulation frame at a time to it, and captures a lossless Android PNG while
paused. The recipe captures at **2048x1000** and exports an opaque RGB PNG at
exactly **1024x500** using a 2x Lanczos downsample, without cropping, padding or
stretching. The generator temporarily adjusts the dedicated emulator's display
and the engine's resolution/aspect ratio through the launcher settings API,
accepts the native resolution trial, and restores the prior settings even if
capture fails. Full-resolution sources
live in `featured-1024-source`; `featured.json` records source/output hashes and
the resize filter. Validation checks the native render size, final dimensions,
downsampled pixels, drawn boss/progress rows and the exact pinned frame.
The optional Python `capture-featured --review-frames N` captures nearby paired
cockpit/reference and full-screen images for manual calibration without replacing
the exported featured image. After review,
pin `frame` and its corresponding `seconds` in the recipe; ordinary reruns then
capture that single moment without ranking or selecting different frames.

To capture only this additional image in an already provisioned run:

```powershell
.\android\generate-store-assets.ps1 -FeaturedOnly -OutputDirectory android/temp/store-assets_20261004_filtered
.\android\tests\test_store_asset_pipeline.ps1 -FeaturedOnly -OutputDirectory android/temp/store-assets_20261004_filtered
```

Use `-NoBuild` when the capture APK already has HUD/camera automation and
introspection plus native replay pause/frame-step support. `-SelectStillsOnly` also re-exports the featured PNG when its
capture exists. Full media validation requires the featured image as well.

To repeat only the downsample/export from the preserved native source, without
an emulator replay:

```powershell
.\android\temp\store-assets-tools\venv\Scripts\python.exe android/helpers/generate_store_assets.py select-featured --output android/temp/store-assets_20261004_filtered
```

## Combining filtered video with reviewed sound

The recommended regeneration command preserves a reviewed soundtrack while
regenerating the launcher, gameplay, stills and fixed-step filtered fly-out:

```powershell
.\android\generate-store-assets.ps1 -AudioDirectory android/temp/store-assets_20261004_combined
```

For already captured visuals and a new fly-out, combine them offline:

```powershell
.\android\generate-store-assets.ps1 -CombineVideo `
    -PictureDirectory android/temp/store-assets_20261004_filtered `
    -AudioDirectory android/temp/store-assets_20261004_tempo `
    -OutputDirectory android/temp/store-assets_20261004_combined
```

The output folder must contain the new `flyout.json` and recording, or supply
`-FlyoutDirectory`. To recapture only that section on the provisioned capture
emulator with the current debug APK installed:

```powershell
& android/temp/store-assets-tools/venv/Scripts/python.exe android/helpers/generate_store_assets.py flyout --visual-only --output android/temp/store-assets_20261004_combined
```

Combination checks that picture/audio use the same clip order, durations and
demo simulation intervals. It copies the reviewed AAC without remixing,
retiming or re-encoding, and verifies identical decoded audio in the final MP4.
The first 24 seconds reuse the filtered H.264 clips. Only the new fly-out is
encoded. All inputs needed for an offline re-export are archived under `inputs/`
and SHA-256 pinned by `combined-edit.json`. Re-export without older source folders:

```powershell
& android/temp/store-assets-tools/venv/Scripts/python.exe android/helpers/generate_store_assets.py compose-combined --output android/temp/store-assets_20261004_combined
.\android\tests\test_store_asset_pipeline.ps1 -OutputDirectory android/temp/store-assets_20261004_combined
```

Combined validation tracks tunnel-wall geometry rather than counting explosion
pixel changes. It reports displacement throughout all six seconds, while checking
the initial moving tunnel for held camera frames and uneven jumps. The camera's
later stationary view is part of the native sequence. These checks require pinned
NumPy 2.2.6 and OpenCV headless 4.12.0.88, installed by `-CombineVideo`.

The earlier audio subtraction used a negative `amix` weight with normalization
disabled. FFmpeg uses the absolute weight in that mode, so it added music to
the effects before acceleration. The editor now inverts the signal explicitly,
and integration verifies that a non-silent track subtracted from itself produces
digital silence. A separate reviewed audio pass is still preferred when the
filtered emulator renders far below real time: compressing effects by fivefold
can damage transients even after music cancellation is correct.
