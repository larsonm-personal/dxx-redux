# Last two batch failures

## Goal

Investigate the two failures in the attached batch report, distinguish product
regressions from state or infrastructure defects, and apply narrow fixes with
ordering-sensitive validation.

## Plan

- [x] Create the investigation plan
- [x] Read project instructions and the attached report
- [x] Inspect detailed failure logs and immediate predecessors
- [x] Correlate failures with current source and recent changes
- [x] Reproduce each failure with relevant prior state
- [x] Implement narrow fixes or record evidence-based disposition
- [x] Run affected regression tests and scoped quality checks
- [x] Record final outcomes

## Outcomes

- `test_gradle_unit_tests`: `File.renameTo` transiently failed while publishing
  a new cache directory with no previous target. The replacement helper then
  incorrectly entered its old-generation backup branch. Publication now
  distinguishes creation from replacement and gives atomic renames a short,
  bounded retry for transient filesystem locks.
- `test_gog_installer_redbook_unified`: the shared 30-second `Adb` bound killed
  the 563 MB installer push. The wrapper ignored the empty timeout result,
  printed `Push complete`, and accepted the truncated file based on existence.
  GOG wrappers now share a size-derived push bound and verify both size and
  SHA-256 before starting the import.
- Validation passed: scoped code quality; two complete JVM unit-test runs (the
  second forced with `--rerun-tasks`); and D1 GOG followed immediately by D2
  GOG/redbook on the same emulator and app data.
