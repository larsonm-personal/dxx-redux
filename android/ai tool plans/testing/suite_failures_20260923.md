# Suite failures from report_20260923_072424

Fix the seven failed owners without treating longer timeouts as the default
solution. Preserve strict failures and examine forward progress, state ownership,
dependencies, and reproducibility before changing budgets or fixtures.

1. Inspect each failure and its retained diagnostics, including individual route cases
2. Repair missing helper dependencies and isolate stale launcher observations
3. Investigate route assertions, the D1-in-D2 replay divergence, and the secret-area schema additions
4. Diagnose multiplayer launch state and make failure/progress observable
5. Run affected host checks and emulator workflows serially; format changed files

Initial findings:

- Process-wait and simulation-reporting tests report missing helper functions
- Launcher readiness failed even though its later diagnostic contains both required buttons
- Secret-area comparison reports only added flyout fields
- Six physical route cases failed inside the route owner
- One D1 level 7 replay failed in D1-in-D2; other engine/canary runs passed
- Multiplayer signaling/chat/ready succeeded, but neither host game process nor sync was observed

Record fixes and verified outcomes below as work completes.

## Repairs and evidence

- Process-wait test: load the output disk-space guard and its fixture parameters
  before invoking the AST-extracted replay waiter. Passed
- Simulation-reporting test: extract the timeout classification dependency with
  the result callback. Passed
- Launcher D-pad: use a fresh, separately published button-only snapshot so UI
  readiness does not depend on music/archive/metadata enumeration. All five
  navigation checks passed with the original time limits
- Secret-area baseline: regenerated the newly published flyout metadata. Verified
  that removing flyout from the new fixture yields the exact previous fixture.
  Full baseline check passed; no secret-area expectations were weakened
- D1 Guide-Bot: connect the existing optional generation-owned D2 asset extension
  after native/custom preparation. Previously no verification actor was available
- D1 texture references: preserve native modulo wrapping against the native
  texture count, excluding optional extension slots and retaining overlay rotation.
  Native/imported compatibility tests and reactor-lifetime route cases passed
- Saturn boss route: unfreeze AI for a dying boss in the route sandbox. Native
  D1 completes its death roll in the object frame. The deterministic case passed
- Portal recovery: validate intermediate full-radius occupancy as well as sweeps,
  matching the movement code's wall-intersection correction. Both Ironstar variants
  now complete. Hidden-door final approaches also require the collision sweep to
  enter the authored target segment; overlapping coordinates are not a crossing.
  Keep intermediate waypoints when that proof fails. All six precision cases pass
  twice with identical results and full player radius, including Invertaus
- D1 Level 7 replay: frame 4744 first diverged after lava contact. D2 drew the two
  rotational-kick values before the shove vector; D1 draws them afterward. Restore
  D1 ordering only for imported D1. The previously failing recording passed with
  its original expected result. Expand gameplay-rule traces to include collision
  velocity and rotation, not just damage and RNG counts
- Gameplay-rule parity: native/imported comparison passed with the expanded
  collision motion fields, including the existing ordinary D2 checks
- Multiplayer: preceding CD tests leave persistent pilot music preferences while
  reset removes the CD source. The fixture now explicitly selects MIDI. Publish
  launch phase/error, retain preflight logs, wait for launch information instead
  of blind sleeps, and fail promptly on a reported launch error. The two-emulator
  suite smoke profile passed; both players entered level 1 via introspection
- Scoped mixed-language formatting and lint passed
- Final Android debug build (all three ABIs), Windows D1/D2 builds, and both
  `test_upstream_compat` CTest targets passed
- Full affected route owner passed: 23/23 cases with the original report's seed
  265. Report: `android/temp/route_regression_cases/run_20260923_101321_706/summary.json`
- Full D1-in-D2 replay owner passed: 8/8 original recordings and expectations,
  including all three Level 7 recordings. Log: `temp/suite-fixes-20260923/replays-final.log`

All seven originally failing test owners now pass their affected checks. The
entire 213-test suite was not rerun. Reproduction commands for the larger groups:

```powershell
pwsh android/tests/test_guidebot_route_regressions.ps1 -NoBuild -Seed 265
pwsh android/tests/test_input_demo_regressions.ps1 -D1InD2 -RecordedGame d1
pwsh android/tests/test_mp.ps1 -SkipBuild -SoakSeconds 0
```

The `NoBuild`/`SkipBuild` commands assume the verified current host binaries and
APK; the multiplayer APK must be installed on both emulators

No test timeout or route stall budget has been increased. Temporary probes were
used to establish causes and are removed from the final changes. Diagnostics are
under `temp/suite-fixes-20260923`; the large paired replay attempt is marked
incomplete because its imported trace hit the capture budget and binaries changed
during other route work. Its complete native repeats match. The ordinary replay
regression supplies the completed pass for the lava fix
