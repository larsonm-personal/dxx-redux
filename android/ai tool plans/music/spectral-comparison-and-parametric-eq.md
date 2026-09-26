# D1/D2 spectral comparison and parametric EQ

Status: baseline spectral report implemented on 2026-09-25; EQ remains planned

## Objective

Determine whether our synthesized D1/D2 music has consistently excessive high-frequency energy relative to matched recordings, independently of playback loudness. Produce repeatable numerical results, plots, and listening comparisons before choosing a correction. Support optional named EQ presets if the evidence warrants implementation

## Existing foundation

- `android/app/src/main/cpp/extract/test_music_synth.c` supplies the production `--render` host path
- `android/tests/compare_music_profiles.py` already produces PCM measurements and an HTML listening report using that renderer
- `android/tests/render_dos_midi_comparison.py` and the DOS parity fixtures help diagnose event/arrangement differences
- `shared/music_synth.cpp` selects FluidSynth/SF2 or ymfm, with a common public API used by gameplay and launcher previews
- `shared/music_fluid.cpp` renders float PCM before clamping to PCM16; the FM branch also has a float stage before conversion
- `game_data/music/D1 MIDI mp3 sc55` contains 26 OGG files; `D2 midi mp3 sc55` contains 10. Inventory and match actual songs; these counts do not imply that every file is a unique compatible pair
- Other recording families include SC88, SC88Pro, MU80, ARACHNO and OPL3. Keep them separate
- `midi-gain-calibration.md` documents the recent SF2 gain increase from 0.2 to 0.4. Existing six-track peak checks are useful but do not establish spectral balance or corpus-wide headroom
- Historical FluidSynth quality experiments have different gain/interpolation settings. Use a freshly built production renderer, not those historical renders, as the primary baseline

## Phase 1: explicit reference manifest

1. Inventory D1/D2 HOG/SNG/HMP assets, selected SF2 files and recording metadata, reusing engine conversion and song resolution
2. Create a checked-in manifest mapping game, HOG song identity, arrangement/backend, soundfont identity, reference file, reference family, and valid comparison sections
3. Start with bundled SC-55 versus SC-55 OGG recordings. Verify the selected font and reference provenance rather than assuming all SC-55-labelled assets represent identical instruments
4. Separately compare FM with OPL3/DOS captures if desired. Exclude redbook, remixes, PlayStation arrangements and ambiguous song matches from corrective fitting
5. Record content hashes, source/codec/sample rate, tool versions, engine revision, effective gain, effects, interpolation, voice limit and actual selected backend. Record fallback explicitly
6. Split valid songs into fitting and held-out validation sets before fitting. Deduplicate repeated level songs and alternate encodings so they do not receive extra weight

## Phase 2: reproducible production renders

Extend the existing host render command only where needed to support full-song/explicit-duration export and effective-setting metadata. Preserve the production HMP device-track selection, controller initialization, scheduling, loop semantics and synthesis implementation

- Render stereo at 48 kHz with shipping gain, reverb, chorus and voice limits
- Capture at least a complete matched musical cycle where available, plus several interior sections; do not use only the opening 20 seconds
- Distinguish cold starts, inherited controller state and repeat passes. Compare corresponding sections and reconstruct preceding synthesis when extracting an interior section
- Keep the shipping PCM16 baseline and optionally export float PCM or pre-clamp peak statistics to identify overload hidden by final conversion
- Decode reference OGG/MP3 to float PCM using a pinned FFmpeg build; resample once to the comparison rate
- Reuse deterministic output locations under `temp/music-spectral/`, or invoke the repository retention helper before creating timestamped run directories

Proposed entry point: `android/tests/compare_music_spectra.py --manifest <path> --renderer <exe> --output <directory>`. This command does not exist yet

## Phase 3: alignment and level control

