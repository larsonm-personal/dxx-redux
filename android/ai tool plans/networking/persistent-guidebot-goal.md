# Persistent guidebot goal

1. Add an launcher-local preference under Local Visual Helpers, including import/export and native application
2. Mark existing navigation messages explicitly, retaining ordinary guidebot chatter and legacy temporary owner messages when disabled
3. Cache the latest goal separately from the expiring HUD queue and reserve/draw it as the first message row
4. Replicate owner goal state reliably with ownership generation validation and periodic snapshots for late joiners; clear it across death, docking, ownership changes, level changes and restores
5. Add behavioral coverage, run scoped code quality, build both host engines and the Android app, and run an integration test

Status: complete

- Launcher preference defaults on, saves immediately and participates in launcher import/export; Restore Defaults enables it and Original Descent disables it
- Existing navigation messages use buddy_goal_message; ordinary chatter retains the original temporary behavior
- The dedicated HUD row precedes boss status and queued messages and reserves a HUD collision rectangle
- The owner caches and publishes goals even when its local preference is off; replicas never enqueue temporary messages
- Reliable state packets include owner generation and sequence, with two-second snapshots for late joiners
- Death/docking and object signature changes clear stale state; repeated pre-death snapshots cannot restore it
- Both Windows engines and the x86_64 Android APK build successfully
- CTest passed: guidebot goal lifecycle/transport, upstream compatibility and co-op world fencing
- Scoped mixed-language code quality checks passed
- Emulator integration passed all 36 steps, covering launcher preference application, unreleased state, goal lifetime beyond queue expiry, HUD rectangle reservation, runtime toggles, docking, redeployment and preference cleanup
- Emulator runner: android/helpers/run_test.ps1 -ScriptName test_guidebot_goal_message.jsonc -Game d2
- For injected-ABI Gradle builds, install android/app/build/intermediates/apk/debug/app-debug.apk with adb install -r -t; the outputs/apk path may hold an older APK
- Two-device live co-op testing has not been performed; co-op delivery/state behavior is covered by the native simulated transport test
