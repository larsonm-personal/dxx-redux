# Automap Translation Speed

## Goal
Reduce Android automap translation speed from 4x to 3x in both D1 and D2.

## Plan
- [x] Locate the duplicated automap movement scaling in D1 and D2.
- [x] Reduce the shared Android automap translation scale from 4x to 3x in both copies.
- [x] Run a focused verification pass for the changed source.

## Notes
- `android/outstanding_bugs.md` says not to edit that file, so leave the checklist unchanged.
- Verification: `.\run-windows-build.ps1 -Target both` passed for D1 and D2.
- Verification: bundled CTest ran for `buildd1` and `buildd2`; both build dirs reported no tests.
