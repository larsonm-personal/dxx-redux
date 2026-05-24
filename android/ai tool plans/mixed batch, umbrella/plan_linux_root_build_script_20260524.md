# Linux Root Build Script Plan

Goal: verify the native Linux build path from repo root and add a root-level Linux build script similar to `run-windows-build.ps1`

## Phase 1: Validate Current Linux Build

- [x] Inspect current native Linux build commands and target layout
- [x] Run a first root-driven Linux configure/build attempt
- [x] Record the exact blockers or success path on this host

## Phase 2: Add Root Build Script

- [x] Mirror the Windows script behavior where it makes sense on Linux
- [x] Keep target selection for `d1`, `d2`, or both
- [x] Use stable root build directories and optional clean behavior

## Phase 3: Verify And Update Plan

- [x] Run the new Linux build script on this host
- [x] Confirm resulting build status and note any remaining host prerequisites
- [x] Mark plan status for the next tranche

## Result

- Added root helper `run-linux-build.sh`
- Script resolves `cmake` and `ninja` from `dependency_base.txt` managed Android SDK CMake before falling back to PATH
- Script supports `--target both|d1|d2`, `--build-type`, `--clean`, `--jobs`, `--generator`, and `--list-tools`
- Script uses root build directories `buildd1` and `buildd2` to mirror `run-windows-build.ps1`
- Validated `./run-linux-build.sh --list-tools` on this host
- Validated `./run-linux-build.sh --target d1 --clean` on this host

## Current Host Status

- Native Linux desktop prerequisites were installed on this Ubuntu host
- `./run-linux-build.sh` now succeeds with no flags and builds both `d1` and `d2`
- Non-game-data desktop tests pass:
	- `buildd1/maths/test_input_demo_fixture`
	- `buildd1/maths/test_input_demo_replay`
	- `buildd2/maths/test_input_demo_fixture`
	- `buildd2/maths/test_input_demo_replay`
- `android/run_cue_iso_tests.sh` now passes all 6 registered tests on this host

## Follow-up Tranche

- [x] Add root Linux build helper
- [x] Add `android/get_deps` helper for Linux native desktop prerequisites
- [x] Re-run `./run-linux-build.sh` with no flags and fix any build issues
- [x] Run tests that do not depend on game data
- [ ] Reduce Linux compiler warnings introduced on the `cmake` branch only
