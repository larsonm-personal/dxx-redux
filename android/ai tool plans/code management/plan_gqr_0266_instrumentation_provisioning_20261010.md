# GQR-0266: provision instrumentation on each selected target, 2026-10-10

Diagnosis and future implementation plan only. GQF-0280 proposed pending canonical admission; P2/high, test-gap/provisioning. Expected inherited reduction zero.

## Concrete problem

Selecting test_manual_ip_engine alone puts it in the dual-emulator tier. run_all_tests.ps1 computes needsInstrumentation only from selected single-emulator tests and excludes this suite. Its build therefore omits assembleDebugAndroidTest. Even when another selected suite enables instrumentation, preflight installs that APK only on the primary target. Tier4 installs main app/data on secondary via Install-AppAndData; nested Install-ApkOnDevice installs only DXX_TEST_APK or app-debug.apk. test_manual_ip_engine invokes RecoveryInstrumentation on emulator-5556. A freshly provisioned secondary cannot run the registered suite; a reused secondary can silently depend on a stale test APK. Source establishes missing provisioning, not an observed device failure.

## Implementation boundary

- [ ] Derive instrumentation requirement from selected tests and their actual execution targets, including dual-emulator client
- [ ] Build and retain one matching app/test APK pair for this suite invocation; coordinate package/signature selection
- [ ] Install and verify the matching test APK on every selected instrumentation target before child launch
- [ ] Preserve per-target ordinary app/data provisioning and truthful install failure handling
- [ ] Validate isolated manual-IP selection on fresh secondary, mixed suites, stale/missing APK and failed install, and both game selections

Use existing provisioning owners; prefer a small explicit test capability/target mapping over special-case build/install lists that disagree. Preserve existing single-device physical isolation policy. The two-emulator manual-IP runner assumes emulator-5554/5556; do not silently run it against a physical target. A matching test artifact must belong to the exact retained main APK, not merely an arbitrary latest AndroidTest output.

Coordinate end-to-end deadline with BR-0658: manual-IP has no aggregate override, default120s competes with instrumentation120s plus60/60/45/60s phase waits. Define complete child budget plus cleanup margin and explicit paired D1/D2 execution. Coordinate package shutdown after parent timeout with BR-0543; process-tree kill alone does not stop Android executor. These existing roots are not duplicated by GQF-0280.

Local fixture simplification: scope EngineQueryChecks socket acquisition and responder/lobbyResponder jobs through one owned cleanup boundary that survives bind/assertion/child failure; deactivate synthetic TouchOverlayView in finally on main thread. Preserve real captured native reply and wrong-source/loss/cancellation checks. Fixed sleeps should become bounded state waits only when production acceptance distinguishes completion; do not replace behavior assertions with private-field equality alone.

## Required acceptance evidence

Fresh D1/D2 manual-IP aggregate selections each build/install current matching APKs on required target and reach actual instrumentation result; no prior test APK dependency. Missing/old/incompatible/denied install fails before test execution with the responsible target. Mixed single/dual suites preserve correct targets and package identity. Deadline at/below/above bounds reaches result or controlled expiry with owned package teardown; original failure survives cleanup exceptions. No product or test implementation and no runtime/build/device execution in this tranche.


## GQ2-0297 additional selected suite scope

GQF-0280/GQR-0266 were admitted by0296 and remain OPEN/TODO. Selecting test_lobby_latency alone also requires RecoveryInstrumentation but is absent from aggregate needsInstrumentation names. It is a single-emulator suite, so it misses both test-APK build and primary install. Include this ordinary selected-suite case in the same capability/target mapping and matching-artifact plan, with fresh/stale/missing/denied install controls. No new root, rating, runtime failure or implementation. QR instrumentation120s plus setup/cleanup also needs parent deadline coordination with existingBR0658; preserve accepted package isolation and existing lifetime owners.


## GQ2-0298 slider and capability selection

Selected test_slider_navigation invokes RecoveryInstrumentation for slider_navigation (including NavigationRepeatChecks) or graphics_capabilities through its CapabilitiesOnly switch. It is omitted from needsInstrumentation. Include both supported suite variants in the existing selected-test capability/target mapping, matching app/test APK pair and fresh/stale/failed-install validation. Same GQF0280/GQR0266 root and45rating. Parent300s equals childinstrumentation300s with setup/cleanup outside; coordinate existing BR0658/BR0543 deadline and owned teardown rather than creating another root. No implementation/runtime proof.


## GQ2-0300 selected package/component consistency

The same slider/capability selected-target artifact contract also needs package identity at execution. test_slider_navigation reads selected DXX_TEST_PACKAGE for force-stop through helpers but instruments literal com.dxxredux.app.test/com.dxxredux.app.RecoveryInstrumentation. Get-TestPackageAdbArguments only scopes broadcasts and leaves instrumentation unchanged. With a nondefault selected package, a missing default test APK fails; an installed stale default can run an unrelated target outside selected force-stop cleanup. Use the selected verified app/test pair to derive the component and keep logs/stop/provisioning on that package; cover default and nondefault emulator selections, both suites, stale default sentinel and missing/incompatible matching test APK. This extends existing GQF0280/GQR0266 selected-target matching-artifact plan; no newroot/rating/runtimeclaim or permission to run this emulator-only fixture on physical devices. ExistingBR0658 deadline andBR0543 ownedexecutor teardown remain distinct.
