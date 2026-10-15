# Classic demo dump wall texture guards

## Goal
Complete public D2 classic-demo JSON conversion without applying texture events to unloaded geometry, while preserving live playback behavior and assertions

## Plan and outcome
- [x] Reproduce the valid public demo assertion in WALL_SET_TMAP_NUM2
- [x] Trace dump NEW_LEVEL handling: native D2 geometry is deliberately not loaded and initial wall records are consumed without application
- [x] Gate only WALL_SET_TMAP_NUM1 and WALL_SET_TMAP_NUM2 live side effects on dump mode; preserve decoding, rewriting and JSON output
- [x] Compile the changed translation unit with the native CMake compile command and relink the D2 executable
- [x] Convert the complete public demo: 554 frames, 4544 objects, truncated=false
- [x] Run existing test_classic_demo_dump_transaction.py: all 19 positive/negative subprocess cases pass
- [x] Check whitespace with git diff --check

The transaction suite covers source/output aliases, malformed headers, invalid and truncated initial wall records, preservation of existing output on failure, temporary-file cleanup, and successful conversion. No new test registration is needed

## Scope and limitations
The historical playback assertion is retained. Both modified conditions behave identically when dump mode is false. Event decoding and output code are unchanged. The baseline assertion predates the dump-only level-loading bypass; this is unrelated to save-clock changes

Adjacent wall-hit, toggle, trigger and effect handlers already gate live effects. Door-opening geometry access is backward-only and unreachable during forward dumping. Cloaking-wall and control-center trigger side effects remain follow-up audit candidates without an independent failure witness; this fix does not expand into parser redesign

Qualification used a focused native relink, not a fresh all-target build. No live playback or D1-in-D2 asset run was performed. PowerShell quality/catalog tools were not executed here; no new automation test was added
