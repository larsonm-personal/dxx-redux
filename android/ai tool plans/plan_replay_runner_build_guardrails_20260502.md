# Replay runner build guardrails plan 2026-05-02

## Goals
- Add optional pre-run host rebuild to run_input_demo_replay.ps1
- Add optional freshness check to fail when exe is older than source files
- Keep defaults non-breaking
- Validate script syntax and basic invocation help

## Steps
- [completed] Patch run_input_demo_replay.ps1 params and helper functions
- [completed] Wire checks into launch flow before sandbox creation
- [completed] Run a smoke validation command
- [completed] Mark plan complete
