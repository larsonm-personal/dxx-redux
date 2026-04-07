# Lint cleanup and VS Code indexing exclusions

## Status: COMPLETE

## Task 1: Fix all leaked linter errors

### Findings
- **clang-format**: Clean (0 errors)
- **shellcheck**: Clean (0 errors)
- **shfmt**: Clean (0 errors)
- **ktlint**: 4 errors in `BuildInfo.kt` (generated file) -- `no-multi-spaces` from aligned `=` signs
- **PSScriptAnalyzer**: 12 errors in `android/app/.cxx/` -- third-party ktx_software scripts in NDK build cache

### Fixes applied
1. [DONE] Fixed `BuildInfo.kt` alignment in committed file + both build script templates (`1_build-aab.ps1`, `1_build-aab.sh`) to remove multi-space alignment
2. [DONE] Updated `run-psscriptanalyzer.ps1` to exclude `.cxx/` dirs (NDK build cache with third-party code)
3. [DONE] Verified all 5 linters pass clean

## Task 2: VS Code indexing exclusions

### Findings (file counts)
| Directory | Files | Description |
|---|---|---|
| `android/app/.cxx/` | 214,226 | NDK CMake build cache -- THE major culprit |
| `tools/etc2tool/build/` | 30,797 | ETC2 tool build output |
| `server/target/` | 16,571 | Rust/Cargo build output |
| `game_data/` | 8,231 | Binary game assets for testing |
| `android/app/build/` | 4,559 | Gradle app build output |
| `temp/` | 4,014 | Test logs and output captures |
| Total excluded | ~278,000 | (~97% of the 286K enumerated) |

### Fixes applied
1. [DONE] Added `files.exclude` in `.vscode/settings.json` for all build/cache dirs
2. [DONE] Added `search.exclude` for game_data dirs (kept visible in explorer but not searched)
3. Expected reduction: 286K -> ~8K indexed files
