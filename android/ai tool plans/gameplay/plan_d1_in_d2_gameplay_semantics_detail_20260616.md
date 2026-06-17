# D1-in-D2 Gameplay Semantics Detail Plan

## Goal

Turn the initial D1-in-D2 gameplay semantics survey into a concrete sequence of
small implementation tranches. The practical target is that D1 missions played
inside the D2 executable use D1 gameplay semantics where those semantics affect
demo determinism, player feel, weapon tuning, robot behavior, scoring, and save
or multiplayer safety.

The motivating near-term bug is identical weapon stats, especially spreadfire.
The longer-term goal remains demo compatibility and eventually making the
separate `d1/` tree unnecessary for Android gameplay.

## Starting Point

Survey source:

- `android/ai tool plans/gameplay/plan_d1_in_d2_gameplay_semantics_survey_20260614.md`

Current tree status after the survey:

- D1 weapon records are now read through `read_d1_weapon_info()` in
  `d2/main/d1_in_d2.c`.
- `d1_in_d2_apply_robot_assets(1)` overlays D1 robot records and imports the
  first 30 D1 weapon records into `Weapon_info`.
- Leaving D1-in-D2 calls `read_hamfile()` through
  `d1_in_d2_apply_robot_assets(0)`, which restores the D2 HAM tables.
- D1 robot aiming has a narrow helper:
  `d1_in_d2_use_d1_robot_aiming()` makes `ai_fire_laser_at_player()` use D1's
  exact aim scale.
- D1 powerup records are still skipped.
- D2 weapon slot tables, D2 powerup IDs, D2-only weapon branches, boss/reactor
  behavior, and save/demo/multiplayer provenance are not yet fully guarded.

## Code Reference Map

Core D1-in-D2 overlay:

- `d2/main/gameseq.c`: level load order, especially the D1 compatibility calls
  around bitmap/effect/vclip/sound/custom data and robot assets.
- `d2/main/d1_in_d2.c`: D1 PIG table readers, D1 robot asset overlay, current
  D1 weapon import, D1 powerup skip, sound/effect/vclip/cockpit layers.
- `d2/main/d1_in_d2.h`: public compatibility helpers and asset stats.

Weapons:

- `d2/main/weapon.h`: D2 `weapon_info`, max slot counts, `Weapon_info`.
- `d2/main/weapon.c`: slot tables, default order, ammo limits, pickup helpers,
  auto-selection, player weapon drops.
- `d2/main/laser.c`: projectile creation, speed variance, homing turn rate,
  D2-only projectile families, smart children, spreadfire emission.
- `d2/main/collide.c`: weapon collision effects, boss invulnerability tables,
  damage radius, multiplayer damage scaling.

Powerups:

- `d1/main/powerup.h` and `d2/main/powerup.h`: D1 and D2 powerup ID maps differ.
- `d2/main/powerup.c`: `do_powerup()` still contains D2-only pickup cases.
- `d2/main/fireball.c`, `d2/main/weapon.c`, `d2/main/multi.c`,
  `d2/main/gamesave.c`, `d2/main/gameseq.c`: powerup creation, drops, object
  validation, object size/vclip refresh.

Robot AI:

- `d2/main/robot.h`: D2-only robot fields.
- `d2/main/d1_in_d2.c`: D1 robot record reader and D2 tuning normalization.
- `d2/main/ai2.c`: lead player, aim, firing, weapon2 branches, movement helpers.
- `d2/main/ai.c`, `d2/main/aipath.c`: awareness, pathing, companion/thief/snipe
  behavior, boss behavior.

Boss and reactor:

- `d2/main/cntrlcen.c`: D2 countdown table and reactor firing.
- `d1/main/cntrlcen.c`: D1 countdown table and older reactor firing behavior.
- `d2/main/ai.c`, `d2/main/collide.c`, `d2/main/multibot.c`: D2 boss tables,
  gate-in, invulnerability, boss death, and special reactor behavior.

Persistence and test:

- `android/app/src/main/cpp/shared/game_introspect.cpp`: introspection output.
- `d2/main/state.c`: save/restore metadata and runtime state.
- `d2/main/newdemo.c` plus Android input-demo files: demo provenance and replay.
- `d2/main/net_udp.c`, `d2/main/multi.c`, `d2/main/multibot.c`: multiplayer
  handshake and gameplay packet assumptions.

