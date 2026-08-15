# Robot Preview Implementation Plan

## Implementation Status

- [x] Confirm the existing developer viewer renders polygon model indexes rather than robot identities
- [x] Preserve a source level for each aggregated robot replacement row
- [x] Add validated robot preview request staging with the selected robot and source level
- [x] Add isolated D1 and D2 robot preview activities with drag rotation, reset, and close controls
- [x] Add the native mission-aware robot render loop and JNI surface/control bridge
- [x] Add Preview robot to replacement rows and report launch failures in the metadata viewer
- [x] Add request/UI/native regression coverage and run Android, native, and Windows verification

Implementation reuses the established level-preview startup request and native file so no D1/D2
engine source changes or second startup argument are needed. The launcher retains exact row/item
provenance, and the isolated preview process loads that mission and level before resolving the robot.
The initial implementation previews the mod definition; base/mod switching remains a follow-up.

## Objective

Launch a read-only engine-rendered robot preview from a robot entry in the metadata replacement browser. The preview must show the robot definition, model, and textures from the correct mission and level without starting gameplay.

## User Experience

1. Expand `Robot changes` in the metadata viewer.
2. Expand a robot entry to see its base/mod fields and a `Preview robot` action.
3. Launch a dedicated full-screen engine preview with a loading overlay.
4. Show the modded robot centered against a neutral background, slowly rotating by default.
5. Allow drag rotation, a reset action, and Back or Close to return to the same metadata browser state.
6. Keep the numeric ID visible even when a friendly name exists, such as `Lou Guard (Robot 60)`.

## Phase 1: Preserve Replacement Provenance

- Add an occurrence model pairing each robot replacement item with its source `level_num` and `level_file`.
- Replace the current blind `flatMap` aggregation with grouping by robot number and definition signature.
- Retain every level that supplied an equivalent definition.
- If the same robot ID has different HXM definitions in different levels, show separate collapsed variants with their affected levels instead of selecting one silently.
- Use the selected occurrence's level for preview because HXM models and POG textures are level-specific.

## Phase 2: Preview Request and Staging

- Add `RobotPreviewRequestStore.kt` using the same owned-cache and canonical-path protections as `LevelPreviewRequestStore`.
- Add `LevelMetadataAnalyzer.buildRobotPreviewRequestJson` so archive extraction and mission staging continue to use the existing prepared-target code.
- Use schema `dxx-robot-preview-request-v1` with:
  - `request_id`
  - `game`
  - existing source, data, mission, HOG, and staging fields
  - `level_num`
  - `level_file`
  - `robot_number`
  - `preview_write_dir`
- Validate the game, successful source level, robot number, owned request directory, isolated write directory, and canonical base-data directory before launch.

## Phase 3: Shared Android Preview Lifecycle

- Extract the reusable Surface, loading overlay, process-isolation, result, and close behavior from `LevelPreviewActivity` into a small engine-preview base Activity.
- Keep map-specific touch controls and automap actions in `LevelPreviewActivity`.
- Add D1 and D2 `RobotPreviewActivity` subclasses with a robot-specific overlay.
- Continue using a separate process and terminate it after preview so SDL, PhysFS, renderer, and engine globals always start cleanly.
- Preserve `LevelPreviewReturnRefreshGate` behavior or generalize it to an engine-preview return gate so metadata state is not needlessly refreshed.

## Phase 4: Native Robot Preview Mode

- Add shared `android_robot_preview.cpp/.h` beside `android_level_preview.cpp/.h`.
- Add a `-robot-preview-request <path>` startup argument and dispatch it from D1/D2 `inferno.c` after graphics, palette-table, and font initialization.
- Add JNI start, close, surface, and introspection hooks for the robot preview Activity.
- Reuse the level preview's safe sequence:
  1. Read and validate the request.
  2. Set an isolated PhysFS write directory.
  3. Mount staged mission files and HOGs.
  4. Run `gamedata_init`, `texmerge_init`, PIG initialization, and `init_game`.
  5. Load the requested mission.
  6. Load the source level so its palette and custom data context are available.
  7. Apply mission HAM and the source level's HXM robot definitions.
  8. Apply the source level's POG texture replacements and D1 custom replacements where applicable.
  9. Validate `robot_number < N_robot_types` and the resolved model index.
- Never create a player, open the automap, scan metadata, calculate routes, or advance game simulation.

## Phase 5: Renderer

- Resolve the model only in native code through `Robot_info[robot_number].model_num`.
- Reuse `draw_model_picture` for model sizing, paging, lighting, and polygon rendering.
- Follow `show_spinning_robot_frame` by maintaining a preview `vms_angvec` and advancing heading at a stable 60 Hz presentation timestep.
- Use a lightweight window handler for draw, close, reset, and rotation events.
- Load the level palette and explicitly apply bitmap replacements before the first frame.
- Add a small renderer helper only if zoom is required; do not duplicate polygon rendering in Kotlin or render a bitmap snapshot in the launcher.

## Phase 6: Input and Presentation

- First milestone: automatic rotation, drag-to-rotate, reset orientation, and Back or Close.
- Optional follow-up: pinch zoom implemented by adjusting the native picture view distance.
- Display friendly name, numeric robot ID, model ID, source level, and whether the preview is the base or mod definition.
- Keep controls minimal and omit the normal gameplay touch layout.

## Phase 7: Base and Mod Comparison

- Initially preview the mod definition selected from the replacement row.
- Add a Base game / Mod toggle after the single-preview path is stable.
- For base preview, load stock definitions and assets without mission HAM/HXM/POG mutations.
- For mod preview, load the exact occurrence context retained in Phase 1.
- Recreate the preview process when switching sides so mutable global model and bitmap state cannot leak between definitions.

## Phase 8: Introspection and Tests

- Expose robot preview introspection containing request ID, game, mission, level, robot number, model number, palette readiness, frame count, angle, first-frame time, and close state.
- Add unit tests for request creation, path validation, robot bounds, provenance aggregation, and differing per-level variants.
- Add native tests for robot/model selection and invalid IDs using small pure helpers.
- Add an emulator automation test that launches a stock robot preview, waits for the first frame through introspection, rotates it, resets it, closes it, and verifies return to metadata.
- Add an Obsidian test that previews a changed robot from its correct HXM/POG level and checks the expected robot/model identifiers.
- Verify D1 and D2 Windows builds, native tests, focused Android tests, scoped quality checks, and the multi-ABI APK build.

## Likely Files

- `android/app/src/main/java/com/dxxredux/app/SetupSections.kt`
- `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt`
- `android/app/src/main/java/com/dxxredux/app/RobotPreviewRequestStore.kt`
- `android/app/src/main/java/com/dxxredux/app/LevelPreviewActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/RobotPreviewActivity.kt`
- `android/app/src/main/cpp/shared/android_level_preview.cpp`
- `android/app/src/main/cpp/shared/android_robot_preview.cpp`
- `android/app/src/main/cpp/shared/android_robot_preview.h`
- `android/app/src/main/cpp/jni_main.c`
- `d1/main/inferno.c`
- `d2/main/inferno.c`

## Main Risks

- Aggregating away the level that supplied an HXM or POG replacement would preview the wrong definition.
- Bitmap replacement and polygon-model globals are mutable, so base/mod switching in one native process is unsafe.
- D1 custom replacement loading differs from D2 HXM/POG loading and needs explicit verification.
- `draw_model_picture` renders a static joint pose; animated robot articulation is a separate enhancement.
