# Remove manual dependency review demands 2026-08-22

## Plan

- [x] Review the pending `tool_versions.conf` and dependency checker diffs
- [x] Remove manual-target review output while retaining useful automatic checks
- [x] Update focused regression coverage to match the simpler contract
- [x] Run scoped dependency-check tests and code quality
- [x] Record the final behavior

## Result

- Hash-coupled Chromaprint, minimp3, stb_vorbis, and dr_flac target updates now download and hash every new payload before writing each complete dependency group to `tool_versions.conf`.
- Play Games 22 remains unavailable while `MIN_SDK=23` and is displayed as `held-minSdk23`, without requesting manual work.
- The obsolete manual-target review report and its metadata were removed. The already pinned 7-Zip row is displayed as `pinned` if a newer target appears.
- Applied all four currently available hash-coupled target updates so the pending `tool_versions.conf` diff contains their commits, URLs, and calculated SHA-256 values.

## Verification

- Live no-prompt updater run listed the four native updates and Play Games as `held-minSdk23`, with no manual-review section.
- Live automatic target run updated all four dependency groups successfully.
- The generated fpcalc v1.6.1 release URL returned HTTP 200.
- `test_download_verification.ps1`: pass.
- `test_get_deps_runtime_updates.ps1`: pass.
- Scoped `run-code-quality.ps1 -Fix`: pass.
- `git diff --check`: pass.
