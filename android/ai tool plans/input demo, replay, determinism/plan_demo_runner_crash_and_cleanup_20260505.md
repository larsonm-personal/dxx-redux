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
- Moved replay hook artifact chatter in `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c` behind optional replay debug logging, keeping the one-line mismatch, pass/fail, and stop summaries default-on while gating full expected/actual JSON dumps and result-path prints
- Centralized the D2 fire-probe optional logging in `d2/main/input_demo_hooks.c` so `d2/main/laser.c`, `d2/main/game.c`, and `d2/main/ai2.c` now call hook helpers instead of printing replay probe lines directly from those older gameplay files
- Centralized the remaining direct escort replay goal/restore probe prints from `d2/main/escort.c` into `d2/main/input_demo_hooks.c`, leaving the old file with helper calls and keeping the existing escort debug-activity gating intact
- Centralized the remaining direct AI RNG replay probe from `d2/main/object.c` into `d2/main/input_demo_hooks.c` so that old-file AI replay logging also flows through the hook layer
- Centralized the D2 follow-path replay probe formatting from `d2/main/aipath.c` into `d2/main/input_demo_hooks.c`, leaving the old file with helper calls while preserving the existing follow-probe gating and path-state companion logs
- Centralized the D2 AI-fire replay probe formatting from `d2/main/ai2.c` into `d2/main/input_demo_hooks.c`, so the old AI firing code now just calls a hook helper for the optional replay line
- Centralized the remaining D2 motion, preserved-UI-RNG, and checkpoint-runtime replay probe formatting from `d2/main/physics.c`, `d2/main/gamerend.c`, and `d2/main/state.c` into `d2/main/input_demo_hooks.c`, leaving those older files with thin helper calls instead of owning the replay probe strings
- After this cleanup pass, the remaining D2 `Input demo replay ...` strings outside `d2/main/input_demo_hooks.c` are the intentional user-facing start/stop and error lines in `d2/main/input_demo_start.c` and `d2/main/gamecntl.c`, plus the corresponding D1 start/result handling in the D1 hook and start files
- Reviewed the remaining default-on result/status lines in `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c`; left them unconditional because they are the durable pass/fail, stop, mismatch, or artifact-failure signals rather than optional probe chatter
- Added a short crash-report triage note in `android/README.md` and `CrashLog.kt` covering Advanced-page export, the `dxx-redux breadcrumbs` section, and the `crash_error_*.txt` fallback report path when xCrash only captures partial crash info
- Follow-up review showed the Advanced page was already listing `crash_error_*` files correctly; the missing exportable fallback was that `CrashLog.appendXCrashSections()` only wrote one when xCrash supplied a nonblank `emergency` buffer, even though xCrash documents that buffer as a disk-exhaustion path rather than a general degraded-native-stub signal
- Build follow-up from `android\0_upload_to_test.ps1`: fixed the D1 `input_demo_debug_printf` declaration gap by restoring the shared debug-logging include in `d1/main/input_demo_hooks.c`, and fixed the D2 `input_demo_replay_fire_probe_active` use-before-declaration by adding a file-local forward declaration in `d2/main/input_demo_hooks.c`
- Fixed the caller cwd leak by restoring the original location in `android/tests/input_demo_host_build_guard.ps1` after nested host builds
- Validated with incremental `dxx-redux-d1` and `dxx-redux-d2` host builds, a real build-guard cwd repro from `android/tests`, and scoped `android/run-code-quality.ps1 -Fix` passes
- Revalidated the Android native slice with `gradlew.bat :app:externalNativeBuildDebug --no-daemon --console=plain`, then ran `android\run-code-quality.ps1 -Fix -Paths d1/main/input_demo_hooks.c,d2/main/input_demo_hooks.c`, and finally reran `android\0_upload_to_test.ps1`; the script built `build-outputs\dxx-redux-internal-20260506-102715-v13020.aab`, uploaded versionCode `13020`, and completed successfully with the Play track left in draft status because the app itself is still draft

Cleanup follow-ups:
