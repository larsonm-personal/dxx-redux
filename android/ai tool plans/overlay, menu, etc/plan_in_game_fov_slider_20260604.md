# In-Game FOV Slider Research Plan

## Status

- [x] Surveyed the fixed projection and render zoom path in D1 and D2.
- [x] Identified render callers that must not inherit the new setting.
- [x] Surveyed graphics settings persistence, live apply, and config export.
- [x] Surveyed multiplayer host option persistence and launch plumbing.
- [x] Implemented the FOV setting.
- [x] Added practical validation notes.

## Current FOV System

The 3D library has two layers that interact:

- `d1/3d/setup.c` and `d2/3d/setup.c`
  - `g3_start_frame()` computes `Window_scale` from the current canvas size and `grd_curscreen->sc_aspect`.
  - This preserves square pixels by scaling one axis for the active canvas aspect.
- `d1/3d/matrix.c` and `d2/3d/matrix.c`
  - `g3_set_view_matrix(..., zoom)` stores `View_zoom`, copies the view matrix, then calls `scale_matrix()`.
  - `scale_matrix()` combines `Window_scale` with `View_zoom`.
  - For `View_zoom <= F1_0`, it scales Z.
  - For `View_zoom > F1_0`, it scales X and Y down equally, which is the existing "zoom out" path.
- `d1/main/render.c` and `d2/main/render.c`
  - `Render_zoom = 0x9000` is the current gameplay view zoom constant.
  - Main `render_frame()` calls pass `Render_zoom` into `g3_set_view_matrix()`.
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
  - The OpenGL projection itself is hard-coded to `perspective(90.0, 1.0, 0.1, 5000.0)`.

So the answer to "is the base option 90 degrees?" is: the GL frustum is 90 degrees, but the engine's active main-view zoom is also shaped by `Render_zoom = 0x9000`. Treat the base setting as "current/base" rather than assuming a raw degree value until the mapping is verified visually and mathematically.

## Main Render Callers

Apply the new setting only to the primary player 3D view:

- D1: `d1/main/gamerend.c:479` calls `render_frame(0)` for the main view.
- D2: `d2/main/gamerend.c:883` updates window 0 for `Viewer` and calls `render_frame(0, 0)` for the main view.

Do not apply it to:

- Automap:
  - D1: `d1/main/automap.c` calls `g3_set_view_matrix(..., am->zoom)` and initializes `am->zoom = 0x9000`.
  - D2: same pattern in `d2/main/automap.c`.
- Cockpit subviews and missile HUD views:
  - D2 `show_extra_views()` in `d2/main/gamerend.c` calls `do_cockpit_window_view()` for guided missiles, missile cameras, rear view, escort, coop, and markers.
  - D2 `d2/main/gauges.c:5043` updates `Window_rendered_data[win + 1]`, then `d2/main/gauges.c:5080` calls `render_frame(0, win + 1)`.
- D2 guided missile big-window mode:
  - `d2/main/gamerend.c:844-858` temporarily sets `Viewer = Guided_missile[Player_num]` and renders the main window as the missile.
  - This should stay base FOV because it is not the player main environment view.
- Endlevel/cutscene camera:
  - D1 and D2 `endlevel.c` call `g3_set_view_matrix(..., Render_zoom)`.
  - Leave at base unless the requirement is later expanded.
- Model preview:
  - D1 and D2 `polyobj.c` call `g3_set_view_matrix(..., 0x9000)`.

## Gameplay Behavior Hazards

Changing the zoom used by `render_mine()` can change gameplay-visible state:

- D1 `do_render_object()` appends robots/players into `Ordered_rendered_object_list`.
- D2 `do_render_object()` appends robots/players into `Window_rendered_data[window_num].rendered_objects`.
- D1 homing acquisition uses `Ordered_rendered_object_list` in `d1/main/laser.c`.
- D2 homing acquisition uses `Window_rendered_data[window_num]` in `d2/main/laser.c`.
- D1 and D2 robot warning from player fire has already been moved to view-orientation based helpers, but should still be rechecked after any render-list changes.
- Demo determinism and input-demo replay can desync if any wider-FOV render list changes homing, robot activity, or RNG-consuming paths.

Implementation should therefore separate visual drawing from gameplay render bookkeeping. Do not simply widen the single `Render_zoom` passed into normal `render_frame()` and allow the wider pass to replace the normal rendered-object list.

## Recommended Implementation Shape

Use an Android-only, visual-only FOV setting with four snapped values:

- `0`: base/current
- `100`
- `110`
- `120`

Suggested C-side model:

- Add a small shared helper in D1 and D2, likely near `render.c` or a tiny Android shared module:
  - `DXX_BASE_RENDER_ZOOM = 0x9000`
  - `int android_main_view_fov_degrees`
  - `fix android_main_view_render_zoom(void)`
  - `int android_main_view_fov_locked_to_base(void)`
- Keep `Render_zoom` as the base gameplay constant, or rename only if the diff stays small.
- Add a render context flag such as `use_visual_fov` or a helper `render_frame_with_zoom(eye_offset, window_num, zoom, update_gameplay_lists)`.
- For the normal player main view only, draw using the visual zoom but preserve gameplay render data as if the base zoom were used.

Two possible ways to preserve gameplay:

1. Preferred for correctness: perform a base-FOV gameplay visibility/list pass, then render the visual pass without updating gameplay lists.
   - The base pass can be made non-presenting if the renderer has a cheap way to suppress draw output, or it can be a targeted list build pass if that is less invasive.
   - This keeps homing and old render-dependent behavior exactly aligned to base FOV.
