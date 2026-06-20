# Large mission archive extraction implementation

## Goal
- Implement durable extraction for mission archives larger than 10 MB while preserving mod enable/order behavior

## Plan
- [x] Add threshold-based extracted-bundle decision and tests
- [x] Extend extraction manifest records with size/provenance details
- [x] Route launch and metadata details through fresh extracted records
- [x] Add metadata UI notes for extracted storage
- [x] Cover nested Rebirth ZIP and cleanup behavior
- [x] Run scoped tests, assemble, and code quality

## Progress
- Large mission ZIP threshold is now 10 MB
- Fresh extracted manifests are used before opening the source archive for launch path generation, mission details, mission soundtrack checks, and music catalog display
- Readme viewing and external document viewing now prefer extracted files before falling back to archive reads
- Extraction manifest writes v2 provenance fields while still reading older records
- Metadata details now show extracted cache path, source archive name, archive format, file count, and extracted size
- Added a regression test that corrupts the stored archive after import and verifies launch paths and details still come from the extracted record
- Added a regression test that imports an Enemy Within parent ZIP whose selected Rebirth child crosses the threshold and verifies durable extraction and mount order
- Verification passed: focused mission ZIP/parser/music tests, scoped code quality, and assembleDebug

