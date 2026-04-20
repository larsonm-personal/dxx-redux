# D2 intro skip includes title screen

## Status

- Completed: inspected the D2 startup/title flow and confirmed the final title screen path lived outside the old `skip_intro_movie` gate
- Completed: extended the D2 auto-skip path so it now exits the full D2 title sequence like D1
- Completed: ran code quality, `run-windows-build.ps1 -Target d2`, and `test_engine_prefs_unified.json5 -Game d2`
- Completed: confirmed the earlier `test_intro_skip_inputs_unified.json5 -Game d2` failure was a launcher automation relaunch issue, not a D2 title-flow regression
- Completed: revalidated `test_intro_skip_inputs_unified.json5 -Game d2` after the harness relaunch fix

## Plan

1. Read `d2/main/titles.c` to identify which title screen path remains outside the current D2 skip gate
2. Apply the smallest D2-only change so `skip_intro_movie` skips that title screen path too
3. Re-run code quality and affected intro or engine-prefs tests
4. Update this plan with the verification result

## Verification

- `android\run-code-quality.ps1 -Fix`: pass
- `run-windows-build.ps1 -Target d2`: pass
- `android\run_test.ps1 -ScriptName test_engine_prefs_unified.json5 -Game d2 -TimeoutSeconds 180`: pass
- `android\run_test.ps1 -ScriptName test_intro_skip_inputs_unified.json5 -Game d2 -TimeoutSeconds 120`: pass after launcher harness clean-restart fix