2. Smaller diff but riskier: let the visual pass draw wider, but gate `do_render_object()` list insertion through a base-FOV test.
   - This may preserve homing lists but does not fully preserve segment traversal side effects if wider FOV changes what `render_mine()` visits.
   - Use only after confirming no other gameplay depends on visited segments or rendered objects outside the object lists.

## Settings And Persistence

Graphics settings already use `descent.cfg` keys:

- Kotlin UI: `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt`
- Config file helpers: `android/app/src/main/java/com/dxxredux/app/SetupConfigFiles.kt`
- Live apply: `MainActivity.applyGraphicsSettingsPrefs()` calls `nativeSetGraphicsOption(...)`
- Native apply/persist: `android/app/src/main/cpp/shared/android_graphics_options.c`
- Export/import: `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt`
- D1/D2 config structs and read/write:
  - `d1/main/config.h`, `d1/main/config.c`
  - `d2/main/config.h`, `d2/main/config.c`

Add a config key such as `MainViewFov` to both D1 and D2:

- Defaults to `0` or `90/base`.
- Clamp to allowed snapped values.
- Include in `EXPORTED_CFG_KEYS`.
- Add a `GraphicsSettingsPage` section near render resolution or color depth.
- Add live apply through `nativeSetGraphicsOption("main_view_fov", value)`.
- Apply from `MainActivity.applyGraphicsSettingsPrefs()`.
- Persist through `android_graphics_options.c`.

Because `descent.cfg` is shared through the existing Android graphics path, this should save, export, import, and apply similarly to `MsaaLevel`, `TexFilt`, and `GammaLevel`.

## Multiplayer Restriction

Requirement: multiplayer non-coop servers can restrict clients to base FOV. This is a gentlemen's block, not a security boundary.

Recommended setting:

- Add host preference `host_restrict_noncoop_fov_to_base`, default `true` or `false` depending on desired default policy.
- Persist in `HostGameDefaults.Defaults` in `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`.
- Add a switch in `CreateGameDialog.kt`, visible when `mode != "coop"`.
- Include the field in:
  - online lobby `game_info`
  - LAN lobby protocol announce/join fields
  - `GameLaunchInfo`
  - `MultiplayerResumeRecord` if quick resume should recreate the same host policy
  - `SetupActivity.launchMultiplayerGame()` intent extras
  - `MainActivity` intent extraction
  - JNI auto-host globals if the native side should know the host policy

Client enforcement:

- On launch as a client, if `mode != coop` and the host policy says restricted, force the runtime main-view FOV to base.
- Preserve the user's saved graphics preference on disk. The restriction should be session-runtime only.
- On leaving the restricted session or starting another game, reapply the saved `MainViewFov` from config.

Host behavior:

- Hosts can also honor their own restriction in non-coop for consistency, or continue using their own configured FOV. Decide this explicitly before implementation.
- The user's wording says "prevent clients from adjusting it", so the minimal interpretation applies only to clients.

## Test Plan

- Unit or JVM tests:
  - `ConfigImportExport` includes and restores `MainViewFov`.
  - `HostGameDefaults` round-trips the new FOV restriction field.
  - LAN/online game info parsing preserves the restriction field and defaults safely when absent.
- Native/host tests:
  - C clamp helper accepts only base, 100, 110, 120.
  - `descent.cfg` read/write round-trips the new key in D1 and D2.
- Runtime automation:
  - Launch D1 and D2, set FOV to 120, introspect or log the runtime FOV value.
  - Enter automap and verify automap zoom/runtime context remains base.
  - In D2, fire guided missile and verify guided/missile views remain base.
  - Run input-demo regression smoke with base FOV and with visual FOV set to 120; final simulation result should match because behavior is unchanged.
- Two-player non-coop launch with restriction enabled: client runtime FOV is base while saved preference remains unchanged.

## Implementation Notes

- Added `MainViewFov` to D1 and D2 `descent.cfg` structs, read/write paths, Android runtime sync, live native graphics options, and Android config export/import.
- Added a Graphics page slider with snapped values: `Base`, `100 deg`, `110 deg`, and `120 deg`.
- Added Android-only D1/D2 render helpers for the main player view:
  - Base FOV path still calls the original `render_frame`.
  - Wider FOV path renders a base pass first for gameplay-visible rendered-object lists and demo recording, then renders a visual-only wider pass.
  - The visual-only pass does not reset or append rendered-object lists, does not record demo frames, and does not mark automap segments visited.
  - Automap, D2 guided missile big-window rendering, cockpit/missile subviews, and endlevel rendering continue to use the original base path.
- Added a persisted host option `host_restrict_noncoop_fov_to_base`, visible for non-coop hosting.
- Propagated the restriction through online `game_info`, LAN `START`, in-game LAN `ANNOUNCE`, multiplayer resume records, launch intents, and runtime native graphics locking.
- Runtime restriction applies to clients only and does not overwrite the saved `MainViewFov` preference.

## Validation Notes

- `git diff --check -- . ':!android/outstanding_bugs.md'` passed. Git reported line-ending normalization warnings on existing CRLF files only.
- Attempted `android/gradlew.bat :app:assembleDebug`, but Gradle stopped before compilation because the local shell is using Java 8 and the Android build requires Java 17 or newer.

## Open Decisions

- Confirm the user-facing label for the base option: "Base" is safest; "90" is only safe if we decide the user-visible model should refer to the hard-coded GL frustum rather than the effective old render zoom.
- Confirm whether the host in a restricted non-coop game is also forced to base, or only clients.
- Confirm whether observers in non-coop should be restricted as clients.
