# Plan: music track chromaprint progress 2026-06-11

- [x] Trace the file metadata music-track list and zip audio fingerprint generation path.
- [x] Add a progress signal for zip music analysis without blocking the UI thread.
- [x] Render progress in the music tracks submenu while preview buttons are still unavailable.
- [x] Add or update focused tests for the progress model/UI helper where practical.
- [x] Run scoped formatting and relevant tests.

## Result
- Added determinate progress state for mission zip music chromaprint generation and optional AcoustID lookup.
- The music tracks dialog now shows a progress bar and count while preview buttons are unavailable.
- Fingerprintable rows show `Analyzing` during the initial local chromaprint pass.
- Added `MissionZipMusicProgressTest` for progress formatting and fraction calculation.
