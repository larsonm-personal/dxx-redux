# First-run graphics chooser

Status: implemented and verified, 2026-10-05

## Experience

After the first successfully presented playable 3D level view, pause simulation and open an Android overlay titled **Choose graphics options**. Keep rendering the paused scene so every selector change can be seen behind the overlay. Offer this once per app installation, shared by D1, D2 and D1-in-D2

Opening behavior: immediately preview the highest supported values in the existing option ranges

Use a compact opaque panel matching the pause/confirmation overlay, with light dimming outside the panel. Avoid blur or a full-screen settings page, so the mine remains visible. Use three large full-width cycling rows, retaining the Video Info order and labels:

1. **Texture filtering: Trilinear**
2. **AF: 16x**
3. **MSAA: 4x**

These are example values for a capable device. Keep normal rows on one line; show a second line only for applying state, unavailable reasons or a different effective MSAA count. Row heights around 56-64 dp, text around 18-20 sp, and a panel around 400-440 dp wide provide large targets without covering the whole landscape view. Fit within safe insets; use a scrollable content region with reachable footer/actions on unusually small displays or large font settings

Show **Game paused - changes preview live** above the rows. Native acknowledgements determine displayed applied values; if applying is delayed, distinguish the selected value from the last applied value

Actions:

- **Done**: close the chooser and begin the 2.5-second quiet period before confirmation
- **Keep previous settings**: restore the accepted settings, close without another confirmation, and resume after restoration
- Back / controller B: same as Done, including confirmation if needed
- Taps outside the panel: consumed without dismissing

Footer text:

> These can be edited later live in Settings > Video Info, or in the launcher's Graphics page

Touch cycles forward through the supported choices and wraps. Controller up/down moves between rows and actions; A/center cycles or activates the focused item. Retain the existing Video Info navigation convention rather than introducing a conflicting left/right editing convention. Provide spoken row labels, current values and unavailable reasons, visible focus, and no input leakage into gameplay

## Capabilities and values

Use the active native renderer's capability report for its current EGL context/color format. Do not choose settings from device models, Android versions, or stale launcher cache alone

| Selector          | Choices                                   | Opening maximum                                 |
| ----------------- | ----------------------------------------- | ----------------------------------------------- |
| Texture filtering | Nearest, Bilinear, Trilinear              | Trilinear (`TexFilt=2`)                         |
| AF                | Off, 2x, 4x, 8x, 16x, filtered by support | Highest listed supported value, otherwise Off   |
| MSAA              | Off, 2x, 4x, filtered by support          | Highest listed supported request, otherwise Off |

The existing product/config limits are 4x requested MSAA and 16x AF. This proposal uses those limits even when the GPU supports more. Expanding MSAA beyond 4x is a separate renderer/config change, not implied by capability detection

Use the per-request MSAA support mapping already exposed as `msaa_2` / `msaa_4`. A request may allocate more samples than requested; show the effective count where different. Do not infer all supported choices from one maximum number. Share the same choice model with Video Info to keep availability and cycling consistent

An unavailable feature remains visible as a disabled row with a concise reason. Unknown capabilities delay the offer until a valid report is available; they never mean "assume maximum". Actual allocation failure uses rollback even if capability queries indicated support. "Highest supported" is a capability claim, not a performance benchmark or frame-rate guarantee

Only these three settings change. Resolution, color depth, HUD/menu filtering and other options retain their existing values

## Trigger and once-per-install state

- Trigger from a presented main gameplay view, with valid player/level state, foreground activity and usable capabilities
- Exclude startup movies, briefings, launcher/level/robot previews, automap, attract/demo playback, replay automation, metadata jobs and non-rendering modes
- Do not use `nativeIsInGame()` or a generic swap counter alone: neither proves that a playable 3D scene has actually been presented
- Tag a main-view render and consume that tag on a successful presentation for the same surface/context generation. Failed swaps, menu swaps and old generations must not trigger the offer
- If another modal or graphics trial is active, wait for it to finish and for another eligible presented gameplay frame
- Include games started from a restored save, provided the installation has not already seen the offer
- Recommended multiplayer behavior: defer until the first single-player level, retaining the pending offer. Local pause cannot freeze a network game. This is the deliberate exception to "first 3D view"; showing it during multiplayer would require a different, explicitly unpaused experience

