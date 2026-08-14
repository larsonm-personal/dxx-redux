# GQR-0065 Inno Version Admission Plan

## Goal

Close `GQF-0078` by removing signed weighted-version arithmetic from Inno admission and every schema-selection path while preserving the supported 5.3.0 through 5.6.99 policy

## Work

- [x] Inventory production and test uses of `INNO_VER` and inspect concurrent Inno memory-accounting changes
- [x] Replace encoded integer comparisons with one overflow-free tuple comparator and direct supported-range admission
- [x] Add parser and schema boundary coverage for `INT_MAX`, supported endpoints, adjacent unsupported tuples, and malformed components
- [x] Run focused MSVC tests, Android D1/D2 ABI object builds where feasible, scoped quality checks, diff checks, and ASCII checks
- [x] Report exact validation results and remaining blockers without editing the canonical ledger or campaign plan

## Constraints

- Keep all changes under the branch-authored Android extraction implementation and its focused test
- Preserve the concurrent peak-live-memory accounting implementation
- Do not modify inherited D1 or D2 sources
- Do not edit the canonical quality ledger or root remediation campaign plan

## Result

- Removed the weighted `major * 10000 + minor * 100 + patch` representation and use one lexicographic tuple comparator for every checksum, header, file-entry, and data-entry layout decision
- The archive gate now admits only tuples from 5.3.0 through 5.6.99 by direct component checks before metadata decompression or parsing
- Focused native tests cover the supported minimum and maximum, minor and patch values one step outside, all three components at `INT_MAX`, an all-`INT_MAX` schema comparison, malformed signed and whitespace components, and exact component overflow rejection
- MSVC built `test_gog_fd` and `gog_fd_tests` passed with both maintained real installer fixtures
- The maintained version-gate documentation oracle passed after replacing its stale encoded-integer source assertion
- `inno_reader.c` compiled for both D1 and D2 Android objects on `arm64-v8a`, `armeabi-v7a`, and `x86_64`
- Scoped code quality, `git diff --check`, no-BOM, added-line ASCII, and repository extraction-source searches for the removed weighted arithmetic passed
- The shared working-tree diff for the four tracked implementation/test files is 351 additions and 102 removals, including the preceding GQR-0063 peak-memory work; this item changed no inherited D1/D2 file
- UBSan execution was not available in the configured MSVC host test build; the Android Clang object matrix compiled the production path, and the focused extreme-component calls exercise it without any remaining weighted arithmetic
