# PowerShell MSI system update path

## Goal
Make `check-updates.ps1` and `update-powershell.ps1` update the ordinary
Windows PowerShell 7 install that fresh terminals launch, instead of only
refreshing the repo-local ZIP runtime.

## Plan
- [x] Review the fresh-window report and current helper behavior.
- [x] Check Microsoft's documented Windows install/update options for
  PowerShell 7.
- [x] Change PowerShell installed-version detection to report the active/path
  host before any repo-local ZIP install.
- [x] Change install sync to run the system update helper.
- [x] Add a UAC-elevated MSI install path using the pinned stable MSI URL.
- [x] Validate with status checks and script quality.

## Validation
- `pwsh -NoProfile -Command '$null = [scriptblock]::Create(...)'`
- `pwsh -NoProfile -File android/get_deps/update-powershell.ps1 -StatusOnly`
- `pwsh -NoProfile -File android/get_deps/check-updates.ps1 -NoPrompt`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/update-powershell.ps1`
- `android/run-code-quality.ps1 -Fix -Paths android/get_deps/check-updates.ps1`
