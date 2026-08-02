# Full-track Chromaprint generation

## Goal

Generate Chromaprint fingerprints from complete tracks, keep reruns deterministic for the same executable, and avoid encoding fingerprint-window policy in sidecar metadata.

## Plan

- [x] Trace every native fingerprint entry point and decoder memory bound
- [x] Remove the fixed 120-second feed limit while preserving exact frame accounting
- [x] Remove the newly added sidecar schema declaration and cache requirement
- [x] Update existing focused native expectations without adding duplicate regression-file tests
- [x] Run scoped formatting, native fingerprint checks, script checks, and relevant existing regressions
- [x] Record completed work and regeneration implications

## Implementation notes

- File and in-memory MP3, Ogg, and FLAC inputs are decoded completely and every decoded frame is fed to Chromaprint
- CD audio is fed incrementally by sector so complete tracks do not require a full-track sector buffer
- The same PCM produces the same fingerprint whether it is fed in one call or in many chunks
- The temporary `fingerprint_schema` sidecar field and cache requirement were removed
- Existing native fingerprint, Windows CLI, Android native, cache provenance, AcoustID, and manifest publication checks pass
- Regeneration must use `-Force` once to replace sidecars previously produced from bounded fingerprints; later runs with the same executable should be byte-stable
