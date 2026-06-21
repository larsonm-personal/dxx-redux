# Failure fault tolerance 2026-06-21

## Goal
- Inspect report `report_20260621_094533.md`.
- Identify durable causes behind the three failures without optional/timeout ping-pong.
- Prefer fixes that make tests or harness behavior more tolerant of valid state variation.

## Plan
- [x] Read project instructions and load the reported failures.
- [x] Inspect full logs and owning test scripts for the three failures.
- [x] Identify shared or structural brittleness.
- [x] Implement one or more durable fixes where the cause is clear.
- [x] Run targeted validation and record results.

## Findings
- `test_levelcomplete_touch_skip` was racing a 500 ms suppress window after `trigger_levelcomplete`; in the report, the next automation step did not run until after that window could expire. The script was also not actually modeling a carried touch.
- The carried-touch native path was incomplete: a touch that started before the level-complete page could release after the page became active and fall through as a mouse release.
- `test_merged_wall_two_pass_probe` failed on an exact `submit_nv` value for a secondary face. The primary target face and route checks still expressed the regression intent; the secondary face vertex count is allowed to vary.
- `test_lan` had useful diagnostics in `temp/lan_test_log.txt`, but the suite report log captured only stdout/stderr and was effectively blank. The underlying LAN failure remains a network sync issue: host stayed in a one-player network game while joiner traffic went out and no replies came back.

## Changes
- Added explicit `send_touch_down` and `send_touch_up` automation actions.
- Fixed the level-complete carried-release path so a release while the suppress gate is active is consumed and clears the gate.
- Reworked `test_levelcomplete_touch_skip.json5` to hold a touch across the level-complete transition, release it, assert the page remains active, wait for the suppress guard to clear by state, then tap to advance.
- Relaxed the merged-wall secondary-face `submit_nv` assertion from exact `3` to `gte 3`.
- Made failing `test_lan.ps1` emit its sidecar diagnostic log to stdout so future suite reports include the real failure detail.

## Validation
- `android/run-code-quality.ps1 -Fix -Paths ...` passed for the touched files.
- `:app:assembleDebug` passed.
- `test_levelcomplete_touch_skip.json5` passed on emulator with D2.
- `test_merged_wall_two_pass_probe.json5` passed on emulator with D2.
- Parsed the touched JSON5 scripts and `test_lan.ps1`.
- Did not rerun the full `test_lan` network test in this pass; only the failure-reporting path was changed there.
