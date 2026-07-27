# BR-0064 extract every CUE data track on Android

## Goal

Discover and extract every supported data track declared by a CUE sheet on
Android without treating the first data track as the complete disc.

## Plan

- [x] Read repository instructions and the complete BR-0064 finding
- [x] Compare the frozen and live CUE parsing, JNI, Kotlin import, progress,
      collision, and commit paths
- [x] Define bounded multi-data-track discovery and extraction semantics,
      including per-track failures and duplicate output handling
- [x] Add mixed-mode, multiple-data-track, malformed-later-track, and
      cross-track collision regressions
- [x] Run scoped code quality, focused CUE/ISO and Kotlin tests, all native
      extraction suites, and Android ABI builds
- [x] Finalize BR-0064 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Frozen-to-live trace: PASS; both Android entry points still selected only the
  first data track, while the CLI retained its all-track loop
- Focused `CueDataTrackExtractionTest`: PASS, including mixed audio/data order,
  same-image and multi-image tracks, unique later files, later-wins collisions,
  malformed later metadata, aggregate storage and progress, cancellation before
  the next track, and rollback after a later failure
- Scoped code quality and direct ktlint for the new untracked test: PASS
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
