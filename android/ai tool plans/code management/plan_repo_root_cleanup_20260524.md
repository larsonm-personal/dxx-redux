# Repo Root Cleanup 2026-05-24

## Goal

Reduce repo-root clutter on the `cmake` branch by relocating Android-owned tooling and test assets under `android/`, deleting temporary branch artifacts, and leaving a short follow-up list of any other root-level directories that should move or go away.

## Current hypothesis

- `tools/` is live branch-owned content and should move to `android/tools/` with reference updates.
- `docker/nat-testbed/` is live Android test infrastructure and should move under `android/` instead of staying at repo root.
- `test_input_demo_recorder_checkpoint_fixture/`, `test_input_demo_replay_checkpoint_fixture/`, and `test_input_demo_replay_fixture/` are not real source fixtures. They are tracked leftovers caused by input-demo tests writing `.rngtrace.jsonl` sidecars and not removing them in all paths.

## Cheap checks

1. Search for all references to the old root paths and update only the live callers.
2. Run the affected input-demo unit tests after fixing sidecar cleanup to confirm the deleted root fixture dirs are not needed.
3. Re-scan for old root path references and inspect branch-added top-level directories against the merge base.

## Work items

- [completed] Confirmed tracked ownership and reference surfaces for `tools/`, `docker/nat-testbed/`, and the input-demo fixture dirs.
- [completed] Moved `tools/` to `android/tools/` and updated live references.
- [completed] Moved `docker/nat-testbed/` to `android/docker/nat-testbed/` and updated live references.
- [completed] Deleted the three tracked input-demo fixture dirs by removing their leaked `.rngtrace.jsonl` sidecars from git, and fixed the recorder/replay tests so successful runs remove those sidecars.
- [completed] Audited other branch-added top-level directories. No other tracked top-level directories need move/delete right now. `game_data_to_copy_to_emulator/`, `server/`, `cmake/`, and `.vscode/` all have live callers or intentional repo roles.
- [completed] Focused validation passed: D1 and D2 host builds via `run-windows-build.ps1 -Target d1` and `run-windows-build.ps1 -Target d2`, `buildd1\maths\test_input_demo_recorder.exe`, `buildd1\maths\test_input_demo_replay.exe`, `buildd2\maths\test_input_demo_recorder.exe`, `buildd2\maths\test_input_demo_replay.exe`, and a stale-path scan on live callers.

## Follow-up notes

- Removed local residue carried along by the `tools/` move: the untracked `android/tools/etc2tool/build/` directory and the empty `android/tools/dem2json/` placeholder.
- Historical plan notes still mention the old paths in some places. Functional callers and active repo config now use the moved locations.