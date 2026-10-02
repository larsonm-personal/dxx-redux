# GitHub Actions repair

## Scope

- Inspect recent Android tooling failures and all packaging workflows
- Reproduce failures locally on Windows and Linux where available
- Fix the underlying tooling or workflow defects without changing unrelated work
- Validate the smoke suite, workflow syntax, and relevant builds or packaging checks

## Evidence and progress

- Latest run 36970068955 fails during tooling smoke tests on both runner platforms; bounded Python installation succeeds
- Package - All currently invokes upstream workflows instead of the local repository workflows
- Local checkout contains unrelated changes; preserve them

## Repairs and validation

- Added catalog ownership for all ten graphics recovery support scripts and an explicit recovery integration runner
- Registered recently added graphics, input, LAN and distribution tests in coverage families; retained caller-selected device tests as explicit runs
- Replaced generated LAN script filenames with explicit supported filenames so the catalog can verify their owner
- Smoke runner now prints failed child diagnostics, while PowerShell 5.1 release checks run even after another test fails
- Android tooling run 36971849004 passed both Ubuntu 24.04 and Windows Server 2022, including all bounded-runtime checks and Windows PowerShell 5.1 release scenarios
- Local Windows smoke suite passed all 24 checks; Windows PowerShell 5.1 passed all 68 release integration scenarios
- All workflows pass actionlint 1.7.12; scoped code-quality checks pass
- Updated packaging checkout/upload actions, selected local reusable workflows, replaced removed vcpkg x-gha caching with file caching, and declared MSVC triplets explicitly
- Packaging jobs now run native CTest checks, build with four workers, and require nonempty archives
- Packaging scripts now stop on failures, check download HTTP errors, quote paths, locate dynamic SDL2 on macOS, and include in-tree Windows DLLs
- Windows FluidSynth DLL output is placed beside game and host-test executables
- Initial MinGW CI build exposed a benchmark test's unavailable MSVCRT timespec_get; use QueryPerformanceCounter on Windows while retaining the existing non-Windows clock

## Final validation

- Package validation run 36973753660 passed all six jobs: Linux, MinGW Windows, MSVC x86, MSVC x64, Intel macOS and ARM macOS
- Each packaging job passed all 54 D1 and 63 D2 CTest checks, then uploaded its packages and required symbol artifacts
- Fixed the stale D2 companion path assertion and added coverage for the cloaked-player destination
- Local D1 and D2 CTest suites passed; final scoped mixed-language formatting and lint checks passed
- Android tooling run 36973752992 passed both runner platforms
- Removed the temporary validation workflow after the complete package pass
- The isolated repair branch excludes unrelated in-progress native-interruption and activity-replacement scenarios; their local changes are preserved
