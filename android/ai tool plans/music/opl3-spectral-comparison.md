# OPL3 spectral comparison

1. Map the supplied full-version D1/D2 MP3 recordings to HMP tracks in the existing game HOGs
2. Extend the spectral runner to select the production ymfm backend, reject SF2 fallback, and retain explicit renderer/reference provenance
3. Render up to 90 seconds per song with no EQ, align recordings, and export per-song and aggregate HTML spectra with matched listening clips
4. Run numerical checks and inspect accepted/rejected pairs and rendered graphs; report measured differences before fitting any OPL3 EQ

Completed: `temp/music-spectral-opl3/report.html` contains graphs, matched listening
clips, JSON provenance and CSV measurements. All 34 full-version pairs attempted;
33 produced verified native ymfm audio with Flat EQ. D2 briefing selected the
production SF2 fallback and was rejected. Of the 33 OPL3 pairs, 11 passed the
unchanged alignment screen (nine D1, two D2); 22 uncertain pairs retain graphs
but are excluded from summaries

Median band differences after 250-2000 Hz matching:

| Game | 80-250 Hz | 2-4 kHz | 4-8 kHz | 8-16 kHz |
| --- | --- | --- | --- | --- |
| D1 | -0.9 dB | +1.6 dB | 0.0 dB | -0.7 dB |
| D2 | -1.3 dB | +1.1 dB | -1.4 dB | -2.5 dB |

These limited accepted subsets do not show broad high-frequency excess. Timing
differences and the small D2 subset should be resolved before publishing an OPL3
correction. No OPL3 EQ or playback behavior was changed

Validation: host renderer build and native CTest passed, five numerical spectral
tests passed, scoped code-quality checks passed, report links and native backend
logs verified, and representative and summary graphs visually inspected
