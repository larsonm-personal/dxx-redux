# FM startup and MIDI preview forward progress

Investigate the reported 2-3 second FM startup delay in menu/briefing/level music
and the SC-55 MIDI preview slowdown/hang around ten seconds in D1 game01.

1. Measure shared synth initialization/preparation/reset and preview rendering,
   callback progress, queue starvation and first audio; reproduce before fixing
2. Fix the measured bottlenecks without increasing waits, reducing selected
   synthesis quality, or hiding underruns with a larger queue
3. Preserve deterministic seek/reset, saved MIDI settings and FM fallback
4. Save a reusable sustained-playback diagnostic, run native contracts, build
   all Android ABIs, and verify startup/transitions and sustained SC-55 playback

Initial hypotheses: the Android debug synth targets lack optimization; FM also
loads the fallback soundfont eagerly. Measure each stage before choosing fixes.

## Findings and changes

- Reproduced SC-55 starvation in the unoptimized Android Debug build: 222
  sustained underruns in 35 seconds, falling below real-time audio consumption
- Enabled `-O2` for shared synthesis, ymfm/player and FluidSynth's object target
  in Android Debug (also used by internal APKs), preserving assertions, precision,
  effects and voice limits. The repeat sustained 48 kHz for 45 seconds with no
  sustained underruns
- Gameplay startup exposed a separate quadratic allocation problem: HMP output
  grew by a few bytes per event and the debug allocator copied it every time
  D1 game01/game02 preparation took approximately 450-600 ms on the emulator
  A synthetic 40 KB MIDI required 30,010 reallocations totaling 601 MB requested
- Added geometric output-buffer growth for both export and playback conversion
  The regression checks bounded allocation cost and every allocation-failure
  point on long GM/FM/legacy conversions. It fails on the old converter
- Verified 156 MIDI outputs (once/repeat/legacy, 52 D1/D2 HMP/HMQ files) are
  byte-identical before/after; native HMP and synth contracts pass
- Kept opt-in startup and sustained-preview diagnostics through the central
  profiling logger (`debug.dxx.music_timing`), with no new callback logging

The all-ABI APK build with the converter fix passes. D1 and D2 music-control
tests pass 41/41 and 48/48 steps. D1 game01 preparation fell from 454 to 25 ms;
D2 game02 fell from 1593-1609 to 75-85 ms, with first PCM dropping from about
1.93 seconds to 0.41 seconds. Full timing results and reproduction instructions
are in `android/tests/music_realtime/README.md`.

Final sustained preview passes with opt-in logging: menu, briefing, 45 seconds
of FM game01, and 45 seconds of SC-55 game01 all sustain 48 kHz with zero
post-startup underruns. Native converter, playback, synth, soundfont and seek
contracts and 15 preview synchronization tests pass. Scoped formatting passes.
Only emulators are connected; the user's phone still needs an audition of this APK.
