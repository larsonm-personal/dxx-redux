# Robot Preview Investigation

## Goal

Determine how D1/D2 expose robot names and how the D2 Redux developer robot preview can be reused by the Android metadata viewer through the game engine.

## Plan

- [x] Locate robot name tables and determine whether they are available in normal runtime builds.
- [x] Trace the Redux developer robot preview entry point, state, rendering, controls, and cleanup.
- [x] Compare it with the existing launcher-to-engine level preview bridge.
- [x] Recommend a concrete robot-preview integration and note any missing engine-facing data.

## Result

- D1 and D2 declare `Robot_names`, but normal retail/runtime builds do not populate or link the editor `bmread.c` table parser. The HAM `robot_info` format has no robot-name field.
- D1 and D2 non-release builds expose a Coder's sandbox polygon-model viewer in `menu.c`. It rotates raw polygon model indices and renders them through `draw_model_picture`.
- The briefing renderer has the more directly reusable robot path: it resolves `Robot_info[robot_id].model_num`, rotates it, and calls `draw_model_picture`.
- The Android level-preview bridge already supplies isolated process startup, target staging/mounting, mission loading, a Surface-backed Activity, close handling, and native rendering.
- A robot preview should add a robot-aware request containing robot ID plus the originating level, load mission HAM and level HXM/POG data, resolve the model in native code, and run a lightweight draw loop without player creation, automap setup, or route analysis.
- Correct level provenance must be retained when replacement rows are aggregated because different levels can carry different HXM robot definitions and POG textures.
- Friendly base-game names require a new engine-owned canonical D1/D2 name table (or an optional `BITMAPS.TBL` parser when that uncommon editor asset exists). Added mod robots must fall back to `Robot N` unless a future metadata extension supplies names.