Store a small installation-only marker under Android's `noBackupFilesDir`, outside graphics config imports/exports and profile data. It survives app updates, activity recreation and switching engines, but resets on uninstall/clear-data and is not restored onto another device. Do not use an ordinary backed-up `dxx_prefs` flag

Write the marker only when the chooser is attached and has drawn, before applying the first preview candidate. Treat that as "offered", regardless of Done, rollback or rejection. A failure before display leaves the offer pending. A crash after display rolls back through graphics recovery and does not create an automatic maximum-settings retry loop

Existing installations without the marker receive the offer once on their next eligible level. Existing user-selected settings remain the rollback baseline

## Timing, pause and confirmation

```text
Eligible main view presented
  -> acquire input and single-player pause
  -> chooser attached and drawn; record offered marker
  -> durable preview session; apply maximum candidate
  -> EDITING (each selector change applies on the game thread)
  -> Done
  -> QUIET PERIOD (2.5 seconds, scene still renders, game still paused)
  -> existing "Keep these graphics settings?" confirmation (5 seconds)
  -> OK: persist accepted selection and resume
     Cancel/timeout: restore accepted selection and resume
```

No confirmation appears while the chooser is open, even after a long idle. Done always gives a full 2.5 seconds of unobstructed paused scene before confirmation. Compute eligibility from `max(chooser_closed_at, last_option_change_at) + option_debounce_ms`, using the existing 2500 ms constant rather than a separate Kotlin copy

Keep the pause/input owner through editing, the quiet period, confirmation and rollback. Do not briefly resume combat between overlays. During the quiet period suppress flight/fire/menu-opening input, and let Back cancel the preview. Transfer held-key/touch suppression across the two overlays so the Done release cannot activate confirmation or fire a weapon

If the final candidate equals the original accepted tuple, finish without confirmation. Explicit Keep previous settings also restores without a redundant confirmation. Other changed tuples use the existing confirmation, including an all-off selection; if retaining the current all-off auto-accept policy is desired instead, decide that explicitly during implementation

## Native preview session

The existing queue is not sufficient: `android_graphics_safety_before_main_view()` currently prepares and shows confirmation before applying its queued candidate. Merely blocking the dialog would also block the requested live preview

Extend the shared graphics safety coordinator with an explicit edit session and runtime states `editing` and `settling`, followed by the existing preparing/challenge/restoring sequence. Keep one accepted rollback snapshot for the entire session

Required invariants:

1. Persist an owned recoverable attempt before applying any unaccepted GL change. Keep the accepted tuple unchanged until OK
2. Provide a store operation to update the owned preview candidate without accepting it, changing ownership or erasing recovery state when the candidate temporarily equals the baseline/all-off. Model the durable preview phase explicitly if needed; reuse the existing store and lock rather than adding a second recovery file
3. JNI receives intents only. Coalesce rapid changes to the latest complete candidate; apply at the next safe game-thread rendering boundary without waiting 2.5 seconds. Never run GL work on the Android UI thread
4. Publish requested/applied candidate revisions. Ignore stale session/revision callbacks, and allow confirmation acceptance only after the final candidate has been applied and its scene presented successfully
5. Defer launcher writes for the entire active session, including editing and settling. Existing cross-process staging must not overwrite the baseline or preview
6. Preserve the marker and rollback across renderer allocation failure, EGL loss, activity backgrounding, game exit and process death. End the preview on lifecycle interruption; activity recreation does not silently accept it or automatically reapply maximum settings
7. Keep the Android overlay responsive independently of EGL. Extend the existing UI-side recovery/watchdog to preview application and restoration, with a bounded acknowledgement timeout per candidate. Do not impose a five-second user-choice timeout on an open editor
8. Reuse the existing five-second confirmation countdown, beginning only after the confirmation overlay is drawn/armed. Its countdown and the 2.5-second quiet period are separate
9. Acquire/release simulation pause on the game thread, exactly once per owned session. Rendering and Android overlay input continue while simulation is stopped