## Tranche 0: Baseline Diagnostics And Golden Values

Status: started.

Progress 2026-06-16:

- Added `weapon_records_active` and `weapon_types` counters to
  `d1_in_d2_asset_stats`.
- Added `gameplay_trace.weapon_samples` to Android debug introspection for
  representative D1/D2 weapon IDs, including the active D2 spreadfire slot and
  D1's legacy spreadfire projectile ID 20.
- Weapon samples expose raw fixed-point speed, strength, fire wait, energy
  usage, lifetime, hit effects, homing, matter, bounce, children, and other
  D2-only fields so later checks can compare table provenance directly.
- Extended the Trine 2 D1-in-D2 automation script to assert that D1 weapon
  records are active and that spreadfire sample fields are present.

Purpose:

Prove what the current tree does before adding more gates. This keeps the
spreadfire work from becoming a broad "feels different" investigation.

Work items:

- Add a temporary or permanent introspection section for D1-in-D2 gameplay
  provenance.
- Report selected base mode:
  - `emulating_d1`
  - mission filename and mission descent version if available
  - D1 robot table active
  - D1 weapon records active
  - D1 powerup table active
- Report a compact `weapon_samples` object for:
  - laser
  - vulcan
  - spreadfire
  - plasma
  - fusion
  - concussion
  - homing
  - smart missile
  - mega missile
  - common robot projectile IDs used by the first D1 robot records
- For each sample, include:
  - weapon id
  - render type
  - speed by difficulty
  - strength by difficulty
  - energy usage
  - ammo usage
  - fire wait
  - fire count
  - lifetime
  - homing flag
  - matter
  - bounce
  - damage radius
  - flash, wall-hit, robot-hit vclips and sounds
  - D2-only fields: `speedvar`, `children`, `multi_damage_scale`, `flags`,
    `afterburner_size`
- Add a small offline comparison script or host test that reads local D1 PIG
  weapon records with the same layout as `read_d1_weapon_info()` and compares
  them to D1-in-D2 introspection output.

Acceptance:

- Trine 2 or First Strike loaded through the D2 executable reports D1 weapon
  values for spreadfire and other sampled D1 weapons.
- Native D2 reports native D2 values after leaving D1-in-D2.
- The report is stable enough for automation assertions.

## Tranche 1: Finish D1 Weapon Table Semantics

Status: partially done.

Current code:

- `d2/main/d1_in_d2.c` imports D1 weapon records with
  `read_d1_weapon_info()`.
- D1-only records are mapped into the D2 `weapon_info` shape and D2-only fields
  are initialized.
- The current import happens inside `d1_in_d2_apply_robot_assets(1)`.
- Restore uses `read_hamfile()` when D1 robot assets are deactivated.

Work items:

- Confirm the import is not accidentally dependent on robot asset loading for
  non-robot gameplay. If it is, either split weapon import into a named helper
  or document that robot asset activation is the gameplay table activation.
- Add counters to `d1_in_d2_asset_stats` or a new gameplay stats struct:
  - D1 weapon records loaded
  - D1 weapon table active
  - last D1 PIG size
  - invalid D1 weapon count reason if import fails
- Decide whether `N_weapon_types` should stay at D2's value or be treated
  through helpers:
  - Prefer keeping the global count D2-sized for first pass safety.
  - Add `d1_in_d2_weapon_count()` for introspection and validation.
  - Do not shrink the global count until every D2-only call site is gated.
- Verify D2-only defaults in `read_d1_weapon_info()`:
  - `speedvar = 128`
  - `children = SMART_ID ? PLAYER_SMART_HOMING_ID : -1`
  - `multi_damage_scale = F1_0`
  - `afterburner_size = 0`
  - `flags = 0`
  - `hires_picture = picture`
- Audit the D1 smart-missile child choice. D1 had special player and robot
  smart behavior depending on data version. The current forced child for
  `SMART_ID` should be compared against D1 `create_smart_children()`.
- Remove or gate `Netgame.OriginalD1Weapons` spreadfire hacks in D1-in-D2 once
  the imported table is proven to carry the D1 values. These hacks currently
  hard-code spreadfire speed in `d2/main/laser.c`.