- Align onset/envelope features with bounded offset search; verify multiple anchors to detect tempo drift and mismatched sections
- Store offsets, compared intervals and an alignment quality score. Reject uncertain pairs from automatic fitting and show the reason in the report
- For tempo drift, map corresponding musical windows without pitch-changing resampling. Avoid unrestricted time warping that can hide arrangement differences
- Exclude lead-in silence, unmatched fades and loop tails using explicit recorded bounds
- Preserve raw loudness and peak measurements. Separately apply one constant gain per compared track section for integrated-loudness-matched analysis and listening; do not use dynamic normalization, compression or limiting
- Also show a gain-independent high-band/mid-band energy ratio and a midrange-anchored spectrum, since loudness weighting itself responds to treble
- Sum left/right spectral powers for the main spectrum rather than summing waveforms to mono, which can cancel stereo content

## Phase 4: numerical and visual comparison

Use pinned NumPy/SciPy/Matplotlib dependencies for analysis and standalone plots, with FFmpeg for decoding and loudness/true-peak measurements

- Compute Hann-windowed power spectra with a documented window length and overlap, for example 4096 samples and 75% overlap at 48 kHz
- Average power in the linear domain, integrate into logarithmic bands, then convert to dB. Keep absolute and level-adjusted curves distinct
- Define difference as `10*log10(P_ours/P_reference)` after documented gain adjustment: positive means more energy in our playback
- Report low/mid bands plus 2-4, 4-8 and 8-16 kHz energy differences, spectral centroid, LUFS, sample/true peaks and clipping counts
- Mask bins near the reference noise floor and identify codec bandwidth limits; never fit a steep recording cutoff or attempt to boost missing reference energy
- Show per-song curves, section variation, and equal-song-weighted median differences with spread and song-level bootstrap intervals. Frames within one song are not independent evidence
- Separate D1/D2 summaries and reference/soundfont families. Publish all valid pairs, not only examples supporting the treble hypothesis
- Where individual tracks disagree strongly, inspect percussion/instrument stems, effects, controller state and voice stealing before treating the result as an output-EQ problem

Deliverables: `report.html`, normalized `report.json`, `summary.csv`, spectrum/difference PNGs, and synchronized loudness-matched reference/current/candidate listening clips. The report must distinguish measured tonal differences from a claim of historical authenticity

## Phase 5: fit a conservative candidate only if justified

Fit the negative of the broad, robust spectral difference using the actual parametric-filter response, not an arbitrary FFT-bin inverse

- Start with a high shelf; add at most one or two broad peaking filters if they improve held-out songs
- Penalize gain magnitude and narrow filters. Initial fitting bounds: gains within +/-6 dB and bell Q from 0.3 to 2; these are fitting constraints, not measured correction values
- Fit a constant gain nuisance term separately so an overall level mismatch does not become EQ
- Weight songs equally; exclude unreliable frequencies and unmatched arrangements
- Publish candidate frequency/gain/Q or shelf slope, response plot, preamp and before/after errors
- Accept only if broad-band error improves on held-out songs in both games without systematic new band errors or clipping. Report effect sizes and uncertainty; do not equate a statistically visible difference with an audible defect
- Listen to level-matched comparisons, optionally with randomized labels. Re-measure loudness after EQ and use constant gain for fair listening copies
- If only specific instruments or soundfonts are bright, correct those causes or scope the preset accordingly. One global EQ cannot repair wrong patches, missing parts, dynamics, or effects

## Phase 6: reusable runtime EQ and presets

Implement a small allocation-free stereo biquad cascade in shared native code under `android/app/src/main/cpp/shared/`, with independent channel state and common coefficients

