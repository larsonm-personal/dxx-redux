# check-updates first-pass install sync plan

## Goal
Fix `android/get_deps/check-updates.ps1` so a dependency selected for a target update can also run its configured install helper in the same pass when the installed version is behind the new target

## Steps
- [x] Reproduce the offer-list logic from the attached two-pass 7-Zip log
- [x] Update the selection/install path so target updates can trigger same-pass install sync
- [x] Add or run a focused verification that covers the 7-Zip-style target update plus install case
- [x] Run scoped PowerShell code quality on changed script files

## Verification
- Parsed `android/get_deps/check-updates.ps1` with the PowerShell AST parser
- Extracted the patched target-install selection block and verified:
  - `Install sync: a` adds a selected 7-Zip target update when installed is `2601` and latest is `2602`
  - blank install selection does not add the target update
  - already-installed latest target updates are skipped
- Ran `.\android\run-code-quality.ps1 -Fix -Paths .\android\get_deps\check-updates.ps1`
