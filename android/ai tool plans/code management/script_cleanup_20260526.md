# Android script root cleanup plan

## Goal
Keep only the requested Android entry-point scripts in android/ and move supporting scripts to android/helpers/

## Requested root scripts
- run_all_tests.ps1
- 1_build-aab.ps1
- run-code-quality.ps1
- Run-Emulator.ps1
- Run-TestMenu.ps1
- 0_upload_to_test.ps1
- 2_deploy-playstore.ps1
- install-aab.ps1
- run_quick_tests.ps1

## Work items
- [x] Inventory script-like files directly under android/
- [x] Assess references to non-root scripts and identify obsolete shell wrappers
- [x] Delete obsolete shell wrappers when no live references remain
- [x] Move supporting scripts to android/helpers/
- [x] Update references to moved scripts
- [x] Run targeted validation and code quality checks
- [x] Mark completed plan items

## Results
- Root android/ script-like files are now the requested PowerShell entry points plus Gradle wrapper files and config.
- Deleted obsolete unreferenced helpers: android/1_build-aab.sh, android/run_test_menu.sh, and android/udp_relay.py.
- Moved supporting PowerShell and shell utilities to android/helpers/ and updated active callers, tests, game-data tools, get_deps scripts, and docs/comments.

## Validation
- android/helpers/stop-stale-formatters.ps1: no stale formatter tasks found before validation.
- PowerShell parser check passed for 228 files.
- bash -n syntax check passed for Android shell scripts.
- android/run-code-quality.ps1 -Fix passed on the touched script, test, game script, doc, and helper file set.
