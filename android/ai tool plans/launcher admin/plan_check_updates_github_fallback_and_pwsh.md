# Check Updates GitHub Fallback And Pwsh

## Goal
Fix android/get_deps/check-updates.ps1 so GitHub-backed version checks keep
working after GitHub API rate limiting, sanity check the remaining non-??? rows,
and add a PowerShell 7 version check.

## Status
- [ ] Replace GitHub API-only helpers with public fallback paths
- [ ] Restore the current ??? GitHub-backed rows
- [ ] Add PowerShell 7 version reporting
- [ ] Run a focused noninteractive validation
- [ ] Mark this plan complete