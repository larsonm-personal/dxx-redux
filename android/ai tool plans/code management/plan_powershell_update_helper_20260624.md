# PowerShell update helper

## Goal
Add a discoverable helper for the PowerShell update case shown in the pasted
logs, where the active terminal reports a stable update but `check-updates.ps1`
reports the repo-pinned PowerShell runtime as current.

## Plan
- [x] Inspect the pasted log and existing PowerShell dependency helpers.
- [x] Add a top-level `android/get_deps` helper that explains active vs pinned
  PowerShell and updates the requested one.
- [x] Point dependency checker hints at the new helper.
- [x] Run focused validation and update this plan.

## Validation
- `pwsh -NoProfile -Command '$null = [scriptblock]::Create(...)'`
- `pwsh -NoProfile -File android/get_deps/update-powershell.ps1 -StatusOnly`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/update-powershell.ps1`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/check-updates.ps1`
