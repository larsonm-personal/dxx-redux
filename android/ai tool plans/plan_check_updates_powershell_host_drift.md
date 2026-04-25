# Check Updates PowerShell Host Drift

## Goal
Clarify PowerShell 7 host-tool drift in android/get_deps/check-updates.ps1 so
manual host updates are reported cleanly and do not end with a misleading
"everything is up to date" summary.

## Status
- [x] Mark PowerShell 7 as a host-managed drift case in the dependency table
- [x] Fix the final summary so manual drift does not report everything up to date
- [x] Re-run safe no-prompt validation and confirm the new wording

## Notes
- PowerShell 7 is target-tracked and installed-version-detected, but remains a manual host-tool update because there is no PowerShell install helper in the updater flow
- Detection uses the pwsh resolved on the current PATH, so after a manual PowerShell update an already-open terminal may continue reporting the old version until its PATH is refreshed or the terminal is reopened