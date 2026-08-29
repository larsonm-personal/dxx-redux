# GuideBot coop endpoint liveness

## Goal

Restore classic-style GuideBot motion after reaching enhanced route endpoints, fix the final fly-through objective failure, and avoid parking at player-owned keyed doors without weakening objective selection or blocker handling.

## Plan

- [x] Correlate the coop log's final fly-through objective, endpoint arrivals, stalls, and mode transitions
- [x] Compare enhanced endpoint handling with the classic escort arrival and wander/return behavior
- [x] Restore classic endpoint liveness while preserving semantic objectives
- [x] Add or extend route and navigation regression coverage, including auto-closing triggers
- [x] Run scoped formatting and D2 host/Android native builds and tests; record the integration replay limitation

## Findings

- Counterstrike level 23 steps 4 through 9 are consecutive fly-through or pass-through triggers. Steps 4 and 5 repeatedly alternate in the log because their linked auto-closing doors are used as completion evidence. Once a door closes, the selector incorrectly makes its already-fired trigger pending again.
- The long endpoint parks are intentional consequences of the Android-only semantic endpoint hold in `aipath.c`. It stops velocity at the final point and bypasses the classic completion branch, which otherwise reverses the path, circles locally, or creates a short path toward the player.
- A player-owned keyed door needs no separate movement mode. It is already a valid physical endpoint while the semantic objective remains intact. Restoring classic endpoint completion supplies the expected motion, and periodic objective refresh supplies retries after the player opens the door.

## Chosen design

- Latch successful trigger activations for the current level and expose that fact to live route completion checks. The latch is monotonic and resets with GuideBot level state.
- Remove the semantic endpoint hold and its terminal-refresh suppression. Keep semantic goal selection and physical-frontier substitution unchanged, while allowing the classic path follower to own endpoint motion again.

## Validation

- `git diff --check` passes apart from the existing CRLF normalization warning for `d2/main/aipath.c`.
- The Windows D2 executable and complete host test target build successfully.
- Focused route validation passes: `test_level_metadata_scan`, `test_guidebot_route_certifier`, `test_escort_goal_policy`, and `test_guidebot_calculation_benchmark` (4/4).
- The complete host CTest run passes 43/44 tests. The unrelated `test_args_defaults` executable faults when run directly as well; none of the changed GuideBot or route tests fail.
- Android native D2 libraries build successfully for both `arm64-v8a` and `x86_64`.
- `test_counterstrike_level23_guidebot_trigger_liveness.jsonc` parses successfully and covers the exact level-23 trigger chain, keyed-door endpoint fallback, auto-closing-door non-regression, and final reactor selection.
- The device replay was not executed because the installed Gradle launcher sees Java 8 while the Android project requires JDK 17. Native Android compilation and the focused host regressions provide compile and logic coverage, but the new scenario still needs an instrumented device run in a JDK-17 build environment.