- Support low shelf, peaking and high shelf filters, up to five active bands, plus preamp
- Store frequency in Hz, gain in dB, bell Q and shelf slope as explicit distinct fields, stable preset ID, display name and intended renderer/font scope
- Provide Flat/bypass, a measured reference-match preset only after validation, and optional taste presets such as Warm and Reduced Treble. Label taste presets separately from measured correction; custom presets can use the same bounded data format
- Keep Flat as the initial default. Select/persist presets through the existing music settings path; share semantics between preview and gameplay and expose the effective preset to introspection
- Apply after synth effects and FM resampling, before PCM16 clamping and before adding to any existing destination buffer. Process music only; applying to the final SDL mix would also change sound effects
- Share one DSP implementation between SF2/FM, with presets scoped to the appropriate backend/font. Do not automatically apply an SC-55 correction to another soundfont, FM, OGG or CD playback
- Preserve the existing bypass conversion path for exact PCM parity. Do not first clamp to int16 and then run EQ: later cuts cannot undo earlier clipping
- Compute/validate coefficients outside the render callback. Publish bounded updates at block boundaries and briefly crossfade parallel old/new filters to avoid clicks
- Validate finite values, frequency below Nyquist, stable poles and bounded gains. Provide headroom from the combined response plus measured peaks; boosts require attenuation. Do not add an always-on limiter to hide overload
- Preserve filter history across ordinary blocks and pauses; explicitly handle reset, seek reconstruction, loops and sample-rate changes. Include EQ state in replayed seek audio or use a documented warm-up/fade policy
- Keep native host code portable and changes to D1/D2 hooks minimal

## Validation and completion criteria

1. Analyzer fixtures: known gain produces no false tonal difference after matching; known shelf is recovered within a stated tolerance; offset/silence and stereo anti-phase content are handled correctly; mismatched songs are rejected
2. EQ integration: Flat equals current production PCM; known filters match expected responses at 44.1/48 kHz; output is independent of render block partitioning; channel state is independent
3. End-to-end D1 and D2 renders with preset selection, resets, seeking and loops, plus preview/gameplay agreement and preset persistence
4. Verify transitions, nonfinite protection, headroom and render-worker timing under realistic polyphony; run existing synth contracts and relevant CMake/Android builds
5. Run scoped repository code quality checks for implementation changes
6. Deliver the baseline report before enabling a correction. Re-run the same corpus with the candidate and retain baseline and corrected results side by side

## Technical references

- W3C Audio EQ Cookbook, biquad peaking and shelving coefficient definitions: https://www.w3.org/TR/audio-eq-cookbook/
- FFmpeg filters, decoding-analysis integration, ebur128/astats and loudness measurement: https://ffmpeg.org/ffmpeg-filters.html

## Implemented baseline report

- Added `android/tests/compare_music_spectra.py`, an explicit SC-55 song manifest,
  pinned Python dependencies and reproduction instructions under `android/tests/music_spectral/`
- Reused the existing production host renderer without changing playback code
- Rendered 33 candidate pairs, using up to 90 seconds per song, with section alignment
  screening, midrange-normalized spectra, LUFS/true-peak measurements and listening clips
- Outputs live under `temp/music-spectral/`: `report.html`, `report.json`, `summary.csv`,
  standalone PNG graphs, original renders and level-matched listening WAVs
- The report embeds graph images for offline viewing and retains uncertain pairs for inspection
- This delivery implements an initial subset of phases 1-4. Full-song/repeat analysis,
  alternate reference families, bootstrap confidence intervals, stems, float pre-clamp
  diagnostics, EQ fitting and runtime presets remain future work
- Numerical tests cover gain independence, antiphase stereo, known treble attenuation,
  offset/silence alignment, unrelated audio and tempo drift

Baseline completed: 28 of 33 pairs passed alignment (23 D1, 5 D2). Median band
differences relative to matched 250-2000 Hz energy were +3.09 dB / +3.97 dB for
D1 and +2.57 dB / +5.45 dB for D2 at 4-8 kHz / 8-16 kHz. Bass at 80-250 Hz
was also elevated (+3.60 dB D1, +3.28 dB D2). These are reference-relative
measurements, not a validated EQ prescription

Excluded from aggregation: D1 endlevel/game16/game22 and D2 briefing/descent.
All retain graphs in the report. No track processing errors occurred

Validation passed: five Python numerical tests, freshly built native
`music_synth_tests`, scoped code quality checks, and artifact checks for all 34
embedded PNG graphs and 66 valid stereo listening WAV links. Generated plot
images were visually inspected. No connected browser was available for a live
HTML preview. No PCM boundary samples were detected in compared playback
sections; maximum measured true peak was -1.51 dBFS
