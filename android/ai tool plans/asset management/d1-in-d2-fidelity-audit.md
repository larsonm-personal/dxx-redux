# D1-in-D2 fidelity and independent asset loading audit

Audit date: 2026-09-20

Detailed implementation follow-up: [D1-in-D2 fidelity and implementation boundaries](d1-in-d2-consolidation-plan.md)

## Goals

Run D1 in the D2 engine with D1's original assets and gameplay, while allowing selected engine features such as Guide-Bot and HUD cameras. Replacing D1 presentation or rules with D2 equivalents is not a goal. Eventual operation from D1 assets alone is a prerequisite for consolidating to one engine

## Audit checklist

- [x] Trace current D1 asset loading, mapping, and presentation choices
- [x] Review gameplay, save/replay, and custom-content fidelity
- [x] Identify D2 asset dependencies in startup, launcher, and optional features
- [x] Assess recent fixes and existing validation against the stated goals
- [x] Write findings, confidence limits, and a staged direction

This is a source and asset-table survey. Existing implementation changes from the preceding fixes remain in place

## Assessment

Partially aligned. Much of the compatibility work restores original D1 assets and behavior, but the implementation is still a D1 overlay on a fully initialized D2 game. D2 supplies the underlying bitmap namespace, texture metadata, animation capacities, cockpit, and bootstrap resources. That architecture permits a hybrid presentation and prevents D1-only operation

The historical [June design](plan_d1_in_d2_full_support_design_20260613.md) explicitly chose "D2 Engine, D1 Asset Overlay" and required D2 base assets for definitions, UI, sounds, and Guide-Bot support. That assumption conflicts with the clarified objective and should no longer govern implementation decisions. The [September weapon audit](../gameplay/plan_d1_in_d2_weapon_behavior_and_projectile_art_20260907.md) and [remaining semantics audit](../gameplay/plan_d1_in_d2_remaining_semantics_audit_20260907.md) already identify several fidelity problems and remain useful inventories, subject to rechecking current code

Using a D2 runtime slot does not inherently substitute D2 artwork. The generic bitmap replacement path reads original D1 pixel data into those slots. The problem is incomplete replacement and retained D2 assumptions, not the numbering convention by itself. The mapping even attempts to preserve distinct D1 textures when the D1 PIG is present; its comments document an exception for aliases of the same rock texture. It would be incorrect to describe all texture conversion as an attempted visual upgrade

## Confirmed presentation and asset gaps

Source locations below refer to the working tree including the preceding light, reactor, and lava fixes

| Area | Current behavior and consequence | Evidence |
| --- | --- | --- |
| Cockpit and gauges | Copies and recolors D2 cockpit/gauge art into the D1 palette rather than installing original D1 cockpit/gauge art. Correct colors alone do not provide the requested original look | [d1_in_d2.c](../../../d2/main/d1_in_d2.c), `d1_in_d2_apply_cockpit`, around 1294-1362 |
| Destroyed monitors | Generic replacement protects D2 destroyed-monitor bitmaps; the effect importer replaces animation frames but not these destination images. The integration fixture explicitly expects the retained D2 destination. This is a policy mismatch, distinct from the user's decision to retain the destroyed-light palette converter | [piggy.c](../../../d2/main/piggy.c), `load_d1_bitmap_replacements`, around 2438; [test_upstream_compat.cpp](../../tests/test_upstream_compat.cpp), around 467-489 |
| Animation completeness | Effects and vclips use the smaller of D1 and original D2 frame counts. They reuse D2 frame slots and only copy selected metadata. D1 animations can therefore be shortened even when their source data is present | [d1_in_d2.c](../../../d2/main/d1_in_d2.c), `d1_in_d2_apply_effects` and `d1_in_d2_apply_powerup_vclips`, around 1911-2020 |
| Effect references and metadata | Effect sound, critical-effect, destination-effect, destination-vclip, and other fields are not comprehensively imported/remapped. Vclip `sound_num` is not copied. Moving an effect to another slot requires translating all incoming and outgoing references as well as its frames | Same functions; this establishes incomplete conversion, not a runtime reproduction of every possible consequence |
| Weapon sprites and HUD pictures | D1 weapon records retain raw D1 bitmap indices, which are subsequently used against the D2 bitmap registry. Model texture import does not solve these direct bitmap references | [d1_in_d2.c](../../../d2/main/d1_in_d2.c), `read_d1_weapon_info`, around 1458-1474; [laser.c](../../../d2/main/laser.c), `Laser_render`, around 109; [gauges.c](../../../d2/main/gauges.c), around 2091-2097 |
| Texture properties | The D1 table reader skips D1 texture metadata; bitmap replacement does not install a D1 `TmapInfo` table. The converted slots retain D2 properties used for lighting, damage, effects, and surface behavior. The recent light guards address one symptom | [d1_in_d2.c](../../../d2/main/d1_in_d2.c), around 347-352; [gamemine.c](../../../d2/main/gamemine.c), `convert_d1_tmap_num`; [collide.c](../../../d2/main/collide.c), texture-property consumers around 417-533 |
| Powerup definitions | D1 powerup records are read and validated, but the application path does not install them into `Powerup_info`. Replacing vclip frames is not equivalent to importing all pickup definitions | [d1_in_d2.c](../../../d2/main/d1_in_d2.c), around 435-444 and `d1_in_d2_apply_robot_assets` |
| Custom assets | `.pg1` and `.dtx` replacement loading exists, but resolves against converted wall indices or D2 bitmap names. Native D1 also loads `.hx1` custom definitions; the D2 D1-custom entry point does not | [d1_custom.c](../../../d2/main/d1_custom.c), around 652-747 and 862; [native custom.c](../../../d1/main/custom.c), around 737-738 |
| D1 data editions | The full table loader rejects the listed early PC/shareware PIG layouts, although lower-level texture loading and mission discovery contain support for some such editions. Finding a D1 mission is not proof that its full asset generation can load | [d1_in_d2.c](../../../d2/main/d1_in_d2.c), `open_d1_registered_pig`, around 200; [gameseq.c](../../../d2/main/gameseq.c), mandatory validation around 990 |

