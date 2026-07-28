# BR-0020 fail incomplete requested archive imports

## Goal

Make archive imports fail whenever any requested file cannot be extracted,
preventing partial extraction from being reported or published as successful.

## Plan

- [x] Read repository instructions and the complete BR-0020 finding
- [x] Compare frozen and live extraction loops, callers, and publication paths
- [x] Define complete-success accounting for every affected archive format
- [x] Implement fail-closed extraction and add partial-failure regressions
- [x] Run scoped code quality, focused and native tests, and Android ABI builds
- [x] Finalize BR-0020 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality passed for all modified native, Kotlin, and Android
  PowerShell files
- All 15 native extraction suites passed
- `CueDataTrackExtractionTest` passed
- Transactional publication and mission HOG truncation PowerShell tests passed
- Android debug assembly passed for arm64-v8a, armeabi-v7a, and x86_64
