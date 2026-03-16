# Plan: Complete Axis Mapping Test Infrastructure

## Problem
1. `Run-TestMenu.ps1` crashes because it force-stops the app but never restarts it before sending the automation broadcast
2. The assert action only supports exact string equality — can't verify numeric comparisons like "heading_time != 0"
3. The test script uses bare `introspect` actions for Phase 2 but has no way to verify the values

## Changes

### 1. Enhanced assertions in game_automate.cpp
Extend `assert_expect` to support comparison operators. Expect values can be:
- Simple string/number: `"axis_bind_pitch": "3"` → exact equality (backward compat)
- Object with operator: `"heading_time": {"ne": 0}` → not-equal

Supported operators: `eq`, `ne`, `gt`, `lt`, `gte`, `lte`, `range`
- `{"gt": 0}` → actual > 0
- `{"ne": 0}` → actual != 0
- `{"range": [10, 1000]}` → 10 <= actual <= 1000

Implementation: Add `op`, `num_value`, `range_min/max` fields to `assert_expect`.
Modify parsing to detect object-valued expects. Modify `run_assertions` to dispatch on operator.

### 2. Fix Run-TestMenu.ps1
Replace the current approach (force-stop + run_automation.sh) with delegation to run_test.ps1,
which already handles the full lifecycle: health check, push, launch, broadcast, monitor, report.

### 3. Update test_axis_mapping.json5
Replace bare `introspect` actions in Phase 2 with proper assertions:
- Phase 2a: Send axis 2 → assert heading_time != 0 (turn works)
- Phase 2b: Send axis 0 → assert slide_lr_time != 0, heading_time == 0 (slide, not turn)
- Phase 2c: Send axis 3 → assert pitch_time != 0
- Phase 2d: Send axis 1 → assert slide_ud_time != 0

### 4. Build, deploy, run end-to-end