Do not mark preview values as accepted via the ordinary config-write path. Normal exit, failure and lifecycle interruption before OK restore the accepted tuple. Reuse accepted-config protection and storage-failure reporting already present in the safety system

## Expected code boundaries

| Area                                                      | Proposed responsibility                                                                                                      |
| --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| New `GraphicsFirstRunChoices.kt`                          | Android-composited chooser, three large controls, footer, focus and lifecycle/input behavior                                 |
| New small shared graphics choice model                    | Labels, supported choice lists and cycling shared by chooser and `VideoInfoOverlay.kt`; avoid copying the full stats overlay |
| `GraphicsCapabilities.kt` / native capability report      | Context-valid supported values and effective MSAA mapping                                                                    |
| `MainActivity.kt`                                         | Installation marker, overlay wiring, input handoff and UI acknowledgements                                                   |
| `android_graphics_safety.cpp/.h`                          | Preview intents/state, pause ownership, frame acknowledgements and shared cooldown                                           |
| `graphics_safety_store.cpp/.h`                            | Durable preview candidate updates, accepted tuple protection and recovery                                                    |
| JNI bridge and `android_jni_overlay.h`                    | Session/revision commands and state notifications                                                                            |
| Shared EGL presentation hook; minimal guarded D1/D2 hooks | Correlate actual gameplay rendering with successful presentation                                                             |
| Introspection and automation                              | Observe eligibility, offered state, capabilities, session phase/revision, chooser controls, pause and rollback               |

Keep both engines on the same shared implementation. The current working tree includes unrelated `MainActivity.kt` changes for intro skipping; integrate narrowly when implementation begins

## Validation plan

- Run both D1 and D2 on a fresh installation state: no prompt in menus/briefings/previews, exactly one prompt after a presented gameplay scene, no second prompt after switching engines or relaunching
- Verify simulation stays paused while rendered frames continue; each selector change reaches the background before Done
- Exercise supported, unsupported and remapped MSAA sample counts; AF unavailable and reduced AF maxima; stale/unknown capability reports
- Wait longer than 2.5 seconds while editing: no confirmation. Close: no early confirmation, then existing confirmation appears. Rapid edits choose the latest value
- Accept, reject, timeout, Keep previous, Back, and edit back to baseline; verify requested/current/accepted configs and pause ownership
- Interrupt during first display, candidate application, settling, confirmation and restoration; verify recovery and no offer/crash loop
- Test touch, controller, keyboard/TV navigation, held inputs, small screens and large text
- Defer on multiplayer; test a later single-player launch. Exclude demo playback, headless work and unrelated previews
- Explicitly seed handled installation state in existing graphics tests; dedicated first-run tests reset only the new marker. Ordinary automation reset must not accidentally redefine real once-per-install semantics
- Extend existing high-level graphics integration coverage; register new top-level runners/scripts and their owners in the automation catalog, then run catalog checks, scoped formatting and relevant Android/host builds

## Implementation sequence

1. Add shared choice model and capability-backed controls
2. Extend durable preview sessions, recovery and independent UI watchdog behavior
3. Add presented-gameplay eligibility, installation marker, pause/input ownership and chooser
4. Connect Done/rollback to the existing confirmation with the 2.5-second settling period
5. Complete the D1/D2 integration matrix and review landscape layout and controller navigation

This is a moderate feature centered on live preview/recovery, not just an extra first-run dialog

## Implementation notes

