# Accept Uneasy4's known route-planning timeout

- [x] Inspect the failed run and batch exit policy
- [x] Accept only the Uneasy4 headless process timeout, retaining the failed simulation record and diagnostic log
- [x] Verify timeout acceptance and rejection of unrelated failures; run scoped formatting and relevant PowerShell tests

The September 20 log stops at `preparation=route_planning completed=0 total=1`, before simulation starts. Ordinary unsolved routes already do not fail the batch. The user explicitly accepts this large mission's timeout. Keep running the mission so future successful results are still recorded, and report accepted timeouts separately in the batch summary

Validation: the original batch selected 2639 levels and had only this infrastructure failure. A focused real-engine run with a 10-second watchdog exited 0, printed `EXPECTED_TIMEOUT`, retained the `infrastructure_error` simulation evidence, and recorded one `expected_timeouts` entry with no fatal infrastructure failures. Artifacts: `android/temp/uneasy4_expected_timeout/20260920_121628`. The canonical 360-second default is unchanged; no checked-in mission data was written by this probe

Schema tests (including repeated timeouts, mixed timeout/crash failures, and exact-level scoping), runner tests (including an injected fatal engine failure), scoped code quality, and diff whitespace checks passed. Native code was unchanged, so no native rebuild or full corpus rerun was needed