A read-only comparison of the local registered D1 PIG and D2 HAM confirmed concrete frame-count losses under the current clamp:

| D1 effect / texture | D1 frames | D2 target effect / texture | Frames available in D2 target |
| --- | ---: | --- | ---: |
| 10 / 333, `misc11` lava | 4 | 66 / 409 | 4 |
| 24 / 349 | 9 | 24 / 372 | 7 |
| 27 / 354 | 10 | 27 / 377 | 6 |
| 29 / 355 | 8 | 65 / 408 | 2 |

Inputs were `game_data/gog installers/descent_enUS_1_0_35122/extracted/DESCENT.PIG` and `game_data/gog installers/descent_2_enUS_1_0_51877/extracted/DESCENT2.HAM`. These contain 55 and 105 effects respectively. This validates table differences; no new screenshots or gameplay captures were produced

## Gameplay fidelity

There is substantial aligned work already: D1 robot/model/joint data, player ship physics, sound samples and sound maps, door animation definitions, homing behavior, robot aiming and scheduling, collision behavior, resource-drop rules, and explosion policy. The explicit D1 branches in `ai.c`, `ai2.c`, `physics.c`, `powerup.c`, and [d1_in_d2_semantics.c](../../../d2/main/d1_in_d2_semantics.c) show that the project is already pursuing gameplay fidelity in many areas

That coverage is incomplete. Current source confirms:

- D1 fires Spreadfire projectile record 20 while primary weapon accounting uses record 12. D2 still fires its `SPREADFIRE_ID`, record 12, after importing D1 records. Fixing this requires preserving the distinction between projectile behavior and energy/fire-rate accounting: [D1 laser.h](../../../d1/main/laser.h), [D1 weapon.c](../../../d1/main/weapon.c), [D2 laser.h](../../../d2/main/laser.h), and [D2 laser.c](../../../d2/main/laser.c), around 2420
- D2 applies the quad-laser three-quarter damage multiplier without a D1 policy check. Native D1's corresponding reduction is commented out: D2 `laser.c` around 777 and D1 `laser.c` around 267
- Imported smart-missile records assign player smart children unconditionally. Native D1 chooses robot children for robot-owned smart missiles when the source edition provides that weapon record: `d1_in_d2.c` around 1454, D2 `laser.c` around 2674, and D1 `laser.c` around 1549
- Imported robot records explicitly inherit saved D2 `behavior` and `lightcast` defaults. Their exact gameplay effect needs focused validation; this is a remaining D2 dependency rather than proof that every robot behaves incorrectly: `d1_in_d2.c`, `apply_d1_robot_d2_tuning`, around 741
- Translated D1 saves still mark a trigger disabled when `TRIGGER_ON` is absent, and discard active morph state. These are source-confirmed omissions in [d1_save_translate.c](../../../d2/main/d1_save_translate.c), around 1137 and 1478. Existing replay passes cannot establish fidelity for state the translator discards or changes
- Secret entry still executes D2 save handling before the D1-specific level-start branch: [gameseq.c](../../../d2/main/gameseq.c), `EnterSecretLevel`, around 1554-1594. The older lifecycle audit warrants dedicated entry, death, save/load, and return tests; this survey did not rerun those transitions

The older audits also flag scoring, bosses, AI path permissions, compound triggers, multiplayer semantics, and save/demo identity. Those should remain explicit follow-up areas, not be declared fixed or newly reproduced by this survey. Existing D1 briefings and mission routing are useful foundations; music, menus, fonts, endings, and full campaign audiovisual parity still need a systematic comparison

## D1-only operation

This is currently blocked at multiple layers:

