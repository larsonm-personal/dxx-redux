# check-updates Windows PowerShell install

## Goal
- Allow `android/get_deps/check-updates.ps1` install sync actions to update the pinned PowerShell host tool on Windows

## Plan
- [x] Inspect the existing PowerShell dependency row and install helper flow
- [x] Add a Windows-capable PowerShell install helper or extend the existing helper without breaking Linux
- [x] Wire `check-updates.ps1` so Windows install sync uses the helper
- [x] Run scoped formatting/linting and a no-prompt smoke check

## Validation
- `pwsh -NoProfile -NonInteractive -Command ... [scriptblock]::Create(...)`: parse ok
- `pwsh -NoProfile -ExecutionPolicy Bypass -File .\android\run-code-quality.ps1 -Fix -Paths ...`: passed for touched PowerShell files
- `pwsh -NoProfile -ExecutionPolicy Bypass -File .\android\get_deps\check-updates.ps1 -NoPrompt`: listed PowerShell 7 as a Windows install-sync action
