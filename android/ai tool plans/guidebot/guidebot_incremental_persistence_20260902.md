# GuideBot incremental corpus persistence

## Plan

- [x] Change normal corpus runs from two repetitions to one
- [x] Preserve an explicit repeated-run path for focused determinism tests
- [x] Persist each completed level into its mission simulation JSON immediately
- [x] Preserve unselected and not-yet-run level records during incremental writes
- [x] Add regression coverage for the default repeat count and incremental write ordering
- [x] Run scoped formatting and relevant tests
