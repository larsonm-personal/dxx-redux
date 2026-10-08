# Graphics confirmation input and first-run retry

1. Track controller axes before the modal opens so the first new D-pad movement works while held directions remain suppressed
2. Confirm first-run choices without the Video Info debounce, default to OK only for first-run trials, and reopen the picker after rollback
3. Extend existing controller and first-run integration coverage, build both Android engines, and run scoped quality checks

Keep the five-second safety deadline and the Video Info editing debounce

## Result

The overlay tracks controller axes while hidden, so a fresh D-pad or stick movement works on the first event and a direction held before opening still requires release. First-run Done skips the quiet period and its confirmation defaults to OK. Rejection restores the accepted settings and starts a new editor with those restored values, without reapplying the failed maximum settings. Explicit Keep previous settings and backgrounding still exit the workflow

## Validation

- Android x86_64 debug build passed for both native engines and the Kotlin overlay
- First-run Accept, Cancel, Timeout, Back, Previous, Unchanged and Background passed for D1 and D2, including cross-engine handled-marker relaunches
- Confirmation input regression passed 97/97 steps for each engine, including first-event hat navigation, held-stick suppression, ordinary default Cancel, key repeat/release and touch routing
- Injected first-run render stalls restored the safe settings and reopened the picker for both engines, with cross-engine relaunch checks passing
- Scoped formatting/lint and both automation catalog checks passed

Evidence: `android/temp/graphics-confirmation-*.log`, `android/temp/graphics-first-run-20261007-141946/`, `android/temp/graphics-first-run-20261007-143750/`, and `android/temp/graphics-safety-20261007-143519/`
