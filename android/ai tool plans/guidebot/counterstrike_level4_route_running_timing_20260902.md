# Counterstrike level 4 route-running timing investigation

## Plan

- [x] Identify what the headed runner's `route running` timer measures
- [x] Trace the headed and headless executables into the route-confirmation loop
- [x] Compare live GuideBot planning calls with ordinary on-device GuideBot routing
- [x] Separate planning cost, simulation time, and wall-clock time for level 4
- [x] Record conclusions and any justified performance follow-up

## Findings

- `Windows route running` is a PowerShell wall-clock stopwatch around the entire
  desktop process. It is not a route-planner duration.
- The desktop run advances a deterministic 60 Hz simulation while rendering.
  Counterstrike level 4 takes 4,634 frames, or 77.23 simulated seconds, so a
  visible run naturally takes roughly that long.
- The equivalent headless run executes the same 4,634 simulation frames and
  completed in about two seconds during focused verification.
- Semantic objective selection uses the same live route rescan and GuideBot
  goal-selection functions used by ordinary gameplay. Physical legs use the
  same GuideBot path generator and AI path follower.
- Route confirmation is still a harness around those shared systems. It uses a
  fixed time step, drives the companion continuously, boosts its speed, removes
  ordinary robots, and applies scripted objective interactions.

## Follow-up

- [x] Move the Windows build out of the hidden route-runner child process
- [x] Run an incremental build check before every visible route invocation
- [x] Show a distinct build status before starting the visible simulation
- [x] Extend the browser integration test to enforce the child `-NoBuild` handoff
- [x] Run scoped quality checks and the focused integration test

The duplicate interval was an incremental Windows build performed by the
hidden child PowerShell. The parent stopwatch began before that child build and
therefore mislabeled the build as `Windows route running`. The browser now owns
and displays the build check before every invocation, while every route child
receives `-NoBuild`. This preserves source-change detection between repeats and
between different levels selected in the same browser session.
