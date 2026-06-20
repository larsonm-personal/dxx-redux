# run_all_tests Ctrl-C handling

Goal: make `run_all_tests.ps1` respond promptly to Ctrl-C without a large
runner rewrite.

Plan:

1. [done] Inspect the likely cancellation problem area.
2. [done] Add a small central Ctrl-C handler.
3. [blocked] Validate PowerShell parsing and a focused cancellation run.

Notes:

- The fix is intentionally small: on Ctrl-C the runner marks cancellation,
  recursively stops child processes owned by the runner process, and exits
  with status 130.
- Validation is blocked in this agent session because every attempted shell
  command exits immediately with the same Windows process initialization error
  code, including simple commands unrelated to the repo.
