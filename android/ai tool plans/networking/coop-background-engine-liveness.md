# Background multiplayer engine liveness

## Contract and evidence

- Live multiplayer must continue simulation and networking for less than 20 continuous background minutes while presentation/audio/UI work is suspended
- User confirmed the host was reviewing text messages in the October 9 build 24540 reproduction
- Host activity stopped at 08:29:34; native progress stopped around 08:30:43 until 08:31:08, while default-process LAN discovery continued
- MultiplayerForegroundService runs in the default process; RuntimeGameStateBridge binds from :game to that service, which does not promote the binding client process
- Verify process classification before attributing the physical-device stall to the cached-process freezer

## Work

- [x] Extend the two-device LAN runner with a background host/client scenario that waits beyond the observed freezer delay without waking the hidden process through introspection broadcasts
- [x] Capture baseline process classification and peer liveness
- [x] Protect the game process for the lifetime of its registered multiplayer session; release protection on disconnect and service teardown
- [x] Log protection lifecycle through exported Network logs
- [x] Run the scenario for D1 and D2 and verify foreground return and cleanup
- [x] Run relevant build, unit tests, catalogs, and scoped formatting

## Validation

- Baseline build 24540: `temp/background-baseline.log` fails the background host process assertion with `oom_score_adj=900`
- `temp/background-baseline-freezer-processes.txt` records the engine PID 28788 as `cached=true`, `isFrozen=true`; default-process foreground service remained active
- The emulator initially retained the engine as the previous activity (`oom_score_adj=700`). Switching from Settings to the stock dialer after the previous-activity grace interval exposed the freezer eligibility. The runner now performs that switch itself
- Android documents that bindings promote the service process, not the binding client: https://developer.android.com/guide/components/activities/process-lifecycle
- Automated old-APK reproduction: `temp/background-automated-baseline-d2.log` loses the host at interval 9 after the Settings/dialer switch; `temp/background-automated-baseline-processes.txt` confirms `cached=true`, `isFrozen=true`
- Build and 10 targeted JVM tests passed (`RuntimeGameStateBridgeTest`, `MultiplayerBackgroundDeadlineTest`, `MultiplayerServiceLeaseStateTest`)
- Both automation catalogs and scoped formatting passed
- This combined Gradle assemble/unit-test invocation produced the current test-only APK at `android/app/build/intermediates/apk/debug/app-debug.apk`; the old `outputs/apk/debug` artifact was stale. Verified the new service in the APK and installed with `adb install -r -d -t` before post-fix validation
- Initial candidate D1-in-D2 full regression passed: `temp/background-complete-d2.log`, exit 0 at 10:00:12. Host and client each remained at `oom_score_adj=200`, with both players connected, simulation advancing, presentation stopped, and the same PID after return. Normal quit released both game service bindings
- Fixture corrections: wait for background acknowledgement, tolerate up to three attempts at the emulator's 800 ms introspection snapshot window, use launcher/back to resume the non-exported activity, and remove robots before the unattended interval so death/respawn cannot consume the quit-menu key
- Fresh-boot D1 validation exposed an incomplete fix: with reciprocal AUTO_CREATE bindings, the engine oscillated between foreground and previous/cached priority, eventually reaching `isFrozen=true` despite the live binding. Evidence: `temp/background-clean-d1.log` and `temp/background-d1-host-processes.txt`
- Make the game-to-owner IPC binding WAIVE_PRIORITY (the owner already runs a foreground service), and the owner-to-game binding IMPORTANT. This expresses protection from the foreground owner into the engine without a reciprocal priority dependency
- The binding-flag correction was also insufficient: `temp/background-verified-d1.log` lost the host after it reached cached priority 900, despite both flags being installed. Do not rely on reciprocal binding priority for engine liveness
- Final implementation: a foreground service in :game, started/stopped alongside the existing transport service by MainActivity. Both use one shared notification. The owner also stops engine protection on its deadline and teardown
- Shared notification behavior verified in AOSP ActiveServices.cancelForegroundNotificationLocked (android14-release): a notification remains while another foreground service in the same package uses its ID
- Direct foreground-service D1-in-D2 validation passed: `temp/background-direct-fgs-d2.log`, exit 0 at 10:43:50. Both engines stayed at foreground-service priority after the second app switch; each advanced simulation without rendering, retained both peers and its PID across return, and stopped its foreground service on normal quit
- Final build also makes engine-service startup failure cleanup use stopService, avoiding a background startService request when foreground startup is denied
- Native D1 validation passed on the final build: `temp/background-direct-fgs-d1.log`, exit 0 at 10:52:23. Host and client each stayed at priority 200 for the whole quiet interval, retained two connected players, advanced simulation without rendering, resumed in the same PID, and released their engine services on normal quit
- Final debug build and all 10 targeted JVM tests passed (`temp/background-final-build.log`); scoped mixed-language formatting and the comment-only follow-up passed (`temp/background-final-format.log`, `temp/background-comments-format.log`)
- Catalog validation passed: 94 standalone JSON tests, 380 support scripts, 316 top-level entries (`temp/background-catalog-final.log`, `temp/background-suite-catalog.log`)
- Physical Samsung reproduction still needs the user's next build/run; emulator results establish the missing process protection and the fix, not that every reported disconnect has this cause
