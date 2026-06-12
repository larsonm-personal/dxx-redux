# Guidebot Nearest-Point Navigation Plan

## Goal
- Add an Android advanced gameplay option: "Guidebot navigate to nearest point"
- When enabled, a guidebot goal with no normal escort path should route the guidebot toward the nearest reachable point along the intended progress path instead of refusing to move
- Study Castaway level 1 (`skorrpyo.rl2`) to confirm whether an exit-adjacent disappearing wall blocks normal guidebot routing

## Phase 1: Study
- [done] Locate launcher advanced gameplay preference plumbing
- [done] Locate D2 escort path creation and failure behavior
- [partial] Inspect Castaway level 1 data for exit-side wall, trigger, and route metadata behavior

## Study Notes
- `EnginePreferencesPage.kt` already has a `Gameplay` section with persisted `SharedPreferences` toggles
- Metadata travel routing treats trigger-opened walls as passable through `triggered_side_opener_count`
- Guidebot pathing uses `create_path_points`, which only traverses currently flyable/openable sides from `WALL_IS_DOORWAY` or `ai_door_is_openable`
- `escort_create_path_to_goal` rejects a partial guidebot path when the final path segment does not equal the requested goal segment, then switches to "can't reach" / scram behavior
- Castaway metadata for level 1 reports travel status ok: 5/5 travel targets reached, 3786.876 route units, 1 key detour
- The local HOG contains `skorrpyo.rl2`, but exact wall/trigger IDs were not decoded yet because the Windows shell began failing process startup with `0xC0000142`

## Phase 2: Implementation Design
- [pending] Add a persisted launcher preference under Game Preferences > Gameplay
- [pending] Bridge the preference into native runtime alongside existing Android helper options
- [pending] Add D2 escort fallback logic that preserves default behavior when disabled
- [pending] Avoid changing D1 except for shared Android preference plumbing where needed

## Phase 3: Verification
- [pending] Add or extend an automated script that launches Castaway level 1, releases/spawns guidebot if needed, asks for exit navigation, and introspects guidebot route state
- [pending] Run scoped Kotlin/native tests or the smallest Android integration path that exercises the preference and fallback
