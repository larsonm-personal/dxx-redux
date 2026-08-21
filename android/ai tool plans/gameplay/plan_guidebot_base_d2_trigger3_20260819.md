# Guide-Bot Base D2 Trigger 3 Plan

## Scope

- Correlate the supplied trace with the active route objective and path endpoint
- Determine why the Guide-Bot stopped too far from objective 3's switch
- Implement a minimal navigation or firing-position correction
- Add a focused base D2 regression and verify affected builds

## Work

- [x] Analyze the trace and identify the failing route state
- [x] Reproduce the objective with automation or deterministic state
- [x] Implement the correction with useful diagnostics
- [x] Add regression coverage
- [x] Run scoped quality checks, builds, and tests

## Findings

- The restored coop session selected trigger 12 in segment 44 and repeatedly stalled on the segment 33 to 30 connection
- Stall recovery blacklisted destination segment 30, so any alternate path that eventually used segment 30 was rejected and recovery restored the same failed path
- Recovery now blacklists the specific undirected connection instead, allowing alternate entrances to either segment
- Stall logs now include the connecting side, wall, doorway flags, AI openability, wall type/state/flags/trigger, and Guide-Bot size
- A clean single-player reconstruction at the logged position traverses normally, which isolates the reported failure to restored runtime/coop state rather than static level geometry

## Verification

- Android debug APK build passed
- Counterstrike level 20 trigger 3 automation passed from the logged segment and position
- Scoped code quality and diff checks passed
- Windows D2 build reached the final headless target but is blocked by pre-existing unrelated `input_demo_headless_main.cpp` errors for `ReadConfigFile` and `GameCfg`
- Existing Obsidian grate automation reached gameplay but the emulator killed both app processes before it produced a result
