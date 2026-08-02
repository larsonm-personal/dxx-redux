# Music fingerprint atomic replacement fix

## Goal

Allow regenerated music-pack fingerprint manifests to replace existing files atomically on current PowerShell and .NET versions.

## Plan

- [x] Trace the failing publish call and identify the empty path argument
- [x] Replace the null backup argument with a unique sibling backup path
- [x] Extend the existing fingerprint publication coverage
- [x] Run focused tests and scoped code quality
- [x] Record the outcome and remaining regeneration validation

## Outcome

- Added one shared UTF-8, no-BOM atomic text writer that uses a nonempty, unique sibling backup path for `File.Replace` and removes both temporary artifacts after publication.
- Music pack fingerprints, emulator mission metadata, and host mission metadata now use the shared writer instead of carrying separate replacement implementations.
- The existing fingerprint publication test now exercises replacement of an existing file and verifies temporary and backup cleanup.
- `test_fingerprint_manifest_publication.ps1`, `test_acoustid_regeneration.ps1`, scoped code quality, and `git diff --check` pass.
- The full fingerprint regeneration remains the end-to-end validation. The failed run completed fingerprint computation but did not publish the final music-pack manifest.
