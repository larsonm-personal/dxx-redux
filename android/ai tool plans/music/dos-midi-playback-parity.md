# DOS MIDI capture and playback parity

## Scope

Make DOSBox capture reproducible, compare captured MIDI with the production
Android HMP conversion, and correct the confirmed startup, timing, and global
loop differences. Keep native MID playback and desktop backends unchanged.
Do not claim synth/audio equivalence from an event-stream comparison.

## Work

1. Retain the original capture and provenance outside scratch retention
2. Add an isolated DOSBox capture preparation/launch helper and instructions
3. Add a production-converter MIDI exporter and an independent SMF diff
4. Implement shared HMP playback conversion with startup volume, exact tempo,
   HMI marker filtering, and supported global loop restoration
5. Route gameplay and preview through it; preserve synth state across HMP loops
6. Add synthetic regression coverage and run the real DOS capture comparison
7. Build both Android engines and run scoped quality checks

## Validation boundaries

- The reference is DOS Descent 1.4 General MIDI at music volume 8
- Treat unsupported local/nested branch semantics explicitly, not as proven
- Preserve the capture's one-tick timing variation in the diff tolerance
- Keep proprietary game files/captures local; check in tools and fixture hashes

## Implemented

- `capture_dos_midi.ps1` / `dos_midi_capture.py` prepare isolated, hashed runtimes
  and optionally extract the selected HMP from a HOG
- Original capture and HMP retained under ignored
  `game_data/music/dos-references/descent14-game02`, outside scratch retention
- `hmp_midi_export` compiles the production converter and pinned TML; exports
  both the SMF and actual TML-decoded events as a second SMF
- `midi_diff.py` checks per-channel event order/data, times, effective note state,
  SysEx, finalized SMF structure, and explicit capture boundaries
- `test_dos_midi_parity.ps1` builds/runs the native and Python regression suites
  and checks the hash-verified DOS recording; legacy conversion is a failing control
- Gameplay and preview use the new bounded playback converter; the old
  `hmp2mid_mem` interchange/metadata path remains available for the baseline
- Exact header tick rate, initial volume zero, measured full-volume CC7 transfer,
  HMI marker filtering, branch-record restoration, ended-track exclusion
- HMP render timing uses the shared sample-based timeline, including trailing
  silence and repeated loop cursor wrapping without resetting the synth
- HMP transitions retain program/bank/pan across TSF reset; preview seeks restore
  the initial state of their current song

## Additional findings

The branch table starts at the little-endian offset at HMP header 0x20. It begins
with one count byte per track, followed by 24-byte records. Global branch 128
records supply the target event byte offset, saved program, controller byte
count, and controller pointers. The restoration channel is the target event's
status channel. Reconstructing controller snapshots from preceding events alone
misses this detail and emits the wrong default program messages.

An already-ended track is not restored or reactivated by the global branch.
Level 2 track 14 ends early; this explained an extra program change in the first
implementation and is now covered by a synthetic fixture.

Tracks whose first delta is nonzero are decremented on the initial driver tick.
Applying that rule also removes the per-track one-tick offsets in the original
research comparison. The exact tick-rate formula agrees with the 60 PPQN /
header BPM interpretation in [ScummVM's HMP parser](https://github.com/scummvm/scummvm/blob/master/audio/midiparser_hmp.cpp),
as well as the captured clock. The implementation uses 1000000 us/quarter and
division equal to header 0x38 to avoid rounding the tempo itself.

The stricter note-state comparison identified inherited pan on channels 4, 6,
7, and 9 (47, 36, 52, and 97 respectively). These come from the briefing, not
Level 2. The reference comparison explicitly shares preceding program/bank/pan
context; it does not seed volume or skip state comparisons. Native TSF tests
separately validate the transition helper and subsequent pan updates.

The pinned TSF `tsf_channel_get_pan()` subtracts 0.5 from an already-offset pan.
The local snapshot helper compensates for this pinned dependency behavior, with
endpoint tests; dependency upgrades must review that compensation.

## Validation completed

- DOS comparison: 29341 matching channel messages over 279.6 seconds, first
  global loop included; maximum timestamp difference 6 ms with 10 ms tolerance
- Legacy negative control fails
- Converted 27 D1 HMP plus 7 D2 HMP and 5 D2 HMQ assets successfully
- Synthetic startup/repeat, malformed input/allocation failure, unsupported-loop
  fallback, ended-track, MIDI diff negative controls, timeline and TSF state tests
- Final D1 and D2 arm64 native libraries and x86_64 debug APK built successfully
- Fresh APK verified to contain the new converter and installed on the emulator
- `test_music_track_controls_unified.jsonc`: D2 passed 48/48 steps, including
  preview play/seek/pause/resume; D1 passed 35/35 steps. Both exercised gameplay
  music selection and next/previous controls. No HMP conversion errors in logcat
- Existing preview synchronization (15) and render-thread tuning (8) tests passed
- Capture preparation helper reproduced the original HMP hash
- Scoped mixed-language formatting and `git diff --check` passed
- Optional host AddressSanitizer build could not link because this MSVC install
  lacks `clang_rt.asan_dynamic_runtime_thunk-x86_64.lib`; no sanitizer result claimed

Local validation artifacts are under `temp/midi-parity/`: `final-runner.log`,
`reference/summary.json`, `android-build-final.log`, `apk-build.log`,
`emulator-d2-new.log`, and `emulator-d1-new.log`. Gradle's injected-ABI build
placed the fresh test APK at `app/build/intermediates/apk/debug/app-debug.apk`;
the older APK under `outputs/apk/debug` was excluded from final device validation.

See `android/tests/fixtures/dos-midi/README.md` for commands and exact boundaries.
Modulation, pressure, reverb/chorus synthesis, lower DOS volume settings, device
track selection, local/finite/nested branches, and OPL audio remain unverified or
unsupported; an event-stream match does not establish audio equivalence.
