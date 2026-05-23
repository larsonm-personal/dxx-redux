# Check Updates Unknown Sources And PowerShell

## Goal
Fix android/get_deps/check-updates.ps1 so the remaining unknown dependency rows
resolve reliably without depending on brittle or rate-limited sources, add a
PowerShell 7.x version check, and make future validation runs safer than the
live integrated-terminal path.

## Status
- [ ] Identify why the remaining dependencies show ???
- [ ] Add safer source fallbacks and noninteractive validation path
- [ ] Add PowerShell 7.x version check
- [ ] Re-run focused validation for the unknown rows
- [ ] Mark this plan complete