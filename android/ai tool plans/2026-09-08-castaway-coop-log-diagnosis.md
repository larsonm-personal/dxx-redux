# Castaway coop log diagnosis

Inspect the two supplied Android logs, locate post-yellow-key behavior, compare it with the successful simulation, and trace the runtime fallback without changing engine behavior

## Evidence

Downloads/debuglog_20260908_143524.txt: Castaway L7, swallow.rl2, Sun-Swallower Station. Background route analysis fails at lines 271, 353, 1203, and 1394 across starts/restores. Guide-Bot readiness becomes failed at line 3100. Yellow is collected at 14:50:09.983, line 9757; Finding RED KEY starts at line 9763 and repeats. Walls opened appears at 14:51:11.887, line 11057, without a trigger or wall ID

Downloads/debuglog_20260908_215616.txt: L7 also starts with failed metadata, then completes. L8, meta.rl2, PTMC Metamorphic Metamorph, loads at 22:05:03.122, line 8788. Background result is failed at line 8794, only 14 ms after after-robots. Guide-Bot readiness becomes failed at 22:06:21.246, line 9919. Yellow is collected at 22:09:16.969, line 12791. Finding RED KEY repeats from 22:13:22.774, line 16108, through 22:15:34.053, line 18745. Wall opened appears at 22:15:23.301, line 18644, without identity

Both sessions match the reported general symptom; L8 in the evening is a strong candidate, but the logs cannot distinguish the precise remembered incident or barrier. Neither records the stalled Guide-Bot pose, wall state, or trigger ID

## Code diagnosis

secretarea.c level_metadata_try_load_pending_cache turns a negative background result into FAILED readiness and background route analysis failed. guidebot_route.c escort_route_poll_pending_cache returns immediately for FAILED readiness. escort.c escort_set_goal_object tries the route planner, but when no route goal is available and readiness is failed, pending is false and execution reaches the classic blue/gold/red key fallback

This is consistent with key-seeking without switch guidance after metadata failure. It is not proof of a wall-state networking desync. The precise source of the background failure is absent from the supplied logs

The host simulation explicitly rescans and requires an actionable live route. Its L8 success includes switches 24, 25, 26, fly-through 43, switch 17, and fly-through 18 after gold and before red, plus incidental fly-throughs 15, 14, and 16. The Android worker/cache failure path and resulting fallback are not validated by that successful simulation

RouteMetadataBackground.kt immediately reports false for a persisted FAILED ledger entry. The very fast failures are consistent with that path, but do not prove it. The next evidence needed is the device's route_metadata_precompute.log, route_metadata_precompute.json, and route_metadata_precompute_status.json. The evening log inventories those files, including a roughly 500 KB precompute log

No engine changes or new simulation runs were made for this read-only diagnosis

## Automap follow-up

The user also saw Objectives unavailable. automap_metadata_draw_readiness emits that exact string when native readiness or Android progress is FAILED and there is no usable pending objective. This corroborates the metadata failure, rather than independently locating the physical obstruction

No adb device is connected. Downloads/route_metadata_precompute.log.txt exists but ends in August, so it cannot establish the September failure reason. Do not infer a cached failure merely from short elapsed time

Add normal GAME-category logging to RouteMetadataBackground.reportCurrent for unsuccessful current-level results, including mission/level identity and the saved failure fingerprint or fresh assessment. Also identify cancellation, exceptions, and missing results. Previously the useful detail was omitted or only sent to logcat/profiling, leaving exported game logs with a generic failed message. Preserve retry and routing behavior until the underlying worker failure is known

Validate the Kotlin change through the Android JVM test task and scoped formatting

Validation completed: scoped ktlint/code-quality and android/tests/test_gradle_unit_tests.ps1 pass. The Kotlin implementation compiles and the Android JVM suite succeeds. Logs: temp/castaway_metadata_diagnostics_quality.log and temp/castaway_metadata_diagnostics_tests.log. No APK was installed; no connected device was available

September 9 follow-up: rechecked the supplied logs and adb; no connected device and no September precompute export are available. Include result/level problem text in fresh failure diagnostics because the classification fingerprint can omit the worker error. Persist current-level failure diagnostics to the existing metadata monitor log as well as the GAME log, including cached failures and exceptions. A persistence error must not prevent the native readiness callback. Retry behavior is unchanged. Scoped formatting and Android JVM tests pass (temp/castaway_metadata_followup_quality.log and temp/castaway_metadata_followup_tests.log). No APK installed. The next diagnostic artifact is available in launcher Advanced -> Route Metadata Precompute -> Export as route_metadata_precompute.log