- Add a regression assertion for spreadfire:
  - D1-in-D2 spreadfire speed equals the D1 PIG record.
  - D1-in-D2 spreadfire `fire_wait`, `energy_usage`, `fire_count`, `strength`,
    `lifetime`, and impact fields equal the D1 PIG record.
  - Native D2 spreadfire still equals the D2 HAM record.

Acceptance:

- Spreadfire player shots in D1-in-D2 are table-driven by the D1 record.
- Robot projectile IDs from D1 robot records use D1 speeds and damage.
- Switching from a D1-in-D2 mission to native D2 restores D2 weapon records.

## Tranche 2: D1 Weapon Slot And Inventory Views

Status: not started.

Problem:

The weapon records may be D1-like, but D2 still exposes 10 primary and 10
secondary weapon slots through global tables and many callers. D1 semantics use
5 primary slots and 5 secondary slots.

Work items:

- Add a small helper layer in `d2/main/d1_in_d2.h` and `.c`:
  - `d1_in_d2_primary_slot_count()`
  - `d1_in_d2_secondary_slot_count()`
  - `d1_in_d2_primary_slot_allowed(slot)`
  - `d1_in_d2_secondary_slot_allowed(slot)`
  - `d1_in_d2_primary_weapon_info(slot)`
  - `d1_in_d2_secondary_weapon_info(slot)`
  - `d1_in_d2_primary_powerup(slot)`
  - `d1_in_d2_secondary_powerup(slot)`
- Keep D2 global arrays unchanged in the first pass. Gate callers through the
  helper only when running D1-in-D2.
- Update or guard:
  - `player_has_weapon()`
  - `InitWeaponOrdering()`
  - `CyclePrimary()`
  - `CycleSecondary()`
  - `auto_select_weapon()`
  - `select_weapon()`
  - `pick_up_primary()`
  - `pick_up_secondary()`
  - `DropCurrentWeapon()`
  - `DropSecondaryWeapon()`
  - HUD weapon list and cockpit icon lookups in `d2/main/gauges.c`
  - touch weapon state and wheel labels if the native API exposes D2-only slots
    during D1-in-D2
- Strip or ignore D2-only weapon flags when loading D1-in-D2 player state:
  - primary slots 5 through 9
  - secondary slots 5 through 9
  - super-laser state if it can leak through laser level logic
  - omega charge and afterburner-related weapon state where irrelevant
- Add D1 default order helpers rather than changing `DefaultPrimaryOrder` and
  `DefaultSecondaryOrder`.
- Confirm D1 and D2 player files/autoselect settings remain separate and that
  launcher-side D1 autoselect does not feed D2-only slots into D1-in-D2.

Acceptance:

- D1-in-D2 cannot select, auto-select, drop, display, or receive D2-only weapon
  slots through ordinary gameplay.
- Native D2 behavior is unchanged.
- Existing Android weapon wheel tests still pass, with D1-in-D2 either using
  the D1 presentation or a documented D2 executable presentation that hides
  D2-only slots.

## Tranche 3: Import And Gate D1 Powerup Semantics

Status: not started.

Problem:

D1 powerup records are still skipped in `d2/main/d1_in_d2.c`, and D1 and D2
powerup IDs diverge. D1 has full map at ID 9 and headlight at ID 26. D2 uses
ID 33 for full map and ID 37 for headlight, with many D2-only pickups above
27.

Work items:

- Add a D1 powerup reader, even though the struct layout currently matches:
  - read exactly `D1_MAX_POWERUP_TYPES`
  - validate `num_powerups`
  - record active count separately from D2's `N_powerup_types` if shrinking
    the global count is risky
- Decide table strategy:
  - Option A: import D1 `Powerup_info[0..28]` in place and keep D2 slots above
    28 intact but inaccessible.
  - Option B: maintain a D1 table and use helper lookups for D1-in-D2.
  - Prefer Option A for art, object size, and vclip alignment, with explicit
    gates around D2-only IDs.
- Update object post-load refresh:
  - object sizes for `OBJ_POWERUP`
  - `rtype.vclip_info.vclip_num`
  - `rtype.vclip_info.frametime`
  - paging of powerup vclips
