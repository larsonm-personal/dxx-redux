# Merged wall two-pass geometry contract 2026-06-24

## Goal
- Restore the useful geometry coverage in the merged-wall two-pass tests.
- Avoid brittle exact or high-water vertex-count assertions.

## Plan
- [x] Reassess the previous change after user feedback.
- [x] Inspect available snapshot fields and assertion operators.
- [x] Add robust geometry assertions that prove a submitted polygon exists.
- [x] Validate the changed tests.

## Decision
- Restored geometry checks using the actual durable contract:
  - `submit_nv >= 3` means the submitted geometry is at least a triangle.
  - `bbox_area > 0` means the target face has projected area in the snapshot.
  - `merged_wall_last_draw_state.screen_area > 0` means the draw path submitted visible geometry.
- Kept the route and implementation checks so the geometry must come from `force_two_pass` / `gpu_two_pass`.

## Validation
- Parsed metadata for both changed scripts.
- Passed `test_merged_wall_two_pass_probe.json5` with `run_test.ps1 -Game d2`.
- Passed `test_merged_wall_two_pass_debug_mode_probe.json5` with `run_test.ps1 -Game d2`.
- Captured output:
  - `temp/test_merged_wall_two_pass_probe_geometry_contract_run.txt`
  - `temp/test_merged_wall_two_pass_debug_mode_probe_geometry_contract_run.txt`
