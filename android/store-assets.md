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
recording. Use `-AllDemos` to sample the full checked-in corpus. The two-second
D2 recording can produce a beginning shot but cannot supply a five-second clip.
All-corpus capture takes substantially longer, particularly the eleven-minute
D1 level 7 recording. Screen recordings are split before Android's three-minute
limit; the editor rejects a clip that crosses a recording restart.

The initial survey found an `endlevel_completed` result mismatch in
`d1_descent_level7_20260921_181652.dximdemo`. It is not in the default selection.
Full-corpus validation reports any such mismatch and retains the capture/logs.

## Presentation and video timing

The runner creates actual pilots and saves through engine menus, applies the
launcher's **Restore Defaults** preset, and keeps the default touch layout.
Input replay deliberately bypasses pilot selection and restores its original
simulation settings. The runner enables only the visual hostage/robot/secret
counters and boss bar on replay startup; it does not alter recorded inputs,
random-number state, physics or Guide-Bot routing to obtain a passing replay.
Final replay state must match the recording for validation to pass.

The opening uses the real Android file picker to import a ZIP made from the
verified local game data. It then scrolls the launcher, launches D2, creates a
pilot, selects Counterstrike, shows three briefing pages and enters level 1.
The edit removes loading waits and compresses this sequence into nine seconds.
The two D1 excerpts start at 90 percent of their demo durations. The D2 excerpt
uses seconds 48-53, a review-selected combat sequence; its late exit approach
was discarded for low action.
The final six seconds show D1's real exit animation, ending at the video's
30-second boundary before the score screen. A debug-only automation fixture
locates the authored exit tunnel in the engine and starts the existing fly-out.
It slows only that single-player sequence to one-eighth speed during capture,
then the editor restores normal speed. This lets a slow emulator render enough
real frames for 30 fps without synthesized motion frames. The clock hook is
debug-only, resets after the exit, and is disabled during replays/multiplayer.

The fly-out uses the emulator's host recorder at 60 fps. Gameplay retains the
guest recorder's variable-frame-rate stream, then restores the input demo's
recorded frame cadence instead of the emulator's uneven wall-clock timestamps.
The input demos contain approximately 25 simulation updates per second; a
30 fps output therefore repeats some gameplay frames. No motion interpolation
is used. The motion report checks visible frame changes and held frames in
addition to nominal FPS. Validation requires at least 23 visible updates/sec in
each gameplay clip, 27 in the fly-out's moving first two seconds, and no holds
longer than 140 ms. Its camera deliberately settles later in the sequence.

The video includes the engine's MIDI music and sound effects, recorded directly
after SDL mixing into bounded PCM buffers. No external MIDI arrangement or
replacement briefing is used. Raw WAV files and audio timing/source metadata sit
beside each screen recording. Edits compress audio with pitch preserved; the
launcher and system picker remain naturally silent. The smooth fly-out visuals
use a separate normal-speed engine pass for their audio. `audio-sources.json`
records each source and the engine-selected music; `audio-validation.json`
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

```powershell
# Reuse an APK already built from the current sources
.\android\generate-store-assets.ps1 -NoBuild

# Capture every checked-in input demo
.\android\generate-store-assets.ps1 -AllDemos

# After editing a run's edit.json, rebuild only the video and gallery
.\android\generate-store-assets.ps1 -ComposeOnly -OutputDirectory android\temp\store-assets_YYYYMMDD_HHMMSS

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

For an existing run, `replay --video-only --demo <path> --output <folder>`
refreshes its video while retaining the existing PNG candidates. Regenerate
`edit.json` or update its source intervals when replacing raw recordings; old
timestamps refer to the old capture and must not be reused.

Review the PNG candidates and motion before choosing store uploads. Nothing in
this workflow publishes to Google Play or YouTube.