- Gate `do_powerup()` D2-only cases while D1-in-D2:
  - Gauss
  - Helix
  - Phoenix
  - Omega
  - Super laser
  - Converter
  - Ammo rack
  - Afterburner
  - D2 full map ID
  - D2 headlight ID
  - super missiles
  - guided missile
  - smart mine
  - mercury
  - earthshaker
  - hoard and CTF powerups outside network modes
- Add D1 ID remap helpers for places where level data or D2 code references
  semantic powerups:
  - `d1_in_d2_powerup_id_full_map()`
  - `d1_in_d2_powerup_id_headlight()`
  - `d1_in_d2_powerup_id_is_valid()`
  - `d1_in_d2_powerup_id_is_d2_only()`
- Audit all powerup creation and drop paths:
  - `spit_powerup()`
  - `DropCurrentWeapon()`
  - `DropSecondaryWeapon()`
  - robot and player death drops in `collide.c` and `fireball.c`
  - multiplayer create-powerup paths
  - level object validation in `gamesave.c`
- Preserve sound correctness by relying on the imported D1 hit sounds and the
  existing D1 sound layer.

Acceptance:

- D1-in-D2 full map and headlight IDs behave like D1.
- D2-only powerups cannot be picked up or introduced by D1-in-D2 gameplay.
- Existing D1 powerups retain D1 size, light, vclip, and hit sound.
- Native D2 powerups restore after leaving D1-in-D2.

## Tranche 4: D2-only Projectile Branch Audit

Status: not started.

Problem:

Even with D1 weapon records imported, D2 `laser.c` contains behavior branches
for guided missiles, omega, smart mines, earthshaker children, D2 homing scale,
weapon speed variance, and D2 child weapon tables.

Work items:

- Add helper functions rather than broad inline `EMULATING_D1` checks:
  - `d1_in_d2_weapon_uses_d1_homing_turn()`
  - `d1_in_d2_weapon_allows_d2_child_logic(id)`
  - `d1_in_d2_weapon_allows_speedvar(id)`
  - `d1_in_d2_weapon_is_d2_only_projectile(id)`
- Compare D1 and D2 `Laser_create_new()` setup:
  - speed variance
  - smart child speed reduction
  - thrust
  - projectile flags
  - lifetime jitter for flares
  - persistent/bounce handling
- Compare D1 and D2 homing:
  - D1 `HOMING_MISSILE_SCALE` is 8.
  - D2 `HOMING_MISSILE_SCALE` is 16.
  - Determine whether D1-in-D2 should use D1's scale only for D1 weapon IDs or
    for every projectile created while `EMULATING_D1`.
- Compare D1 and D2 smart child selection:
  - D1 uses player smart homing for player smart bombs and has special handling
    for older registered/shareware data.
  - D2 uses table-driven `children` and extra D2 child IDs.
  - D1-in-D2 should not spawn smart mines, robot smart mines, or earthshaker
    children from D1 weapons.
- Remove the D1 spreadfire speed hard-code under `Netgame.OriginalD1Weapons`
  from D1-in-D2 paths once table import is validated.
- Add focused assertions to the input-demo event stream or introspection:
  - created projectile id
  - parent type
  - initial velocity magnitude
  - homing scale or D1/D2 homing mode
  - children id for smart missile

Acceptance:

- D1-in-D2 projectile creation is table-driven and D1-like for all D1 weapon
  IDs.
- D2-only weapon branches are unreachable or explicitly skipped in D1-in-D2.
- Native D2 projectile behavior is unchanged.

## Tranche 5: Robot AI Neutralization And D1 Compatibility

Status: partially done.

Current code:

- `d1_in_d2_apply_robot_assets(1)` reads D1 robot records.
- `apply_d1_robot_d2_tuning()` sets `weapon_type2 = -1`, `aim = 255`, and
  preserves selected D2 `behavior` and `lightcast` values.
- `d1_in_d2_use_d1_robot_aiming()` makes D1-in-D2 robot shots use D1 aim scale
  at shot time.

Work items:

- Expand D1 robot default normalization to every D2-only field that can affect
  sim:
  - `weapon_type2 = -1`
  - `firing_wait2 = firing_wait`
  - `companion = 0`, except explicitly injected guidebot behavior
  - `thief = 0`
  - `kamikaze = 0`
  - `pursuit = 0`
  - `smart_blobs = 0`
  - `energy_blobs = 0`
  - `energy_drain = 0`
  - `death_roll = 0`
  - `deathroll_sound = -1`
  - `behavior` chosen from a documented D1-compatible source
  - `aim = 255`, with D1 aim helper still overriding the shot formula
- Audit D2-only code that can run even with zero/default fields:
  - `lead_player()`
  - `ai_fire_laser_at_player()`
  - `set_next_fire_time()`
  - homing-aware fire branch near `Weapon_info[weapon_type].homing_flag`
  - `move_towards_vector()` attack/thief/kamikaze scaling
  - pursuit/path logic in `ai.c` and `aipath.c`
- Compare D1 and D2 fire timing:
  - one weapon only
  - current-gun progression
  - rapidfire handling
  - boss-specific fire timing
- Add introspection samples for representative robots:
  - class 1 driller
  - green platform
  - red hulk
  - boss robot
  - one robot with a homing projectile
- For each robot sample, report:
  - D1 robot id
  - weapon_type and resolved weapon stats
  - D2-only field values
  - behavior
  - aim mode
  - firing waits

Acceptance:

- D1-in-D2 robots do not use D2-only special behaviors unless intentionally
  injected by the compatibility layer.
- Robot aim and projectile tuning are D1-like.
- Native D2 robot AI remains unchanged.

## Tranche 6: Boss And Reactor Compatibility

Status: not started.

Problem:

D1 and D2 boss/reactor systems diverge in countdown timing, gate lists,
teleport/cloak logic, invulnerability tables, boss death, and final reactor
side effects.

Work items:

- Add compatibility helpers:
  - `d1_in_d2_boss_flag_is_boss(robot_id)`
  - `d1_in_d2_boss_flag_is_super_boss(robot_id)`
  - `d1_in_d2_reactor_countdown_time(difficulty)`
  - `d1_in_d2_use_d1_reactor_fire_pattern()`
  - `d1_in_d2_use_d1_boss_gate_list()`
- Replace D2 countdown table in D1-in-D2:
  - D1: `{50,45,40,35,30}`
  - D2: `{90,60,45,35,30}`
- Compare D1 and D2 reactor firing:
  - extra shot probability and count
  - `Control_center_next_fire_time`
  - best-gun selection
  - control-center hit/seen wake behavior
- Compare D1 and D2 boss behavior:
  - boss flag interpretation
  - super-boss gate list
  - teleport and cloak intervals
  - invulnerable matter and energy rules
  - boss spew bots
  - boss death sequence and `special_reactor_stuff()`
- Add D1-in-D2 gates in `collide.c` around D2 boss invulnerability and spew
  tables.
- Add D1-in-D2 gates in `ai.c` and `multibot.c` around D2 boss gate and action
  tables.
- Defer multiplayer boss parity until single-player D1-in-D2 behavior is
  stable, but ensure D1-in-D2 multiplayer either rejects unsupported boss modes
  or handshakes them explicitly.

Acceptance:

- D1-in-D2 level 7 and final-boss behavior use D1 timings and gate choices.
- D1-in-D2 reactor countdown times match D1.
- D2 boss and reactor behavior remains unchanged.

## Tranche 7: Wall, Trigger, Damage, Score, And Object Semantics Survey

Status: not started.

Purpose:

The initial survey listed these areas, but weapons, powerups, AI, and
boss/reactor work should land first. This tranche is the second survey pass
after the major tables are no longer obviously wrong.

Work items:

- Compare D1 and D2 wall and trigger behavior:
  - locked boss door special case
  - secret exits
  - control-center triggers
  - one-shot and multi-use trigger flags
  - wall clip flags now overlaid by D1 compatibility
- Compare damage paths:
  - robot/player collision damage
  - weapon force
  - matter vs energy
  - multiplayer scaling
  - blast radius
- Compare score:
  - robot score values
  - hostage and powerup score values
  - difficulty modifiers if any
  - boss score
- Compare object loading:
  - invalid powerup and weapon object replacement
  - model and vclip references
  - contained object semantics for robots and players
- Produce a short follow-up plan for any found differences.

