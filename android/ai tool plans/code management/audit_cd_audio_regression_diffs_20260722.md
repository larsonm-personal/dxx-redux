# Audit CD and audio regression diffs

## Goal

Classify current generated CD extraction and audio regression changes as stable
successful updates or failures requiring investigation and remediation.

## Plan

- [x] Create the audit plan
- [x] Inventory changed regression files and separate authored workflow changes
- [x] Parse semantic changes, failure markers, file counts, sizes, and hashes
- [x] Check repeated-run stability evidence and generator completeness
- [x] Trace suspicious changes to extraction behavior or source availability
- [x] Run focused validation where it will not overwrite user results
- [x] Report commit-safe files and files requiring investigation

## Findings

- All 34 `track_hashes.json` files were regenerated on 2026-07-22 and remain
  byte-identical to Git, so no raw data-track or audio-track oracle changed
- None of the changed extraction specs records hashes of extracted files;
  `expected_files` validates names only
- Twenty-five changed specs are successful file-only result metadata with
  unchanged extraction oracles
- The Levels of the World and Dimensions specs gained deterministic sorted
  nonempty filename oracles, but neither new oracle has a successful validating
  run in the current results
- The d1 Mac second image records an invalid 0-of-2 pass from the run that began
  before its regenerated oracle was available
- Vertigo settled with only 3 of 4 expected names and needs a focused rerun and
  missing-file diagnosis
- Seven consecutive `setup_timeout` results began after the emulator became too
  slow to initialize SetupActivity; the live emulator still remained on its
  splash screen without producing setup introspection, so these are test
  infrastructure failures rather than extraction-byte results
- The workflow self-test passed, and all 34 CD regression specs passed structural
  validation
