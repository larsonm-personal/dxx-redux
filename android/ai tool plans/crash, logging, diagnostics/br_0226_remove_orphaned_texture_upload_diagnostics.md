# BR-0226 orphaned texture-upload diagnostics

## Goal

Remove the unreachable texture-upload diagnostic API and its private helper chain without affecting the remaining named-texture diagnostics.

## Plan

- [x] Confirm the three public diagnostics have no live callers and identify their complete private dependency chain
- [x] Remove only the orphaned declarations, definitions, and support code
- [x] Run symbol searches, scoped code quality, paired D1/D2 native tests, and Android debug assembly
- [x] Record validation and archive BR-0226 through the adversarial review ledger process

## Validation

- Symbol-exact searches found no remaining upload-source, expanded-pixel, mip-upload, private-helper, or advertised log-tag references in the production Android, D1, and D2 trees
- Scoped code quality and `git diff --check` passed
- D1 and D2 Windows host builds and all registered native maths CTests passed
- Android debug assembly passed for arm64-v8a, armeabi-v7a, and x86_64
