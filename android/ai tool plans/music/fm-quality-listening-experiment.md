# FM output-quality listening experiment

Produce 60-second, level-matched game01/game07/game08 comparisons without changing
shipping playback or the FM arrangements, instruments, chip clock, or HMI driver.

1. Build the production host renderer and isolated experimental copies
2. Compare current PCM16, floating-point box resampling, native-rate output with
   windowed-sinc conversion, and an optional 5 Hz high-pass / 8 kHz low-pass tone
3. Verify register-event equivalence, repeatability, duration, peaks, loudness,
   and actual differences; preserve raw files and input/source hashes
4. Publish a local listening page with synchronized switching and reproducible
   runners; label filtering as an audition, not measured card emulation

Only the existing pinned BSD ymfm/ymfmidi and MIT TinySoundFont code is compiled.
Offline analysis uses pinned NumPy/SciPy in an experiment-local environment.
No new soundfont or third-party instrument bank is introduced.

## Completed 2026-09-23

- Reusable source/runner: `android/tests/fm_quality/`
- Listening page: `temp/fm-quality/listening/listen.html`, twelve 60-second clips
- Shipping event/bank/arrangement selection retained: game01/game08 use HMQ;
  game07 uses HMP with the original AdLib banks
- All ordered register traces agree after conversion to millisecond timestamps;
  baseline and native renders repeat byte-for-byte for all three tracks
- Presentation loudness matched to -20.9 LUFS, with no clipping or limiting
- Sinc tone check: passband checks passed at 1/20 kHz, 24.6 kHz rejected by 88.5 dB
- Release CMake build passed without warnings; scoped mixed-language checks and
  explicit CMake format/lint passed (the repository formatter's CMake allowlist
  does not include this new standalone test directory)
- Page JavaScript checked for syntax, file routing, selected state and simulated
  position-preserving paused/playing switches; actual browser audio UI could not
  be checked because no browser surfaces were available to the UI tool

Next decision belongs to listening: choose the useful treatments before attempting
a real-time implementation and Android CPU/latency measurements. No app changes
are part of this experiment.

## Listening feedback and follow-up

User preferred more precision for a high note at game01 0:30-0:31, clean resampling
at game07 0:45-0:50, and both on game08. Warm filtering helped game01 but removed
important riffs on game08. This favors clean processing as the candidate baseline
and warmth as an optional tone setting, subject to further listening.

Add a one-pole 8 kHz warm version alongside the original two-pole version, holding
the cutoff, DC filter, source and loudness-matching method fixed. Add shortcuts to
the reported passages, regenerate all fifteen clips, and verify the existing
versions remain identical where the common loudness target allows.

Follow-up complete: all fifteen clips pass at the same -20.9 LUFS target, and the
original twelve listening WAVs retain their exact SHA-256 hashes. Release build
and scoped formatting/lint passed. Page JavaScript checks cover fifteen file
links, all three cue buttons, preserved positions and paused/playing switches.
At 12/16 kHz, the one-pole filter attenuates about 6/10 dB versus 10/19.1 dB for
two poles, before loudness matching; both attenuate 8 kHz by about 3 dB.
