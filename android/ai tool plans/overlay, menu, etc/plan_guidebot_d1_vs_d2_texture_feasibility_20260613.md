# Guidebot D1 vs D2 Texture Feasibility

## Goal
Assess the smallest viable path to play D1 content, specifically Trine 2, with guidebot support.

## Checklist
- [x] Locate D2 guidebot implementation and Android touch overlay entry points
- [x] Compare D1 object/robot/spawn support against D2 guidebot dependencies
- [x] Trace D1-level-in-D2 mission/asset loading and texture mapping behavior
- [x] Identify the lower-risk implementation path and expected blast radius
- [x] Record findings and recommended next step

## Findings
- D2 already has `escort_spawn_at_player()` in `d2/main/escort.c`, and Android already routes the locked Guide wheel spawn action through `android_escort_spawn_pending` in `d2/main/gamecntl.c`.
- D1 does not have escort/companion gameplay support. Its `robot_info` lacks D2's `companion` field and its AI/collision/pathing code lacks the D2 companion branches.
- D1's Android UI intentionally hides Guide controls for `gameVariant == "d1"`, and JNI reports D1 has no Guide-Bot.
- Trine 2 is a D1 mission ZIP containing `trine2.msn`, `trine2.hog`, per-level `.rdl`, per-level `.dtx`, and `.bbm` assets.
- D1 calls `load_custom_data(level_name)`, which loads `.pg1`, `.dtx`, and `.hx1`.
- D2 only calls `load_d1_bitmap_replacements()` for emulated D1 levels or `.POG` replacement loading for normal D2 levels. It does not load Trine 2's `.dtx` files.

## Recommendation
Prioritize making D1 missions played in D2 load D1-style `.dtx` custom textures. This is likely much smaller and safer than backporting D2 guidebot behavior into D1, and it directly targets the immediate goal of playing Trine 2 with an already-working D2 guidebot spawn path.
