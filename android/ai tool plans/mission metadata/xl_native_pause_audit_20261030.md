# Native XL persistence-fixture pause audit

The seismic integration exposed native fixtures that call `start_time()` after
`state_save_all_sub()` has already consumed the caller's pause. Other saves omit
the required preceding `stop_time()`. Native release builds can then continue
with negative pause depth. Directly called gameplay consumers may still produce
passing assertions while pause-aware frame paths remain inert.

Keep this correction in the XL test harness. Do not alter production Redux or
Android pause behavior, remove persistence controls, or compensate by forcing a
paused frame to run.

1. Add a running-state assertion to an existing affected original-layout fixture
   and record its failure before changing the pause calls.
2. Inspect every native XL fixture save/restore call. Remove redundant resumes,
   add missing caller pauses, and assert running state at continuation boundaries.
   Preserve intentional paused-state controls and their matching operations.
3. Build both native hosts and run the required XL acceptance selection. Diagnose
   any newly exposed behavior rather than weakening the assertions. Existing
   Android gameplay qualification remains distinct because this is native-only
   harness code.
4. Record the reproduced defect, corrected sites and complete-run results; keep
   the follow-up PR separate from seismic implementation and preserve existing
   CI membership and timeout bounds.

This audit does not establish new mission support. Its purpose is to make later
gameplay verification depend on valid simulation state throughout persistence
tests.

The negative control reproduced the defect in the existing 15/34 fixture, and
the corrected control passes on Windows. The implementation replaces 41
redundant resumes and supplies four missing caller pauses across 30 native test
headers. Both host builds and all 106 required Linux acceptance tests pass,
including sixteen original-format witnesses and three modern source levels.
Maintained
results are in [the pause evidence](../../XL_compat/evidence/native-persistence-pause.md).
