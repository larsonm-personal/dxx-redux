# PowerShell stable-only dependency tracking

## Goal
Make the Android dependency updater track stable PowerShell releases only, and
move the pinned repo PowerShell runtime from the preview build to the current
stable release.

## Plan
- [x] Review the pasted `check-updates.ps1` log.
- [x] Verify the current stable PowerShell release from official release
  sources.
- [x] Update prerelease detection so `preview` tags are excluded.
- [x] Make PowerShell target selection prefer stable over a currently pinned
  prerelease even when the prerelease has a numerically higher version.
- [x] Pin `tool_versions.conf` to PowerShell 7.6.3.
- [x] Run focused validation and update this plan.

## Validation
- `pwsh -NoProfile -Command '$null = [scriptblock]::Create(...)'`
- `pwsh -NoProfile -File android/get_deps/update-powershell.ps1 -StatusOnly`
- `pwsh -NoProfile -File android/get_deps/check-updates.ps1 -NoPrompt`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/check-updates.ps1`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/update-powershell.ps1`
