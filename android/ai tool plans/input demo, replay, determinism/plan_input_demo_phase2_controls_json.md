# Input Demo Phase 2 Controls JSON Tranche

## Goal

Start Phase 2 with the smallest Android-first slice that is immediately useful
for later recording/replay work:

- shared portable control-record structs under `android/`
- sparse JSON/JSONL parse/write helpers using `nlohmann::json`
- tiny D1/D2 conversion helpers for the replayable `control_info` subset
- host round-trip tests for D1 and D2

## Constraints

- Keep D1/D2 edits tiny and generic
- Do not hand-roll JSON parsing or writing
- Match the existing sparse key plan in `input-demo-schema.md`
- Keep ordering stable for git-friendly diffs
- Avoid broad replay/startup hooks in this tranche

## Planned Steps

- [x] Add a shared portable record header and C++ JSON helper under `android/app/src/main/cpp/shared`
- [x] Add small D1/D2 conversion helpers for `control_info`
- [x] Add a host test that round-trips `control_info -> portable record -> JSONL -> portable record -> control_info`
- [x] Wire the test into the existing Windows build path
- [x] Run build, test, and code-quality validation

## Completed Notes

- Added `input_demo_controls.h/.cpp` under `android/app/src/main/cpp/shared`.
	The helper owns the portable held-state and pulse structs, sparse JSONL key
	ordering, validation, and file read/write paths using `nlohmann::ordered_json`.
- Added tiny `control_info` adapters in `d1/main/input_demo_control_info.h` and
	`d2/main/input_demo_control_info.h` so the game-specific surface stays small.
- Added `android/tests/test_input_demo_controls.cpp` and wired it into both
	`d1/maths/CMakeLists.txt` and `d2/maths/CMakeLists.txt` as a host probe.
- The shared portable structs needed explicit packing in their own header. The
	game headers compile under packed layout from `pstypes.h`, while the shared
	C++ helper does not unless packing is declared locally.
- The maths probe targets had to stay host-only. In the unified Android CMake
	graph, D1 and D2 are configured together, so duplicate probe target names must
	be excluded from `ANDROID` builds.

## Validation

- `run-windows-build.ps1 -Target both` passed after the tranche landed.
- `buildd1\maths\test_rng_seed_resume.exe` passed.
- `buildd1\maths\test_input_demo_rng_mode.exe` passed.
- `buildd1\maths\test_input_demo_controls.exe` passed.
- `buildd2\maths\test_rng_seed_resume.exe` passed.
- `buildd2\maths\test_input_demo_rng_mode.exe` passed.
- `buildd2\maths\test_input_demo_controls.exe` passed.
- `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon` passed with
	the new helper integrated into the Android native source lists.

## Exit Criteria

- D1 and D2 can serialize and parse the same planned sparse control keys
- D2-only keys are preserved on D2 and rejected on D1
- Tests pass on Windows host builds
- Plan status is updated when the tranche completes