1. [inferno.c](../../../d2/main/inferno.c), around 455, requires `descent2.hog` or `d2demo.hog` before normal initialization and mission selection
2. [piggy.c](../../../d2/main/piggy.c) initializes the D2 HAM, PIG, and sound resources. The D1 overlay relies on the resulting registries and definitions; removing the initial HOG check would not make it independent
3. D1 asset installation depends on original D2 bitmap slots, frame counts, cockpit art, and saved table values. Returning from D1 asset mode reloads the D2 HAM
4. Android [SetupGameFiles.kt](../../app/src/main/java/com/dxxredux/app/SetupGameFiles.kt), `d1InD2Readiness`, around 140, explicitly requires both D2 readiness and D1 readiness when the D1-in-D2 mission path is needed. General D2 launch readiness likewise selects D2 files
5. Guide-Bot preparation copies the D2 companion model and textures: `d1_in_d2.c`, `save_d2_guidebot_assets`, around 1672. The navigation feature is engine code, but the familiar robot's artwork is D2 content. The current D1 sound loader replaces the sound registry, so complete optional Guide-Bot audio also needs explicit ownership and mapping

D1-only operation and Guide-Bot support are compatible design goals if optional feature assets have an explicit source. The base D1 game must start without those assets. Installed D2 assets can supply the original Guide-Bot, or a separately chosen asset set can support it; the survey does not choose replacement artwork. HUD camera rendering should be an engine capability with D1-compatible presentation and no implicit dependency on a D2 cockpit

## Assessment of the preceding fixes

- **Lava:** aligned. The recent change finds the relocated effect and installs original D1 `misc11` frames into the D2 runtime slots. It does not upgrade D1 lava to D2 art. The problem was missed replacement, causing D2 pixels to be interpreted using the D1 palette. This fixes that case, not the broader animation/import problems above
- **Reactor:** aligned. Importing the D1 reactor definition and selecting its original live/wreck models repairs an inconsistent mixture of D2 object identity and D1 model tables
- **Unbreakable imported lights:** aligned with the user's explicit policy. The guards suppress the inherited D2 static-light destruction path while preserving D1 effect-based monitor destruction
- **Destroyed-light palette conversion:** retain, as explicitly requested. It is a correct supporting conversion for retained D2 assets and is separate from the choice to make D1 lights unbreakable. Its existence should not dictate base D1 asset selection

The earlier Windows build and 59-test CTest run passed after those fixes. Those results cover regressions and their fixtures, not complete D1 fidelity. In particular, a test that requires a retained D2 destroyed-monitor image currently enforces the wrong visual contract for the clarified goal

## Recommended direction

Treat D1 and D2 as game data and rules profiles hosted by one engine. Select the profile before bootstrapping asset-dependent UI and game tables. The D1 profile should be populated from D1 resources directly, with deterministic defaults for D2-only structure fields, rather than borrowing a live D2 initialization

Preserve original source identities, or translate them through a complete explicit registry. Either implementation can work. What matters is that every bitmap, texture property, model, weapon picture, sound, effect reference, and animation frame resolves to the intended D1 resource. D2 table counts and similarly named assets must not define the limits or fallback behavior of the D1 profile. Retain provenance in diagnostics so a rendered asset can be traced back to its game, source file, and source index

Suggested order:

1. Adopt the clarified fidelity contract and revise the active overlay-era design assumptions. Keep useful parsers, validators, D1 gameplay corrections, and recent fixes
2. Build an early D1-only vertical slice: boot and load First Strike using only D1 resources in the search path, with optional D2 features disabled. This should expose hidden dependencies before investing in more overlay patches
3. Complete the D1 asset registry and reference conversion: original cockpit/gauges and weapon pictures, full animation lengths and metadata, destroyed monitors, texture properties, powerup definitions, and custom-content precedence
4. Complete behavioral parity against native D1, beginning with the confirmed weapon and checkpoint issues. Define edition-specific expectations rather than assuming every D1 release has identical tables
5. Reattach Guide-Bot and HUD cameras as explicit engine features. Test them independently of the default D1 presentation, then test interactions with D1 rules and save/replay state
6. Consolidate engines only after the fidelity and asset-independence gates pass. Keep native D1 as a comparison implementation during migration

Acceptance evidence should include original decoded pixels and palettes, all animation frames/timing, texture and object definitions, targeted gameplay traces, fresh-start and save/load parity, campaign/secret transitions, and custom assets. Also exercise D1 -> D2 -> D1 switching to catch residual global state. Test D1-only launches with D2 archives actually absent, and record attempted resource opens so accidental external fallback cannot make the test pass

## Validation limits

This turn changed documentation only. It inspected current source, historical plans, existing test assertions, and local registered asset tables. It did not build, run new tests, launch Android, or establish full visual/gameplay parity. Findings above distinguish confirmed implementation behavior from broader areas awaiting runtime validation. No engine removal, loader redesign, or new gameplay fix was performed as part of this survey
