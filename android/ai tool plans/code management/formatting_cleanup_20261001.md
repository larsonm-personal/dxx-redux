# Formatting cleanup

- Inventory existing runners and languages
- Select only files added since the merge base with upstream/main, plus new untracked files; exclude d1/, d2/, generated artifacts and upstream SDL patch copies
- Share selection across direct runners and the mixed-language entry point; add preview, changed-file and tool selection modes
- Extend coverage to Python, Rust, Java/C#, JSON/JSONC, YAML, Markdown, XML/SVG and HTML with pinned dependencies
- Run the full eligible fix pass, then the check pass and formatter integration tests
- Verify inherited files remain byte-identical and inspect formatting changes for behavior changes; run relevant native builds and tests

Existing worktree edits belong to the user and must be preserved

## Completed

- Shared Git-based selection protects every inherited baseline path, including a file removed from the index and recreated as untracked
- Added preview, changed-file, baseline and tool selection modes; direct helpers reject empty scopes and skip unavailable tools when their language has no eligible files
- Added pinned Ruff/rustfmt and locked Prettier/XML dependencies, installation and reusable Node path discovery; documented usage in `android/CODE_QUALITY.md`
- Expanded existing native, Kotlin, PowerShell, Bash and CMake coverage and corrected formatter failure propagation and the native CMake check flag
- Resolved PowerShell compatibility lint through shared JSON helpers, stable grouping and guarded type lookup; documented intentional test-mock suppressions
- Updated stale source/documentation assertions and the sound integration fixture exposed by the broader verification

## Validation

- Full language checks passed except live ledger formatting changed by concurrent work; the subsequent mixed-language scoped check passed for those ledgers and the final edited files
- Formatter selection/dispatch and interrupted-process cleanup tests passed, including inherited paths, invalid/empty scopes, staged removal, checkout isolation and new formatter processes
- PowerShell 7/5.1 helper parity and parsing of 458 PowerShell files passed; guidebot browser and reporting tests passed
- D1 native build and 54 CTest cases passed; D2 native build and 63 CTest cases passed
- Python suite: 250 tests, no failures/errors, six skips; the converter-specific `test_hmp_playback.py` requires its separate executable argument and was outside this discovery run
- JVM unit tests: 1,102 tests, no failures/errors, one skip
- `git diff --check` passed
- Snapshot audit retained concurrent graphics edits in four protected engine files (`ogl.c` and `newmenu.c` in each game); the other 939 protected hashes matched. No formatter selected any `d1/` or `d2/` file
- Python AST changes were limited to the three intentional fixture/assertion updates. Other native-token and JSON-value differences in the snapshot audit belonged to concurrent graphics work and were preserved

Validation logs and before/after snapshots are under ignored `temp/format*`; formatter stage summaries are under `android/temp/run-code-quality.*.json`
