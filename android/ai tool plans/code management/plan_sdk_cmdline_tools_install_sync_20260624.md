# SDK command-line tools install sync

## Goal
Integrate Android SDK command-line tools installation into `check-updates.ps1`
when that target upgrade is selected, so the script does not tell the user to
run `helpers/get_sdk.sh` as a follow-up.

## Plan
- [x] Inspect the existing update and install-sync flow.
- [x] Patch `check-updates.ps1` to invoke the configured SDK install helper
  after updating the pinned SDK command-line tools URL.
- [x] Patch `helpers/get_sdk.sh` so it refreshes `cmdline-tools/latest` when
  the pinned command-line tools build changes.
- [x] Run focused script syntax and lint checks.

## Validation
- `pwsh -NoProfile -Command '$null = [scriptblock]::Create(...)'`
- `bash -n android/get_deps/helpers/get_sdk.sh`
- `shellcheck -e SC1091 android/get_deps/helpers/get_sdk.sh`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/check-updates.ps1`
- `git diff --check`