Acceptance:

- There is a ranked list of remaining non-weapon compatibility gaps after the
  core D1 gameplay tables are active.
- Every high-risk gap has a proposed helper or gate location.

## Tranche 8: Save, Demo, And Multiplayer Provenance

Status: not started.

Problem:

Gameplay semantics must be deterministic from mission selection and installed
data, or saved/replayed/remote games can silently run with different tables.

Work items:

- Add save metadata or validation:
  - mission filename
  - whether the save was D1-in-D2
  - D1 PIG variant or a compact D1 table fingerprint
  - D1 gameplay compatibility version
- On restore:
  - load mission first as today
  - verify the restored mission selects the same D1-in-D2 mode
  - verify D1 table fingerprints if recorded
  - fail with a clear message if semantics cannot match
- Add input-demo metadata:
  - selected base game
  - mission filename
  - D1-in-D2 flag
  - gameplay compatibility version
  - weapon and powerup table fingerprints
- Add multiplayer handshake fields:
  - selected base game
  - mission filename
  - D1-in-D2 flag
  - table fingerprint or asset data readiness flag
  - compatibility version
- Prefer rejecting mismatched sessions over trying to translate behavior live.

Acceptance:

- A save or replay cannot silently run D1-in-D2 data using native D2 semantics.
- Multiplayer clients cannot join a D1-in-D2 game unless their compatibility
  mode and base data match the host.

## Tranche 9: Regression Coverage

Status: started.

Progress 2026-06-16:

- Extended `android/game_scripts/test_trine2_d1_in_d2_custom_textures.json5`
  with first-pass weapon table assertions.
- These assertions are intentionally presence/provenance checks, not final
  golden-value checks. The next step is an offline D1 PIG comparison helper or
  exact expected raw values for spreadfire.

Work items:

- Add or extend one Android automation script for D1-in-D2 gameplay
  introspection. Candidate:
  - launch D2 executable
  - choose a D1 mission such as First Strike or Trine 2
  - enter level 1
  - request introspection
  - assert D1-in-D2 mode and D1 gameplay table stats
- Assertions:
  - D1 weapon records active
  - spreadfire sample equals D1 PIG values
  - one D1 robot weapon resolves to D1 weapon stats
  - D1 powerup records active once Tranche 3 lands
  - D2-only weapon slots hidden or unavailable once Tranche 2 lands
- Add a native D2 companion assertion:
  - load Counterstrike
  - verify D1 overlays are inactive
  - verify spreadfire sample equals D2 HAM values
- Once stable, record or update an input-demo smoke test:
  - pickup spreadfire or grant it through automation
  - fire a small number of shots
  - assert projectile speed/fire wait through introspection or event logs
  - replay deterministically

Acceptance:

- The detailed compatibility work has at least one repeatable regression test.
- Test failures identify which gameplay table or semantic layer is wrong.

## Recommended Order

1. Tranche 0, because the current tree already has partial weapon and AI fixes
   and needs proof before more code changes.
2. Tranche 1, because identical spreadfire and robot projectile stats are the
   highest-value near-term fix.
3. Tranche 2, because D2-only inventory slots can reintroduce wrong weapons
   even if the D1 table is correct.
4. Tranche 3, because powerup IDs and records are the next largest table
   mismatch.
5. Tranche 4, because projectile code branches are easier to audit after tables
   and slots are correct.
6. Tranche 5, because robot AI should be evaluated after robot weapons are
   correct.
7. Tranche 6, because boss/reactor behavior is isolated but high-risk.
8. Tranche 8 before any compatibility work is considered demo-safe.
9. Tranche 9 continuously, adding assertions at the end of each tranche.

## First Implementation Slice

For the immediate outstanding-bug item, keep the first slice narrow:

1. Add D1-in-D2 weapon introspection samples.
2. Add a test script assertion for spreadfire values in D1-in-D2 and native D2.
3. Verify current `read_d1_weapon_info()` output against local D1 data.
4. Fix only the mismatches found by that comparison.
5. Then decide whether to remove or gate the hard-coded spreadfire speed hacks
   in `d2/main/laser.c`.

That gives a concrete pass/fail signal for "identical weapon stats" without
waiting for powerups, boss behavior, or full D1 folder removal.
