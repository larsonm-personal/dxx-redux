# GuideBot regression stage failures

- Inspect the September 25 batch evidence and reproduce level-load failures and simulation timeouts
- Fix the underlying engine or runner defects while preserving existing workspace edits
- Build the affected targets, run focused regression coverage, and rerun the failed batch cases
- Run scoped code quality and record the results

## Findings and changes

- Original batch: 2639 levels, 13 load failures across Descend Again and Orion variants, and two process watchdog timeouts in af_d1_beta
- D1 trigger decoding rejected editor garbage that previous loading tolerated: unknown flags, inert records with invalid counts, high-byte garbage in otherwise bounded counts, and an invalid matcen segment link
- Added level-only repair before strict trigger decoding, preserving strict save translation and rejecting unbounded active link counts
- Both af_d1_beta hangs occurred on the first frame while animating an unavailable vclip (81) with zero duration
- Restored native D1's unavailable object-vclip fallback in D1-in-D2 level loading, also checking empty and zero-duration clips
- No runner timeout increases or changes to failure classification

## Validation

- D2 Windows build and scoped code quality passed
- All 61 D2 native CTest tests passed; the affected upstream compatibility test passed again after the final Orion count repair
- Focused repeat run: all 15 original failures plus one matching collection level completed twice, with 16 identical result pairs and zero infrastructure failures
- af_d1_beta levels 2 and 4 now return durable semantic route-stall results instead of hanging
- All 13 original load-error cases now confirm their routes
- Full 2639-level GuideBot regeneration passed with exit 0 and zero infrastructure failures in 661.455 seconds; checked-in simulation JSON was refreshed
- Full report: temp/guidebot-full-verified/summary.json
- Repeated-run report: temp/guidebot-repeat-verified/summary.json
