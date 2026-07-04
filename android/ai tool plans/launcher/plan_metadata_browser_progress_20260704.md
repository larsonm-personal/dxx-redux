# Metadata browser progress plan

## Goal
Add visible progress feedback when opening slow mod metadata entries such as `kcxf2rm.mn2`, and improve existing metadata loading spinners with best-effort progress bars.

## Steps
- [x] Create this plan.
- [x] Trace the mod metadata browser detail-loading path and existing metadata analysis progress UI.
- [x] Add minimally detailed progress state for descriptor/detail loading.
- [x] Surface spinner plus best-effort progress bars in the relevant metadata loading UI.
- [x] Add or update focused tests where practical.
- [x] Run scoped code quality and relevant tests/builds.

## Notes
- Existing unrelated local edits are present. Avoid touching them unless directly needed.
- `MissionZipConstituentDialog` currently computes `GameFileMetadata.summarizeZipConstituent` in `remember`,
  which can block the first dialog frame. Move it to async state before adding the progress UI.
- `LevelMetadataAnalyzer` already runs on `Dispatchers.IO`, but it has only a spinner in the dialog.
- Added shared `MetadataLoadProgress` formatting tests before validation.
- Validation: scoped `android\run-code-quality.ps1 -Fix -Paths ...` passed; targeted
  `:app:testDebugUnitTest --tests com.dxxredux.app.MetadataLoadProgressTest` passed.
