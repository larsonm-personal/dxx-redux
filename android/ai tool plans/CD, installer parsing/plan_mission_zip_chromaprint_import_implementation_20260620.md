# Mission Zip Chromaprint Import Implementation

## Goal
- Run bundled chromaprint recognition for mission zip soundtracks during import/staging, and show matched names in the in-game music track list without requiring manual metadata viewing.

## Steps
- [x] Add reusable mission zip audio identification helpers outside the metadata dialog.
- [x] Write a mission soundtrack name sidecar next to staged or extracted mission zip files.
- [x] Load that sidecar from the Android engine path for built-in/addon mission music names.
- [x] Keep CD audio and standalone audio import behavior intact.
- [x] Add focused tests and run formatting/tests for touched Kotlin/C files.
