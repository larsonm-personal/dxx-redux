# Check Updates Unknowns And PowerShell

## Goal
Improve android/get_deps/check-updates.ps1 so more of the current ??? rows are
detected reliably, and add a check for PowerShell 7.x.

## Status
- [x] Diagnose the current ??? rows in noninteractive mode
- [x] Patch the broken or missing upstream version helpers
- [x] Add a PowerShell 7.x dependency check
- [x] Re-run a safe updater validation
- [x] Mark this plan complete