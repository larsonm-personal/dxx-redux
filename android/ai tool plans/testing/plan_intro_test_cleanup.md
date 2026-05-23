# Intro test cleanup and consolidation

## Status

- Completed: confirmed the two intro scripts were not pure duplicates
- Completed: folded intro auto-skip pref coverage into `test_engine_prefs_unified.json5`
- Completed: deleted the standalone `test_intro_auto_skip_pref_unified.json5` script
- Completed: retained `test_intro_skip_inputs_unified.json5` because it is the only script that exercises touch and controller intro dismissal
- Verified: `./android/run-code-quality.ps1 -Fix`
- Verified: `test_engine_prefs_unified.json5` passed for D1 and D2 after the fold-in
- Verified: `test_intro_skip_inputs_unified.json5` passed for D1 and D2 after the cleanup

## Plan

1. Inspect the two intro scripts and identify overlap and distinct assertions
2. Survey nearby existing launcher or title scripts for a better fold-in target
3. Consolidate by deleting, merging, or folding into an existing script with minimal duplication
4. Re-run the affected tests and update this plan with the result