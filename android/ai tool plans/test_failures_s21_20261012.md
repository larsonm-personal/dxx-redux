# S21 suite failure investigation, 2026-10-12

Scope: investigate the 12 failures in `temp/test_reports/report_20261012_100719.md` against execution evidence, source, and recent history

1. [x] Classify each failure using its full test log and test prerequisites
2. [x] Trace automation failures through launcher/native code and compare earlier runs
3. [x] Record confirmed causes, remaining uncertainty, and focused remediation

Do not infer engine regressions from tests that failed before exercising the engine

## Findings

The run used S21 `R3CR40Q4XPK`, isolated package `com.dxxredux.app.nsdtest`, and clean commit `11837f0b67458fcc192208aa38f08769626ef465`

Nine failures occurred in prerequisites or test setup:

| Test | Confirmed cause | Remediation |
| --- | --- | --- |
| `test_guidebot_mission_metadata` | Explicitly throws unless the serial matches `emulator-\d+` | Exclude on physical targets until audited for the isolated test app |
| `test_slider_navigation` | Same emulator-only guard | Same |
| `test_vertigo_merge_creation` | Requires a disposable emulator | Same |
| `test_classic_trigger_demo` | Mandatory `ImportedExe` and `Output` missing | Exclude caller-configured probes from the timed rotation |
| `test_pickup_autoselect_parity` | Mandatory `NativeExe`, `ImportedExe`, `D1Assets`, `D2Assets`, and `Output` missing | Same |
| `test_device_network_campaign` | Requires two selected physical serials and an output directory | Exclude from the single-device rotation |
| `test_d2xxl_texture_merge` | Missing host `android/tools/etc2tool/build/Release/etc2tool.exe` | Classify as a host test and provision its tool dependencies |
| `test_launcher_media_controls` | Stages MP3 through hardcoded `run-as com.dxxredux.app`; that package is absent on S21 | Use the shared target/package resolver in the helper |
| `test_double_launch` | Launches hardcoded `com.dxxredux.app/.SetupActivity`, then waits for the selected diagnostic package | Replace all hardcoded package/process references with the resolved test package |

The catalog already marks the three caller-configured probes as `explicit`, but `run_all_tests.ps1` bypasses `Select-TestSuiteCoverage` when `-Target45Minutes` is selected. Its default PS1 infrastructure classification is `emulator`, so these probes and the host texture conversion test enter the single-device candidate list. Physical filtering only lists three other emulator-only tests

### Mission launch cache: handoff regression

Both D1 and D2 successfully imported Uneasy4 and launched the engine, then failed waiting for the first `LAUNCHER_CONTINUE` handoff. The second launch and mod cleanup never ran

Commit `11837f0b6` changed `android/helpers/test_helpers.ps1`: the no-force-stop handoff wait previously applied only to software emulator targets; it now applies to every target and removes the clean launcher restart fallback. The October 10 passing emulator log explicitly used that fallback for both handoffs in both games

This is a direct connection to the recent mod-fuzzing work, but does not demonstrate a mission asset/cache regression. The new policy can expose an existing native-to-launcher lifecycle failure. Investigate that lifecycle and use bounded recovery that cannot interrupt an executor which has already resumed

`Unknown action: delete_mod` is a separate native parser omission. It also appears in the older passing logs and occurs after the first handoff in the source script, so it does not explain this first-handoff failure. Native parsing should nevertheless preserve launcher-only action indices

### Independent trigger axes: launcher automation/layout

Both game variants fail at step 25/63, looking for `Fire Primary` in the BRAKE assignment dialog. The hold/axis assertions already passed. The available button list contains lower choices, including `Rear View Drop Bomb`, but no `Fire Primary`. The October 10 emulator run passed both variants

The picker source still includes `Fire Primary`. `SetupAutomationApi.kt` collects labels by matching accessibility rectangles and handles scrolling; `LauncherScriptExecutor.kt` searches exact labels after resetting to the top and scrolling. This points to visibility, scrolling, or label aggregation in the phone layout rather than a demonstrated trigger-input engine failure. Exact mechanism needs a targeted accessibility trace

### Level-complete touch skip: cold route metadata

The initial level-complete touch suppression and fresh-tap checks pass. The failure is step 39/92 in the added Counterstrike level-2 guidebot switch sequence: it expects `shoot_switch` / trigger 23, but sees no active route

Failure introspection reports:

- `level_metadata_route.readiness = calculating`
- `level_metadata_route.problem = route metadata still calculating`
- `estimated_progress_permille = 871`
- Analysis cache: 65 misses, zero hits/writes/publication adoption attempts
- Guidebot: no active route, no route ticks, next waypoint pending
- `mission_asset_owner = base`, no mounted texture mods

The preceding HXM worker-reuse test explicitly clears both route and level-metadata result caches. This leaves a cold-cache dependency which the touch test does not prepare. The script issues `Next` immediately and gives the objective assertion 60 seconds. Other route tests explicitly await metadata readiness before issuing `Next`

The October 10 emulator run completed the route assertions successfully. The recent guidebot source change in `cb32b6a96` concerns Hostages commands, whereas this test requests Next. The evidence therefore points first to route calculation/readiness and test ordering; it does not establish that the trigger-23 route logic or the recent Hostages fix is broken

## Validation and remaining work

Source and original logs reviewed for all 12 failures. No production or test code changed during this investigation

Focused reruns on the existing isolated S21 APK, with full logcat capture:

- `test_levelcomplete_touch_skip`, D2: exit 1, same step 39 timeout. At failure metadata was still calculating (824/1000 progress). Full logcat shows the next-level worker being preempted by an active-level request, the active worker starting at `cpu_duty_percent=10`, and `Still calculating` when Next is selected. No completed active-level result appears before the assertion times out
- `test_mission_launch_cache`, D2: exit 1, same first handoff timeout. Full logcat records native `ENTER_LAUNCHER` at 11:35:05.806, followed by repeated `main_menu: ACTIVATED` breadcrumbs through 11:35:23, without launcher consumption. This narrows the failure to game/menu exit rather than mission import

The cache fixture resets pilots and returns to the launcher as soon as `screen_mode=menu`, without completing pilot creation. Both games' main-menu activation handlers reopen `RegisterPlayer()` when the callsign is empty, without checking `Quitting`. That is a concrete candidate for the observed activation loop, to verify before implementing an exit fix

Artifacts: `temp/s21_failure_investigation/levelcomplete.log`, `levelcomplete_logcat.log`, `mission_launch_cache.log`, and `mission_launch_cache_logcat.log`

Recommended repair order:

1. Apply caller-configured and physical-device eligibility before the 45-minute sampler; correct host-only classification
2. Route the two legacy helpers through the shared package resolver
3. Fix or safely recover the native menu-to-launcher handoff, retaining protection against stopping a resumed executor
4. Make the switch fixture explicitly prepare/await route metadata, and separate its readiness budget from the route-behavior assertion
5. Trace the phone assignment dialog's accessibility labels and scroll positions before changing trigger input code
