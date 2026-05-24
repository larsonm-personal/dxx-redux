# Add PowerShell Shebangs 2026-05-24

## Goal

Make every `.ps1` in the repo directly runnable on Unix-like hosts by standardizing on `#!/usr/bin/env pwsh` and setting the executable bit.

## Steps

- [x] Inventory all `.ps1` files and detect existing shebang coverage
- [x] Check for BOM or CRLF shebang edge cases that would break direct execution
- [x] Add or normalize the `pwsh` shebang across the repo
- [x] Set the executable bit on every `.ps1`
- [x] Revalidate shebang coverage, BOM cleanup, and executable bits

## Result

- Updated 74 PowerShell files that were missing the shebang or carried a UTF-8 BOM that would block Unix direct execution
- Verified all 103 `.ps1` files now start with `#!/usr/bin/env pwsh`
- Verified there are no remaining UTF-8 BOM-prefixed `.ps1` files and no non-executable `.ps1` files

## BOM Follow-up

- [x] Audit tracked source files for remaining UTF-8 BOM usage after the shebang pass
- [x] Convert tracked BOM files to plain ASCII where the file content allows it
- [x] Recheck tracked files for BOM-free ASCII content and confirm the shebang stays plain byte-0 text

## BOM Follow-up Result

- Normalized 42 tracked UTF-8 BOM files to plain ASCII
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` needed punctuation cleanup in addition to BOM removal; the remaining tracked BOM files were ASCII once the BOM bytes were removed
- Revalidated that tracked BOM count is zero and all tracked `.ps1` files still begin with `#!/usr/bin/env pwsh` at byte 0

## Lint Follow-up

- [x] Find the main repo-wide linter entrypoint and confirm where the extra pass belongs
- [x] Add a BOM lint pass to `android/run-code-quality.ps1`
- [x] Add a short note to `.github/copilot-instructions.md` that UTF-8 BOM files should be avoided
- [x] Run a focused validation pass for the updated linter flow

## Lint Follow-up Result

- Added a tracked/scoped UTF-8 BOM pass to `android/run-code-quality.ps1`; check mode now fails on BOM-prefixed files and `-Fix` strips the BOM bytes
- Added a short repository note in `.github/copilot-instructions.md` to avoid utf8-with-bom files
- Validated with a temp BOM file: check mode returned failure, `-Fix` removed the BOM, and a recheck passed