# Level 7 boss-room slowdown follow-up

- Audit recording-only and Android-only work absent from the Windows timing experiment
- Reproduce the supplied input demo on an Android emulator and collect targeted profiling when possible
- Investigate projectile, explosion, texture, GPU and diagnostic costs that grow during combat
- Implement measured or directly evidenced performance fixes without changing simulation timing or outcomes
- Validate D1/D2 builds, relevant regression tests and scoped code quality; document remaining phone-specific uncertainty

## Evidence

The previous investigation in `demo-finalization-performance-20260921.md` established that the long frame times are recorded inputs. Windows replay preserves those delays; accelerated replay did not reproduce sustained expensive boss-room simulation or rendering

An Android emulator (`Nexus5X_Light_1`, x86_64, SwiftShader) replayed the supplied native D1 demo from 17:10:36 to 17:13:25 on September 21. It reached the existing terminal `level_summary.endlevel_completed` mismatch (expected false, actual true). This is performance evidence, not a successful determinism regression. The emulator has different graphics costs from the user's phone

- At replay frame 1350, the frame log reports 332,880 us total, 102,720 us rendering and 230,075 us replay work including deliberate pacing, with 202 textured polygons and 72 object draws
- A 12-second simpleperf capture collected 15,715 samples. `game_render_frame` accounts for 68.81% inclusive samples and the emulator's `glBufferDataSyncAEMU_enc` accounts for 66.17%; polygon-object rendering accounts for 44.77%. Replay stepping accounts for 2.30%
- This implicates emulator graphics transport rather than a demonstrated boss-AI hotspot. It does not identify the original phone bottleneck
- Do not infer a phone fix from streaming-buffer experiments on this emulator: `../graphics/normal_renderer_performance_survey_20260719.md` records a previous change that improved emulator performance but reduced the real phone from roughly 25 FPS to 10.5 FPS. That change was reverted; the current packed upload path is retained
- GPU timer samples on this emulator are zero, so the timer fix below is validated with a fake delayed GPU, not claimed as a measured emulator speedup

Scratch evidence: `temp/boss_baseline_android.log`, `temp/boss_baseline_simpleperf.txt`. The Android log includes gameplay after the replay finishes; exclude those later lines when analyzing the supplied demo

## Changes

1. GPU timing no longer forces an unavailable query result when its ring fills. The previous fallback could block the render thread precisely when the GPU was behind. Now it only reads available results and skips sampling while every slot is occupied. Repeated 3D-view starts before one display flip also preserve the existing active query instead of starting a nested query. This shared path serves D1 and D2 on Android, including devices where timing queries are available without explicit profiling logging
2. Recording diagnostic JSON uses direct insertion of the 360 known unique fields into a reserved ordered object. Previously each `operator[]` insertion searched the preceding fields, and fixed arrays were built and then copied. Direct array conversion removes those extra copies and preserves canonical field order. No diagnostic data or replay timing is removed
3. Android profiling now reports `record_us` for recording-state capture in normal, slow-frame and flight-recorder output. This is a subset of `sim_us`, not an additional time to sum; it does not cover late per-projectile events or all RNG trace work
4. The automation `write_bool_pref` command routes the per-frame recording option to its actual `launcher_prefs` store. Its old generic `dxx_prefs` write silently failed to enable diagnostics in integration runs. Other preference keys keep their existing store

The 3,000-frame host recording benchmark measured 1.316 s of capture after the JSON change. Prior runs were approximately 3 s, and the immediately preceding runs were approximately 4.9 s while the emulator was active. These are indicative timings under differing system load, not a controlled phone speedup claim. Finalization remained approximately 0.24 s

## Validation

- `run-windows-build.ps1 -Target both` passed
- Both engines passed all eight selected tests: six `test_input_demo_*` tests (controls, fixture, recorder, result, replay, rng_mode), `test_ogl_gpu_timer` and `test_android_slowdown_detector`
- The recorder test now checks a SHA-256 of the complete diagnostic JSON against the old encoder's output, including every key, array and canonical order
- The GPU test exercises queue saturation across 12 delayed frames, duplicate begin calls, nine retire/recycle frames including ring wraparound, and invalid/disjoint timing results. Reading an unavailable result fails the test
- Scoped mixed-language code quality passed
- Reusable Android integration: `android/game_scripts/test_input_demo_recording_profile.jsonc`, run with `android/helpers/run_test.ps1 -ScriptName test_input_demo_recording_profile.jsonc -Game d2 -Install`
- The Android test enables full per-frame state, records at least 60 frames, stops in-game, then verifies the launcher finds the saved demo with a readable header, RNG trace and classic-demo sidecar. Profiling logging is disabled at the end
- Final Android `:app:assembleDebug` passed for all configured ABIs (arm64-v8a, armeabi-v7a, x86_64), including both engines
- The final emulator integration passed all 34 steps in 36.376 s (runner exit 0). Reading the resulting JSONL independently confirmed 61 frames, a matching header count, and all 360 diagnostic fields in every frame. Sampled `record_us` values with full diagnostics were 1,587-2,332 us; earlier sub-millisecond samples were from runs without full diagnostics
- Final artifacts: `temp/boss_windows_build.log`, `temp/boss_android_build_final.log`, `temp/boss_recording_android_test.log`, `temp/boss_recording_android_profile.log`, `temp/boss_android_recorded.dximdemo`, `temp/boss_quality_final.log`

The original phone's boss-room slowdown is not proven resolved. These changes remove two concrete sources of overhead/stalls, and the added recording timing allows phone logs to distinguish capture cost from other simulation work
