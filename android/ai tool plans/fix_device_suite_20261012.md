# Device suite fixes

1. [x] Audit discovered tests for caller configuration, emulator-only requirements, host dependencies, and hardcoded packages
2. [x] Fix selection before timed sampling and adapt reusable single-device helpers
3. [x] Diagnose and repair menu exit and phone dialog automation, preserving both games
4. [x] Prepare cold route metadata explicitly and guard metadata completion by request generation
5. [x] Complete focused S21 integrations after catalog/helper checks, formatting, and Android builds

Evidence: `test_failures_s21_20261012.md` and `temp/s21_failure_investigation/`

## Repairs

- Timed sampling now applies explicit-test exclusions before selecting scenarios, including when combined with a filter or manual inclusion
- Host texture merging builds its etc2tool dependency; host-only probes no longer require a device
- Single-device wrappers inherit the selected package and serial and permit an isolated physical test app
- APK discovery handles both Gradle output layouts and chooses the newest base/instrumentation artifact; instrumentation owner declarations cover slider, lobby, and manual-IP tests
- Both games stop retrying initial pilot creation during Android shutdown, and main-menu reactivation cannot reopen it
- Native parsing preserves indices for all currently used launcher-only actions, with catalog coverage and a mixed launcher/native handoff regression
- Launcher scrolling stops after a successful semantic scroll; the extra landscape swipe skipped fire bindings on S21
- The level-complete fixture explicitly clears route caches and awaits readiness before route assertions; the guidebot overlay fixture also prepares readiness and positions the bot beside the player before asserting base routing rather than catch-up behavior
- A cancelled previous-level metadata request could clear the new request's calculating flag, suppressing automap CPU priority; completion and teardown now synchronize generation ownership
- Removed a nonexistent SAF script action and updated a stale mod-usage wording expectation exposed by the broader JVM run

## Validation

- Android native D1/D2 builds for arm64-v8a and x86_64, base APK and instrumentation APK
- Full JVM run: 1,169 passed, one stale wording failure, one skipped; the corrected mod-usage test and five scrolling unit checks passed on rerun
- Scoped mixed-language formatter/linter passed
- Catalog, automation-action catalog, timed-sampling, target/APK selection, process-wait, runner-result/recovery, and host texture-merge checks passed
- S21 portrait controller baseline passed; landscape reproduced the failure before the scrolling fix and passed all 63 steps in both games afterward
- S21 mission-launch cache passed both games, including both returns, parser-index preservation, and mod cleanup; breadcrumbs confirm initial-pilot shutdown now unwinds
- S21 slider navigation and graphics controls passed, including new portrait/landscape binding checks
- S21 launcher media controls, double launch, Vertigo merge creation/taps, Enemy Within metadata/switch guidance, and Maximum secret-level preview passed
- S21 cold-cache level-complete integration passed all 99 steps in 26.7 seconds of native execution; logs show generation 2 cancellation ignored for generation 4 and automap duty raised to 100 percent
- S21 guidebot overlay passed all 41 steps, retaining the strict HIGH-LEVEL, BASE routing, and hidden-history assertions
- Additional S21 lobby latency, transport rebind recovery, and orphan native-session replacement checks passed

All nine automatically runnable failures from the original report passed focused reruns, including the host texture-merge test. The three caller-configured probes remain available explicitly but are excluded from unattended timed sampling. A complete new 45-minute sample and a full two-device network run were not performed

Final device evidence is in `device_results.txt`, with the last completed result for each test superseding its diagnostic iterations. `test_levelcomplete_touch_skip_complete.log` and `guidebot_overlay_complete.log` contain the final readiness-fixture passes. Interim fixtures incorrectly used a nested `field` condition and Tab to close the automap; these were corrected to `expect` and Escape, and native nested-field misuse is now checked by the catalog. These interim harness failures are separate from the confirmed metadata-generation race
