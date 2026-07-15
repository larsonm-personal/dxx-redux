# Headless Input Demo Regression Run Plan

Date: 2026-07-14
Status: Complete

## Objective

Run the committed input-demo regressions that support the D2 headless executable, continue past individual failures, and report complete per-demo results.

## Plan

- [x] Read repository instructions and inventory committed demos by game and start mode
- [x] Confirm the regression wrapper's headless executable selection and required data path
- [x] Run all D2-recorded demos through the D2 headless executable without stop-on-first-failure
- [x] If needed, isolate failures with direct headless invocations and available diagnostics
- [x] Record the complete result matrix and any actionable failure cause

## Headless Inventory

All 15 committed demos use `start_mode: save_checkpoint`. The D2 headless executable supports the 11 D2-recorded demos. It does not support D1 recordings, including D1-in-D2: a direct attempt exits with `HEADLESS-RUN FAIL load Input demo replay currently supports D2 demos only`.

The D1-in-D2 regression wrapper was also tried, but it deliberately selects the full D2 windowed executable with `-inputdemo-norender`, not the headless executable, so those four runs are not counted as headless results.

## Command

```powershell
.\android\tests\test_input_demo_regressions.ps1 `
    -Game d2 -RecordedGame d2 -RunMode headless -TimeoutSeconds 180
```

The run continued after failures and attempted all 11 demos in 49.5 seconds.

## Results

| Demo | Result |
| --- | --- |
| `d2_descent2_level10_20260512_231237.dximdemo` | Access violation before result |
| `d2_descent2_level10_20260514_150656.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260511_192533.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260511_192804.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260511_193107.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260511_215620.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260511_215654.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260511_215831.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260512_084243.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260512_115227.dximdemo` | Access violation before result |
| `d2_descent2_level9_20260512_115624.dximdemo` | Access violation before result |

Every D2 headless process exited with Windows status `-1073741819`, or `0xC0000005`, and no actual-result file was written. A direct invocation reproduced the same fault. Earlier tracing placed the first failure after the demo header and player settings parsed successfully, during checkpoint restoration and before level startup. This is one common checkpoint-restore crash affecting the whole committed headless corpus, not a set of comparison mismatches.
