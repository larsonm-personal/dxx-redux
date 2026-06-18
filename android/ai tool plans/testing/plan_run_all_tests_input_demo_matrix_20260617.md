# run_all_tests input-demo matrix

Goal: make `android/run_all_tests.ps1` run committed input demos in explicit D1, D2, and D1-in-D2 sections, with headless/graphics variations where supported.

Plan:

1. [done] Inspect the existing input-demo regression wrappers and `run_all_tests.ps1` scheduling.
2. [done] Extend the regression runner so it can filter demos by recorded game and optionally replay D1 recordings through the D2 D1-in-D2 path.
3. [done] Add explicit test entries for D1, D2, and D1-in-D2 input-demo sections.
4. [done] Update `run_all_tests.ps1` to include and label all of those sections, skipping each only when its demo corpus is absent.
5. [done] Run scoped quality checks and at least a non-destructive listing/dry-run or focused wrapper invocation to verify the matrix shape.
