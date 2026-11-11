**For CODEX only (ignore if you are COPILOT)**
If you are CODEX, then also consider the following instruction files:

- ALWAYS read the instructions file at `./.github/copilot-instructions.md`
- When workspace free space is low, use `./android/clean-workspace.ps1` from the repository root. Preview first, exclude active runs and authentic input assets, wait for active jobs, then use `-TemporaryOnly -TargetFreeSpaceGB 50` (and `-PreserveEmulatorWorkspaces` if an emulator is open). Follow `android/CLEANUP.md`; do not hand-delete individual caches as the normal response to low space.
