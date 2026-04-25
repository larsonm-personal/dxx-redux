# Check Updates Stable Version Filtering

## Goal
Fix android/get_deps/check-updates.ps1 so it stops offering prerelease
alpha/beta/rc upgrades by default, resolves clang-format from the highest
stable Windows asset version, and avoids VS Code terminal lockups while it
fans out across many web requests.

## Status
- [x] Patch prerelease filtering in version source helpers
- [x] Fix clang-format asset version selection
- [x] Suppress per-request PowerShell progress output to avoid terminal lockups
- [x] Run focused validation on affected dependency rows
- [x] Mark this plan complete