- The chooser uses a new `GraphicsFirstRunChoices` child of `GraphicsConfirmationOverlay`, keeping a single Android overlay/input owner across editing, settling, confirmation and rollback
- `GraphicsOptionChoices` is shared with Video Info; active native capabilities supply per-request MSAA support
- A durable `GRAPHICS_SAFE_PREVIEW` store phase protects the accepted tuple while candidate revisions change
- Native completed-gameplay tags are consumed by successful EGL swaps with matching surface/context identity
- Normal automation explicitly seeds the installation marker; dedicated first-run fixtures reset it through a debug-only setup command

## Verification, 2026-10-05

- Android x86_64 debug APK builds successfully; all 1,137 JVM tests passed
- Windows D1/D2 build passed; the native graphics safety persistence/recovery CTest passed
- The first-run runner passed 20 primary cases and 20 cross-engine relaunches: Accept, Cancel, Back, Timeout, Previous, Unchanged, Background, Stall, Unsupported and Crash for both D1 and D2
- D1-in-D2 acceptance and subsequent native-D1 launch passed with the same installation marker
- Live rendering while paused, latest candidate application, the full quiet period, accepted/requested configuration protection and no gameplay input leakage are asserted by the integration runner
- The existing Video Info overlay regression passed for both engines
- Isolated level and robot previews passed for both engines with the installation marker absent throughout
- Normal and 1.3x Android text sizes were checked with screenshots and real automation touch/controller input; action buttons remain pinned and equal in height, with scrollable content and controller access to the footer
- Scoped formatting/lint and both automation catalog checks passed

Local evidence is under `android/temp/first-run-graphics-design/`: aggregate `results.json`, build/quality logs, integration logs and chooser screenshots. The registered reusable runner is `android/tests/test_graphics_first_run.ps1`; `-D1InD2` exercises the imported-D1 runtime

GPU choice mapping also has JVM coverage for unavailable features, reduced AF maxima, unavailable individual MSAA requests and remapped sample counts. This run used the ANGLE emulator, not physical-phone performance measurements or a two-device multiplayer session; multiplayer deferral is enforced by the native eligibility guard

## Rearm preference, 2026-10-05

Add "Show first boot graphics chooser" to the launcher's Graphics settings. The existing no-backup marker remains the sole source of truth: absent means checked/pending, present means unchecked/handled. Unchecking suppresses the offer; checking schedules it once again. Refresh the checkbox when the launcher resumes and refresh the native pending flag when a backgrounded game resumes. Reset any previous confirmation deadline before the next chooser session

The resume test exposed an existing lifecycle race: overlay polling could reassert foreground state between `onPause` and `onStop`. Move the activity's resumed flag reset into `onPause` so native graphics safety receives a reliable foreground transition when the game resumes

Validation passed on `emulator-5582`: the existing graphics capability UI instrumentation now covers default/on/off, persistence and native consumption on resume. The first-run integration runner's new Rearm case passed for both engines, including showing once in the same resumed process, remaining open beyond a previous confirmation deadline, and staying handled on the next engine launch. The existing Background case and its cross-engine relaunch passed for both engines as well

Android debug and instrumentation builds, scoped formatting/lint, both automation catalog checks and `git diff --check` passed. Evidence is under `android/temp/graphics-chooser-preference/`, with detailed runner logs under `android/temp/graphics-first-run-20261005-190103/` (Rearm) and `android/temp/graphics-first-run-20261005-190309/` (Background)

Both Game Preferences presets, Original Descent and Restore Defaults, now reset TexFilt to Nearest (0), MSAA to Off (0) and AF to Off (0), and re-enable the first boot graphics chooser. Their confirmation previews list these changes. The shared preset reset is exercised for both game configs and both presets by `GraphicsConfigHelpersTest`; all 14 tests, the Android debug build, scoped formatting/lint and `git diff --check` passed. Logs are under `android/temp/graphics-preset-reset/`
