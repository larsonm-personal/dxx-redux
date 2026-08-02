# Regeneration output stability implementation

## Goal

Make mission metadata and Chromaprint regeneration produce stable, complete output without relying on a final cleanup pass or adding tests that duplicate checked-in regression-file review.

## Plan

- [x] Recheck live generator code and existing worktree changes for overlap
- [x] Canonicalize mission ordering and preserve metadata fields in Android output
- [x] Make per-file JSON normalization and publication fail closed and atomic
- [x] Make regular music-pack sidecar publication atomic and record generator identity where appropriate
- [x] Run focused existing checks, scoped formatting, and inspect representative output diffs
- [x] Record completed work and remaining validation requiring a full regeneration run

## Implementation notes

- Android mission descriptors are sorted deterministically before analysis, and the per-file normalizer sorts mission arrays by `mission_filename` and recomputes `target_index`
- Android regression serialization uses descriptor display names and retains `calculated: false`
- App, host, emulator, and regular music-pack publication paths now replace completed files atomically; emulator normalization failures stop publication
- Fingerprint scripts rebuild the default executable incrementally before generation so reruns use current native code
- Existing focused normalization, cache provenance, AcoustID regeneration, fingerprint publication, Kotlin metadata, and native fingerprint build checks pass
- A full emulator regeneration remains the final corpus-level check; Ogg duration changes and substantive route changes still require review of the resulting regression diff
