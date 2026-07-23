# Fix whole-library CD regression workflow

## Goal

Make forced CD extraction robust for zero or one matching source file, keep
generated regression specs stable when their semantics have not changed, and
reject empty extraction oracles instead of reporting vacuous success

## Plan

- [x] Create the implementation plan
- [x] Trace extraction discovery, canonical spec writing, and expected-file verification
- [x] Normalize source discovery results to arrays in the CD batch extractor
- [x] Make generated comments content-stable without hiding meaningful spec changes
- [x] Reject empty expected-file oracles with an actionable failure
- [x] Add focused PowerShell regression coverage for all three behaviors
- [x] Run scoped code quality and focused workflow tests
- [x] Update the plan with final validation results

## Validation

- The repaired host batch processed the single-CUE `d1 mac 2nd bin+cue` source, extracted eight files, and restored fourteen track hashes
- Forced spec generation reclassified that source as D1 full with nonempty HOG and PIG oracles
- Both non-launchable level-library specs now use their complete extracted filename sets as oracles
- The isolated workflow test proves unchanged semantic content is byte-stable, meaningful changes update content and the generated time, and empty oracles fail validation
- The library validator passed all 34 CD regression specs after regeneration
- The complete native extraction build and all 13 CTest suites passed
- Scoped code quality and `git diff --check` passed
- The user's existing all-library emulator run remained active and was not interrupted; its per-disc result changes were preserved
