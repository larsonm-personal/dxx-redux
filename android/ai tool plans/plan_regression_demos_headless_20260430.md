# Plan: regression demos and headless survey

Date: 2026-04-30

Goal: make input demo regression checks run from `android/regression_demos`, tolerate leading `//` comments in demo files, revive the regression script for all demos in that directory, and begin the headless-engine work with an initial survey plus the first safe implementation step.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Locate input-demo parser, host replay wrapper, dormant regression scripts, and previous headless planning. | completed |
| 2 | Update parser so `.dximdemo` files can contain `//` comment lines where JSON records are read. | completed |
| 3 | Revive or add the regression demo script to run every `.dximdemo` under `android/regression_demos`. | completed |
| 4 | Run the regression script against the moved demo and fix issues until it passes. | completed |
| 5 | Survey engine paths needed for headless demo regression checks and record findings. | completed |
| 6 | Begin headless work with a small, low-risk code or script change. | completed |

## Notes

- Keep the regression demo directory as the default input source.
- Preserve normal Windows, Linux, Mac, and Android behavior.
- Avoid broad game-source changes unless the host regression path needs them.
- Parser changes are in the shared fixture parser and the host replay wrapper, so both the game process and PowerShell metadata reads skip blank lines and full-line `//` comments.
- Added `android/tests/run_input_demo_regressions.ps1`, defaulting to `android/regression_demos`, and `android/tests/run_input_demo_headless.ps1`, which uses the first no-present stage.
- Headless survey: skipping `game_render_frame()` changes simulation because render traversal fills rendered-object data used by robot wake-up logic. The safe first stage keeps SDL/OpenGL and render traversal active but skips the final `gr_flip()` when `-inputdemo-norender` is set during input demo replay. Later stages can split render traversal side effects from drawing, then investigate no-window/no-GPU SDL bootstrap.