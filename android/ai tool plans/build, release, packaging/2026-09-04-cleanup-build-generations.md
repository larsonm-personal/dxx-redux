# Automatic superseded build cleanup

- [x] Identify native build layouts and comparable build generations
- [x] Keep the newest generation per family and automatically remove older inactive generations
- [x] Extend fixture tests for CXX, CMake, versioned outputs, retention, and safety
- [x] Run formatting, build/tests, and a read-only workspace preview

User explicitly classifies N-1 builds in generated build/temp directories as automatic cleanup, without confirmation

## Policy

- Separate CXX hash families per module/configuration, compatible CMake cache families across build/temp locations, and versioned/timestamped package families
- Keep one newest generation by full-tree modification time by default; retain timestamp ties and a 24-hour grace window
- KeepBuildGenerations and BuildGraceHours are configurable; BuildsOnly excludes ordinary scratch/review cleanup
- Generated FetchContent clones, their submodules, and dependency remnants under stale ABI recovery directories are part of the disposable build generation
- Preserve unrelated nested repositories, tracked/non-ignored files, filesystem links, held locks, active work, and changed candidates
- Recheck that the newer retained build still exists before deleting its predecessor

## Verification

- Synthetic Git fixture tests pass, covering module/configuration/architecture separation, two-generation retention, recent builds, package naming, generated clones/submodules, and BuildsOnly
- Windows D1/D2 builds pass; D2 CTest passes 45/45
- No real build directories have been deleted during development
- Scoped formatting and lint passed
- Workspace BuildsOnly preview completed, identifying about 138.77 GiB of superseded artifacts, including 31 old app/.cxx/Debug hash directories and retaining Debug/4bi316q1
- Preview drove coverage for FetchContent submodules and partial stale ABI dependency trees; fixtures cover these cases
- Exclude .cxx tool metadata from hash-generation recognition by requiring an ABI child directory
- The measured preview preceded the final package timestamp-family normalization and metadata exclusion; those adjustments passed the synthetic tests and do not remove additional native hash families
