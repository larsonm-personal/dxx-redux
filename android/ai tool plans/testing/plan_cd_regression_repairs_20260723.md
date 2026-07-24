# CD Regression Repairs

## Goal

Repair the CD regression workflow and the product paths exposed by the
2026-07-23 full run without accepting weakened generated oracles.

## Plan

- [x] Separate normal regression comparison from explicit oracle refresh
- [x] Preserve stronger full-launch evidence across file-only runs
- [x] Add explicit setup import state and remove stable-file completion races
- [x] Support nested custom mission files from level-pack discs
- [x] Correct direct-import failure classification and timeout diagnostics
- [x] Fix the Mac HFS to STi2 extraction regression
- [x] Add or extend focused host, Kotlin, and PowerShell tests
- [x] Run scoped formatting, builds, and focused regression tests
- [x] Reconcile generated run evidence only after the fixed paths pass

## Constraints

- Preserve unrelated user changes, including `.vscode/settings.json`
- Do not accept regenerated `d2 mac` expectations
- Do not let file-only evidence replace an existing full-launch pass
- Do not use a stable partial file list as an import completion signal
- Keep extraction safety limits while avoiding whole-installer buffering

## Implemented

- Normal `run_all_cd_regressions.ps1` runs no longer regenerate specs.
  `-RefreshOracle` is required for that maintenance operation.
- File-only results cannot replace an existing full result.
- CD and ISO setup imports expose `idle`, `running`, `complete`, and `failed`
  state through setup introspection.
- The extraction runner waits for explicit completion and reports root count,
  recursive count, missing expected files, result count, and import error.
- Nested recognized game and mission files use the centralized
  `GameFileFormats` registry and are hoisted into the active set.
- Direct import failures use `import_failed`, `import_incomplete`, and
  `import_timeout` instead of `file_push_failed`.
- Preview-specific unsupported launch handling runs before the generic
  `-SkipLaunch` pass.
- Mac HFS extraction recognizes `Install Descent`, `Install Descent 2`, and
  `Install Descent II`, merges non-duplicate loose HFS files, and maps the
  temporary STi2 archive read-only rather than loading it into one heap buffer.
- The 36 generated spec changes from the failed run were restored from the
  committed oracle after focused verification.

## Verification

- PowerShell CD runner tests passed.
- PowerShell extraction workflow tests passed.
- `DiscImportHoistTest` passed under `testDebugUnitTest`.
- Android debug APK assembled successfully for all configured ABIs.
- HFS host tests passed: 9/9.
- STi2 host tests passed: 10/10, including the real D2 Mac oracle.
- Focused D2 Mac extraction produced 17 unique recognized files.
- Levels of the World passed on-device with 191/191 expected files.
- Dimensions for Descent passed on-device with 88/88 expected files.
- Destination Quartzon USA waited for completion and passed with 3/3 expected
  files instead of prematurely recording zero.
