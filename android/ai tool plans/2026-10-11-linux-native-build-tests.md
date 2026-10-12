# Linux native build and test entry point

## Scope

Make fresh Linux native D1/D2 builds and asset-free tests reproducible without PowerShell, preserving build-only defaults and engine behavior

## Baseline

- Branch from cmake at 062a4c0f
- Read AGENTS.md, copilot instructions, existing Linux tooling plan, CI and replay documentation
- Native build prerequisites were absent in the verification container; provisioned official Debian packages and CMake into a workspace-local prefix without modifying system paths
- GCC/G++ 14.2.0: D1 Debug all-target build fails in test_upstream_compat because JSON initializers bind references to packed engine fields in config and pickup fixtures
- Existing local build script has no CTest mode, an obsolete prerequisite hint and unchecked option values
- Existing CI already runs the appropriate asset-free test list with Debug assertions, serial execution, dummy SDL drivers, timeouts and zero-test failure

## Plan

1. Reproduce both games' actual configure/build failures and repair fixture scalar serialization without changing packed engine layouts
2. Add opt-in Debug build-and-test mode to the existing Linux entry point, isolated from normal/replay build trees
3. Reuse native CTest registrations and CI safeguards, add isolated CLI regression coverage, and document requirements and excluded media/device tiers
4. Run both complete native builds and CTest suites, CLI regressions, catalog validators and scoped code quality; record passes, failures and blockers separately

## Limits

Retail media and full input-demo recordings are not tracked, so full-world replays and graphics/device validation require separately provisioned inputs and hardware

## Execution evidence

- Full Debug all-target builds pass for D1 and D2 using GCC/G++ 14.2.0, CMake 3.31.6 and Ninja 1.12.1
- Direct asset-free CTest: D1 69/69 and D2 78/78 passed with dummy SDL drivers, serial execution and 120-second per-test limits
- The fixture fix explicitly copies numeric packed members into JSON values, preserving original engine layouts and serialized numbers
- Linux CLI regressions exercise the actual Bash script with isolated recording executables; all 15 tests passed
- Automation catalog validation passed: 95 standalone JSON tests, 382 support scripts and 219 standalone PowerShell tests
- The run-all catalog check could not launch its supervised child because this cloud lacks /proc/self/task/<pid>/children; its safety supervision was not disabled
- First clean test-tree attempt encountered an HTTP error downloading a pinned archive. Reused baseline hash-addressed archives and retained CMake's SHA-256 checks for the retry
- Final isolated test-mode workflow passed end to end: all native targets built, D1 69/69 and D2 78/78 CTests passed, and the runner exited successfully
- Scoped mixed-language formatting and the subsequent check-only run passed; Bash syntax, actionlint and git diff --check also passed
- PowerShell native entry point passed its CLI regression suite and D1 69/69 through the new Linux workflow
- Private-data tests, emulator/GPU validation, Windows/macOS execution and sanitizer execution were not part of this native verification pass
