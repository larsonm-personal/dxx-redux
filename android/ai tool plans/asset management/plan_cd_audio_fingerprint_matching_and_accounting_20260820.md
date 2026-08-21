# CD audio fingerprint matching and accounting

## Goal

Restore bundled Chromaprint matches for the known D1 MacPlay BIN/CUE audio
tracks and include imported CD-audio tracks in Advanced background hashing
inventory and completion accounting, including already-hashed tracks.

## Work plan

- [x] Reproduce the MacPlay mismatch against the bundled database and trace
  track geometry through CUE parsing and fingerprint generation.
- [x] Correct the fingerprint input or matching path without weakening strict
  matching behavior.
- [x] Extend background hashing discovery and cache accounting to registered
  CD-audio tracks alongside mission audio.
- [x] Add focused and high-level regression coverage for MacPlay matching and
  CD-audio discovery/accounting.
- [x] Run scoped code quality, focused tests, native tests as applicable, and
  the Android debug build.

## Status

Complete. The local MacPlay BIN/CUE reproduced every checked-in track hash and
fingerprint, focused unit tests passed, scoped code quality passed, and the
Android debug APK assembled successfully.
