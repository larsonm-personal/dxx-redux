# DOS Level 7 drum comparison

## Plan

1. Verify Level 7 song mapping and capture the original DOS General MIDI output
2. Compare the opening percussion and full loop with the production converter
3. Check drum kit, velocity, channel volume/expression, retained state and synth behavior
4. Fix confirmed differences and add a repeatable regression for Level 7
5. Produce short reference/candidate audio clips where a comparable synth is available
6. Build and run relevant native and Android integration tests

Keep the original GOG runtime and phone settings unchanged. Record any changes
to the disposable DOS capture pilot/configuration. Distinguish MIDI parity,
General MIDI instrument balance, and the separate AdLib/OPL sound target.

## Findings and implementation

- User confirmed the target is GOG's default AdLib/FM sound and the phone issue
  was heard after the previous fixes
- Level 7 maps to `game07.hmp`. Its header holds separate FM, GM/GUS and digital
  sample arrangements. The prior converter layered all of them, including two
  channel-10 kick drum parts at time zero (note 36, velocities 119 and 123)
- Original DOS General MIDI chooses tracks 6,9,10,11,12,13. The new shared
  converter selects 0xa000/universal tracks and excludes the seven alternatives
- Device records start at 0x90 for track 1 and use a 20-byte stride, not 16
- Empty device lists remain universal. FM-only HMQ files without GM notes retain
  approximate legacy all-track playback, explicitly reported in diagnostics
- Level 7 loops at EOF, unlike Level 2's global branch: DOS emits CC123, CC121,
  pitch bytes 64,64, CC7=0 for used channels, then restarts one tick later
- The initial HMI pitch reset is 8256, slightly above the conventional 8192
- TSF's CC121 also erases bank/pan and leaves sustain set. HMP dispatch now
  preserves bank/pan and resets sustain and volume LSB consistently with song
  transitions, with native regression coverage
- No arbitrary drum gain or instrument replacement was introduced

## Captures and evidence

`game_data/music/dos-references/descent14-game07` retains the original MIDI,
HMP, hashes, config and pilot. The disposable v7 pilot's base-mission highest
level byte was changed from 3 to 7, grounded in `d1/main/playsave.c` / `.h`;
the source installation and original pilot are untouched.

The GM capture starts Level 7 at 102306 ms. A 250-second window contains 13336
matching channel messages with maximum 5 ms timing difference. The old converter
and a separate all-device-tracks negative control both fail. Level 2 still passes
with its original 29341 messages and maximum 6 ms difference.

`adlib/dos.wav` is the DOSBox 0.74 WAV capture using GOG's device 0xa009, music
volume 8, and effects volume 0. It includes briefing lead-in; its first 22 seconds
are included in the local listening page without claiming exact audio alignment.

The `midi_tsf_render` host tool and `render_dos_midi_comparison.py` generate
repeatable same-synth comparison clips. Enabling every device track as a negative
control raises percussion RMS by 6.015 dB over the first 20 seconds. Corrected
and captured-DOS-GM mixes measure -29.062 and -29.067 dBFS. These are quantitative
checks, not a claim that the assistant listened subjectively or that SF2 matches FM.

## Validation

- Native synth/timeline, converter/allocation and nine synthetic Python tests pass
- Both real MIDI regressions and their negative controls pass
- All 39 stock D1/D2 HMP/HMQ corpus files convert; five FM-only HMQ files report
  approximate fallback rather than becoming silent
- D1/D2 ARM64 native libraries and x86_64 debug APK build successfully
- Fresh APK native libraries verified to contain the new converter before install
- Expanded D1 music-control test passes 38/38 steps, ending on `game07.hmp` with
  audio active, peak sample 10460 and zero clipped samples
- D2 music-control/preview seek/pause/resume test passes 48/48 steps
- Existing preview synchronization (15) and render-thread tuning (8) tests pass
- Scoped mixed-language formatter and `git diff --check` pass

Logs are retained locally under `temp/dos-midi-game07/`. Regressions run with
`android/tests/test_dos_midi_parity.ps1` (both levels by default). Listening clips
are at `game_data/music/dos-references/descent14-game07/listening/listen.html`.
The converter fix applies to gameplay and preview; exact GOG FM timbre still
requires an FM synthesis backend with Descent's original instrument banks.
