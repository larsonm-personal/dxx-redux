# Android duplicate root script cleanup plan

## Goal
Remove fresh duplicate support scripts that reappeared under android/ after the helper relocation, while preserving any meaningful newer content in android/helpers/ first.

## Work items
- [x] Inventory files that exist in both android/ and android/helpers/
- [x] Compare duplicate root copies against helper copies
- [x] Preserve any meaningful newer root content in helper copies
- [x] Delete duplicate root copies that should not remain
- [x] Verify root script list and active references
- [x] Run targeted validation
- [x] Record final results

## Results
- Found duplicate root copies for the relocated helper scripts.
- Most root copies were byte-for-byte identical to the android/helpers/ copies.
- The only differing root copies were emu_health.ps1, run_automation.sh, run_cue_iso_tests.sh, run_test.ps1, and test_helpers.ps1. Their differences were stale usage comments; the helper copies already had the corrected android/helpers/ paths.
- No meaningful newer root content needed to be preserved.
- Removed the duplicate root copies and kept android/helpers/ as the source of truth.
- Updated stale usage comments in android/helpers/collect_crash.ps1 and android/helpers/diff_vs_upstream.ps1.

## Validation
- Duplicate root/helper inventory now reports no duplicate files.
- Root script search reports only the requested entry points plus gradlew.bat.
- PowerShell parser check passed for 73 root, helper, and test scripts.
- git diff --check passed for this plan file.
- Stale tracked-reference search reports no active references to old android/ helper paths.