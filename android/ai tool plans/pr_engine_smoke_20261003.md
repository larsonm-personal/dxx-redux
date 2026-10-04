# PR engine smoke checks

## Findings

- Android tooling runs 24 host checks on Linux/Windows, including dependency,
  extraction, process cleanup, release, comparison and catalog checks. It does
  not compile an engine or execute gameplay tests
- Both native CMake builds already register asset-free engine integration and
  unit tests. D2 includes a synthetic classic-demo writer/reader round trip
- Input recordings and retail game media are not tracked, so the full replay
  corpus cannot run in an ordinary clean PR checkout

## Plan

- [x] Add parallel D1/D2 Linux PR jobs with a 20-minute limit per job
- [x] Compile the normal all target, including desktop, headless and test tools,
      with OpenGL, audio and networking enabled, then run all registered CTests
- [x] Keep assertions enabled and bound tests; preserve logs on failure
- [x] Restrict tooling runs to changes in tooling and remove duplicate push runs
- [x] Validate fresh Linux builds and tests, workflow lint and scoped formatting
- [x] Document measured timings, coverage limits and the future replay-media path

## Validation and findings

- Fresh Ubuntu 24.04 WSL builds, four compile workers per game, no compiler cache:
  D1 configured in 91 seconds and built all 804 steps in 5m11s; D2 configured in
  about 100 seconds and built all 1559 steps in 9m22s
- Debug caught an existing integration fixture creating an `OBJ_WEAPON` with
  `CT_NONE`. Corrected it to `CT_WEAPON`; no engine behavior or assertion changed
- Windows-mounted fixture storage made the save-state integration test exceed
  short timeouts. A debugger sample found it reading save data through PhysFS
  D1 passed directly in 68 seconds for the suite. D2 passed all 64 tests in
  26 seconds with its isolated fixture directory temporarily mounted as tmpfs
  The mount is local validation only; ordinary hosted Linux uses native storage
- Final validation passed both suites as the normal WSL user: D1 55/55 in
  17.11 seconds and D2 64/64 in 26.26 seconds, with only the
  integration fixture directories on temporary tmpfs mounts inside the build
  tree. Logs and JUnit remain on disk; mounts are removed when each run exits
- Scoped mixed-language formatting/lint and actionlint 1.7.12 passed
- Existing native Debug compiler warnings remain; this change adds none
- Hosted timing is not yet measured. The workflow caps each parallel job at
  20 minutes; queue time is outside that budget

Evidence: `temp/pr_engine_smoke_20261003/`. Coverage, trigger policy and the
full replay-media follow-up are documented in `.github/CI.md`
