# Host Replay Sandbox Guardrails Plan

## Goal

Make host replay experiments repeatable so manual investigation does not get stuck in first-run flows like pilot creation, fullscreen grabs, or intro startup.

## Planned Steps

- [x] Confirm the existing host replay wrapper and doc behavior.
- [x] Add a small wrapper guardrail for reusable replay sandboxes.
- [x] Document the exact manual workflow for iterative host replay debugging.
- [x] Run a focused validation of the updated wrapper behavior.

## Notes

- `android/tests/run_input_demo_replay.ps1` already creates a windowed sandbox and skips D1 titles or D2 movies.
- The missing piece for iterative manual experiments is preserving a known-good sandbox state, especially a created pilot in `Players/`.
- Validation: the wrapper now prints `-inputdemo-replay ... -pilot replay`, and a sentinel file survived a second `-ReuseSandbox` run in the same sandbox.