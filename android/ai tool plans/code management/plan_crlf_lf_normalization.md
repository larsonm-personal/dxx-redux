# CRLF -> LF Normalization Plan

## Problem
1. `.gitattributes` has rules in wrong order -- `*.bat` is overridden by `*`
2. `Write-TestResult` in `test_extract.ps1` writes CRLF via here-strings and `Set-Content`
3. `ConvertTo-Json` on Windows outputs CRLF -- affects `fingerprint_disc_tracks.ps1` and others
4. Many files committed with CRLF before `.gitattributes` existed, never renormalized

## Root causes
- PowerShell here-strings (`@"..."@`) use platform line endings (CRLF on Windows)
- `ConvertTo-Json` uses `[Environment]::NewLine` internally (CRLF on Windows)
- `Set-Content` writes whatever line endings are in the string
- `.gitattributes` order: last match wins, so `* text=auto eol=lf` overrides `*.bat text eol=crlf`

## Fixes
1. **`.gitattributes`**: swap order so `*.bat` comes after `*`
2. **`test_extract.ps1:Write-TestResult`**: normalize CRLF->LF before writing
3. **`fingerprint_disc_tracks.ps1`**: normalize ConvertTo-Json output to LF
4. **Other scripts**: audit and fix all file-writing in `game_data/*.ps1` and `android/**/*.ps1`
5. **Renormalize**: `git add --renormalize android/ game_data/ server/` to fix committed files

## Status
- [x] Plan created
- [ ] Fix .gitattributes
- [ ] Fix script file-writing code
- [ ] Renormalize git index
