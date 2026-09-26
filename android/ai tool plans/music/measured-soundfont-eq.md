# Measured SoundFont EQ

Implement one D1+D2 correction, not per-song EQ, from the existing spectral report

1. Average accepted song differences in dB with equal song weights; smooth on log2 frequency with 0.5/1/2 octave FWHM Gaussian kernels (Detail/Balanced/Broad)
2. Fit five broad biquads to the inverse mean, restricted to 60 Hz-12 kHz to avoid chasing codec-edge artifacts. Export a versioned measured preset with font hash, exact fit parameters, corpus provenance, and a comparison HTML report
3. Add native stereo EQ after FluidSynth effects and before PCM16. Flat remains exact bypass; reset/rate/seek history follows the synth. Use the existing stopped-preview profile replacement path for preset changes, so no new live audio-thread parameter mutation is needed
4. Persist Flat/measured and smoothing in the music settings; add an EQ section in SoundfontSelector (music editor), JNI propagation to preview and both games, configuration export/reset, and introspection
5. Restrict measured correction to the matching SoundFont identity; FM and recorded music bypass it. Retain the user's selection when temporarily using another bank
6. Validate mathematical filter responses, block/reset/rate/channel contracts, native D1/D2 corrected renders, preference persistence, and installed-app preview/game setup wiring. Build native and Android targets and run scoped formatting

The measured preset describes average matching to this recording corpus, not exact reconstruction of every recording. No runtime per-song normalization or EQ selection

Completed implementation and validation:

- Fitted one equal-song correction from 28 accepted pairs (23 D1, 5 D2), with Detail/Balanced/Broad smoothing and bundled-bank identity checks. Flat remains the default
- Added the measured preset and smoothing selector to Music Editor > MIDI > Equalizer, persisted through configuration export/import and passed to previews and both native games
- Generated `temp/music-eq/report.html` with fitting curves and `temp/music-eq/native/report.html` with actual corrected renders and level-matched listening clips
- Native Balanced validation reduced mean per-song spectral error from 3.589 to 2.408 dB; 22 of 28 songs improved. No clipping; maximum corrected true peak was -2.8 dBFS. Fresh Flat renders matched baseline PCM byte-for-byte for D1 and D2
- Native synth CTest, 15 MIDI sync Python tests, 19 targeted JVM tests, and Android debug builds for all three ABIs passed. Scoped formatting and diff whitespace checks completed
- Installed-app integration passed all four EQ selections on D1/D2 previews, seeking, pause/resume, persistence, FM bypass, and measured EQ activation during native gameplay for both games. Actual editor radio and smoothing controls were exercised
- A 35-second measured-EQ real-time probe sustained 47,994 frames/second with zero underruns and render cost of 0.0162 wall seconds per audio second
- Verified report asset links and restored emulator preferences after testing
