# Release Upload Build Stamp Plan

Status: implemented and verified

## Goal
- At the end of `android/0_upload_to_test.ps1`, print the build date stamp that was uploaded.
- The printed value should match the date shown in the app's About tab.

## Plan
1. Inspect the upload script and generated build info source. Done.
2. Add a small post-upload print that reads the generated build stamp from the same source used by the app. Done.
3. Run PowerShell lint/format checks for the touched script. Done.

## Verification
- `android\run-code-quality.ps1 -Fix` passed for `android\0_upload_to_test.ps1` and this plan.
- `git diff --check` passed for the touched files.
