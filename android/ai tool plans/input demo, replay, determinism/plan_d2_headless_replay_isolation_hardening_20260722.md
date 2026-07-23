# D2 headless replay isolation hardening

## Goal

Diagnose and harden `test_input_demo_regressions_d2` so replay results do not depend on prior demos or ambient host state.

## Plan

- [x] Reproduce each reported failing demo alone and in the reported suite order
- [x] Compare repeat runs, replay artifacts, and runner sandbox lifecycle to locate leaked state or nondeterminism
- [x] Fix the engine or runner isolation boundary without changing recorded expectations or extending timeouts
- [x] Run scoped code quality checks and the relevant build
- [x] Run the focused failing demos and the complete D2 headless replay suite
- [x] Record verification results and mark this plan complete

## Result

The three failures reproduced deterministically with the pre-existing host executable. State tracing first showed a weapon-object difference at frame 154 and an RNG divergence at frame 689, but a separate-worktree revision check showed that the fixture passed from the July 14 known-good source through current D2 engine commit `bc4e11e1`. Rebuilding the current checkout made all three failures pass.

The root cause was stale build state. The input-demo build guard accepted an executable based only on source and stamp timestamps, so a binary from another source revision could appear current after source checkouts or commits preserved older file timestamps.

Host builds now stamp the Git source revision on Windows and Linux. The input-demo guard requires that stamp to match `HEAD` in addition to its existing timestamp checks. A focused test creates a temporary repository, advances its revision without changing the simulation source timestamp, and verifies that the old executable is rejected.

Verification:

- `android/tests/test_input_demo_host_build_guard.ps1`: pass
- `android/tests/test_validate_automation_catalog.ps1`: pass
- `run-windows-build.ps1 -Target d2`: pass
- Reported three D2 fixtures after rebuilding: 3/3 pass in 44.065 seconds
- Complete D2 headless replay suite: 11/11 pass in 96.46 seconds
- Scoped `android/run-code-quality.ps1 -Fix`: pass
- `shellcheck run-linux-build.sh` and `git diff --check`: pass
