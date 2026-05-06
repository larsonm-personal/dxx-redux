# demo runner crash, headless cwd, and cleanup -- 2026-05-05

Goal:
- diagnose or instrument the Android on-device demo replay crash so the next failure shows whether it dies before checkpoint restore, during restore, or after entering the level
- fix the headless replay helper so it does not leave the caller PowerShell session in the repo root
- capture cleanup follow-ups for temporary replay/debug logging now that the forcefield/grate sync path is stabilizing

Plan:
- [x] inspect the Android replay-start and crash breadcrumb path, then either identify the crash site or add focused replay-start breadcrumbs
- [x] inspect and fix the PowerShell cwd drift in `android/tests/run_input_demo_replay.ps1`
- [x] run focused validation for the script fix and any touched Android/native files
- [x] record cleanup tasks to move temporary replay diagnostics behind existing optional logging hooks or remove them

Status:
- No Android device or emulator was attached during this tranche, so the current tombstone could not be inspected directly
- Added focused replay-start crash breadcrumbs in `d1/main/input_demo_start.c` and `d2/main/input_demo_start.c` to distinguish: command-line handoff, replay load success, new-level launch, checkpoint temp-file write, checkpoint restore complete, and replay armed
- Android replay logs later proved the restore-time object mismatch: checkpoint restore hit a `CT_CNTRLCEN` object with `OBJ_GHOST` + `RT_NONE`, which is the hidden control-center placeholder shape, not a live reactor
- Fixed `state_object_rw_to_object()` in `d1/main/state.c` and `d2/main/state.c` to only recompute control-center gun points for live `OBJ_CNTRLCEN` + `RT_POLYOBJ` objects and to skip ghost placeholders during checkpoint restore
- Cleaned the one-off reactor restore investigation logs back out of `d1/main/state.c` and `d2/main/state.c`, leaving only the documented placeholder guard in place
- Moved replay-start checkpoint-path chatter, player-config dumps, and replay trace/result path chatter in `d1/main/input_demo_start.c` and `d2/main/input_demo_start.c` behind `input_demo_debug_printf()` while keeping failures and the final replay-start summary unconditional
- Fixed the caller cwd leak by restoring the original location in `android/tests/input_demo_host_build_guard.ps1` after nested host builds
- Validated with incremental `dxx-redux-d1` and `dxx-redux-d2` host builds, a real build-guard cwd repro from `android/tests`, and scoped `android/run-code-quality.ps1 -Fix` passes

Cleanup follow-ups:
- Audit one-off probe families still spread through `d2/main/ai.c`, `d2/main/collide.c`, `d2/main/fvi.c`, `d2/main/object.c`, `d2/main/physics.c`, `d2/main/laser.c`, `d2/main/switch.c`, and the D1 replay hook equivalents; delete probes that are now superseded by state/rng traces and gate the survivors through `android/app/src/main/cpp/shared/input_demo_debug_logging.cpp`
- Review always-on replay result/status prints in `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c` and keep only durable user-facing pass/fail lines by default, moving path dumps and trace-file chatter under `-inputdemo-debug-log`
- Add a short launcher or crash-log note documenting that replay-crash investigation should export tombstones from Advanced Settings and inspect `dxx-redux breadcrumbs` or the emergency fallback report when xCrash only reports the non-zero child exit stub
