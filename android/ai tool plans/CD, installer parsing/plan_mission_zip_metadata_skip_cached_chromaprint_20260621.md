# Mission Zip Metadata Skip Cached Chromaprint

## Goal
- Stop the mission zip metadata music viewer from re-running local chromaprint analysis for tracks already analyzed during import.

## Steps
- [x] Confirm where the metadata viewer auto-runs local fingerprint analysis.
- [x] Change the auto-analysis pass to only process tracks missing cached fingerprint entries.
- [x] Add focused coverage and run scoped formatting/tests.
