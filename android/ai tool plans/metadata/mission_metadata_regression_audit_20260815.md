# Mission metadata regression audit

Date: 2026-08-15
Status: complete

## Requests

- Diagnose the level-name regressions in HDVBETA2 and Outer Reach
- Diagnose the Phobos route regression from ok to partial
- Audit every currently modified mission metadata JSON for other degradations
- Fix underlying analyzer or regeneration causes rather than accepting worse data

## Plan

- [x] Compare all modified mission JSON files with HEAD using structured fields
- [x] Reproduce and trace the HDVBETA2, Outer Reach, and Phobos regressions
- [x] Classify other changed missions by improvement, neutral churn, or regression
- [x] Implement general fixes and add focused regression coverage
- [x] Regenerate affected and full-corpus metadata
- [x] Run route, native, Android, and quality validation

## Constraints

- Do not hard-code behavior for individual missions
- Preserve legitimate route improvements and unrelated working-tree changes
- Keep generated metadata normalized and free of replacement detail fields

## Findings

- The host analyzer serialized the engine's fixed-size `Current_level_name`
  directly, while Android preferred the complete name stored in the level file
  and rejected non-printable names. Both analyzers now use the shared chooser.
- The same name bug affected the three Lunar Series Revamped levels, which now
  use their filename stems instead of binary text.
- Phobos level 4 previously reached the wrong boss because stale base robot
  definitions identified robot 23 as a boss. Its HXM correctly identifies
  robot 20 in segment 369, behind a buddy-proof boundary. The route now leads
  the Guide-Bot to the player handoff point, records that it waits there, then
  continues through the boss objective to the exit.
- The apparent Ulterior result-count reduction removes duplicate analyses of
  byte-identical descriptors; it does not remove a distinct mission variant.
- Other boss/reactor substitutions are deterministic results of loading each
  level's custom robot definitions from a clean baseline.

## Validation

- Two complete host corpus regenerations: 115 sources, 114 passed, 1 skipped,
  0 failed
- Structured audit: no remaining name or route-status degradations in modified
  mission metadata compared with HEAD
- Reviewed route corpus: 1,509 levels
- D1 native tests: 33 passed
- D2 native tests: 40 passed
- Focused mission-name, route-corpus, JSON-normalization, and regression-tool
  contract tests passed
- Scoped code quality passed
