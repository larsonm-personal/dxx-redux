# D1-in-D2 Gameplay Semantics Survey

## Goal

Survey the D2 code paths that still behave like D2 while playing D1 missions
inside the D2 executable. Identify the areas most likely to matter for "feels
like D1" correctness, such as weapon tuning, robot AI, robot behavior tables,
physics, triggers, damage, scoring, and save/network safety.

## Scope

This is a survey and design pass. Do not implement behavioral changes during
the first pass.

Files and areas to inspect:

- `d1/main` versus `d2/main` weapon code and weapon tables.
- `d1/main` versus `d2/main` robot AI code and robot table loading.
- D2 D1-emulation checks using `EMULATING_D1`.
- Existing D1-in-D2 compatibility code in `d2/main/d1_in_d2.c`.
- Level loading, object loading, wall/trigger handling, control-center logic,
  scoring, multiplayer gates, and save-game paths.
- Existing Android introspection hooks that could expose behavior provenance.

## Deliverables

- A ranked list of D1-in-D2 gameplay compatibility gaps.
- Specific code references for each gap.
- Recommended implementation phases that minimize diffs in old D2 files.
- Test and introspection recommendations.

## Status

Survey complete.

## Findings

The current D1-in-D2 mode is mainly an asset overlay. It restores D1 wall
effects, powerup vclips, robot models, robot object bitmaps, custom textures,
and base/custom D1 sounds, but several core gameplay tables and behaviors still
come from D2.

### 1. Weapon table semantics

`d2/main/d1_in_d2.c` reads the D1 robot table and then skips the D1 weapon
table entirely:

- `num_weapon_types` is validated against `D1_MAX_WEAPON_TYPES`.
- The file cursor skips `D1_MAX_WEAPON_TYPES * D1_WEAPON_INFO_SIZE`.

This is the most important gameplay gap. The player spreadfire pattern in
`d2/main/laser.c` is mostly the same as D1, but projectile speed, damage,
energy usage, fire wait, lifetime, impact vclips, hit sounds, homing flags,
matter flags, and robot projectile behavior all read from `Weapon_info`.

D2 also adds table fields and behavior that D1 does not have:

- `speedvar`
- `children`
- `multi_damage_scale`
- `afterburner_size`
- `flags`
- `hires_picture`

If we import D1 weapon records into D2's `weapon_info`, these D2-only fields
need explicit D1-safe defaults instead of stale D2 values.

### 2. Weapon inventory and slot semantics

D1 has 5 primary and 5 secondary weapon slots. D2 has 10 of each, plus super
laser, gauss, helix, phoenix, omega, flash, guided missiles, smart mines,
mercury, and earthshaker.

Important tables differ in `d1/main/weapon.c` and `d2/main/weapon.c`:

- `Primary_weapon_to_weapon_info`
- `Secondary_weapon_to_weapon_info`
- `Primary_weapon_to_powerup`
- `Secondary_weapon_to_powerup`
- `DefaultPrimaryOrder`
- `DefaultSecondaryOrder`
- `Primary_ammo_max`
- `Secondary_ammo_max`

D1-in-D2 should either expose D1 views of these tables through helper functions,
or gate every caller that can select, auto-select, drop, respawn, or display D2
weapons while `EMULATING_D1`.

### 3. Powerup table and pickup semantics

`d2/main/d1_in_d2.c` also skips the D1 powerup table. D1 and D2 powerup IDs
overlap for the early entries, but D2 moved/added several important entries.
For example, D1 has `POW_FULL_MAP` at 9 and `POW_HEADLIGHT` at 26, while D2
uses those IDs differently and places D2-specific powerups at 28 and above.

The art overlay makes powerups look right, but `do_powerup`, object load
validation, respawn limits, multiplayer powerup accounting, and player-drop
logic are still D2 semantics unless specifically guarded.

### 4. Robot weapons

D1 robot records are loaded into D2 `Robot_info`, and `weapon_type2` is forced
to `-1`. That is good, but each D1 robot `weapon_type` still indexes the active
D2 `Weapon_info` table. This means robot models can be correct while their
shots still use D2 speeds, damage, homing, hit sounds, and impact assets.

This should be fixed by the D1 weapon table import before doing deeper AI work.

### 5. Robot AI defaults and extra D2 fields

D2's robot struct has fields that D1 did not serialize:

- `weapon_type2`
- `firing_wait2`
- `companion`
- `thief`
- `kamikaze`
- `pursuit`
- `smart_blobs`
- `energy_blobs`
- `energy_drain`
- `death_roll`
- `deathroll_sound`
- `behavior`
- `aim`

Current D1 robot loading zeros the struct and fills the D1 fields, which is
mostly a safe base. The likely exception is `aim`: D2's firing code uses
`robptr->aim` to scale shot error. A zero default means maximum D2 aim error,
not the old D1 formula. D1-in-D2 should set `aim` to the D2 value that makes
the D2 formula match D1's old aim error, or guard the D2 aim block with a D1
helper.

### 6. Robot AI code shape

D2 AI has substantial D2-only branches for companion, thief, pursuit, two
weapons, energy drain, and snipe/run behavior. Many remain dormant if the
loaded D1 robot fields are zero, but this should be audited with explicit D1
neutral defaults and a small set of `EMULATING_D1` guards where the D2 code
does extra work even with zero defaults.

The highest-risk spots are:

- `set_next_fire_time` in `d2/main/ai2.c`
- `lead_player` and `ai_fire_laser_at_player` in `d2/main/ai2.c`
- awareness and chase logic in `d2/main/ai.c`
- companion/thief path code in `d2/main/aipath.c`

### 7. Boss and reactor behavior

D1 and D2 boss handling diverged. D2 uses indexed D2 boss tables for teleport,
spew, invulnerability to matter/energy, and invulnerable spots. D1 has a
different boss/super-boss flow and gate list.

The current D1 robot loader copies D1 `boss_flag` into D2 `Robot_info`. D2 boss
code expects D2 boss flag ranges in several places, so D1-in-D2 needs a small
compatibility layer for boss flag interpretation before this can be considered
complete.

Reactor behavior also differs:

- D1 countdown times are `{50,45,40,35,30}`.
- D2 countdown times are `{90,60,45,35,30}` unless per-level overrides apply.
- D2 reactor firing can emit more extra shots than D1.
- D2 has `special_reactor_stuff`.

### 8. Homing and projectile behavior

D2 has extra guided missile and omega code, a different `HOMING_MISSILE_SCALE`,
`speedvar`, smart-mine children, earthshaker children, and D2 child weapon
logic. Some of this is unreachable for D1 weapon IDs, but some is table-driven.
After importing D1 weapon records, the remaining differences should be audited
with a targeted `laser.c` D1 helper for homing turn rate and child weapon
selection.

### 9. Sounds

D1 base and custom sound replacement exists in `d2/main/d1_custom.c`, and is
activated by `d1_custom_load_data`. However, this only swaps samples by D2 sound
name/index. Gameplay tables still decide which sound number is played.

Correct sound behavior therefore depends on importing the D1 `Weapon_info`,
`Powerup_info`, `Vclip`, `Effects`, wall clips, and robot sound fields together.

### 10. Save, demo, and multiplayer safety

Any gameplay-mode state must be serialized or reconstructable:

- save/restore must know whether a save is running D1-in-D2 semantics
- demos should record enough to reject or reproduce mismatched semantics
- multiplayer should handshake D1-in-D2 mode and data readiness

The safest first version is to make the mode deterministic from mission
selection and installed data, then add explicit introspection fields that expose
which D1 overlays are active.

## Recommended Work Plan

### Phase 1: Central gameplay overlay entry points

Add `d2/main/d1_in_d2_gameplay.c` and `d2/main/d1_in_d2_gameplay.h`, or extend
`d1_in_d2.c` if we want to keep one file. Prefer the new file if it keeps old
D2 files smaller.

Provide small public helpers:

- apply/restore D1 weapon table
- apply/restore D1 powerup table
- report D1 gameplay stats for introspection
- answer D1 slot limits and table-view questions
- normalize D1 robot defaults after reading robot records

### Phase 2: Import D1 weapon records

Read the skipped D1 weapon table into D2 `weapon_info` records:

- save original D2 `Weapon_info`
- read D1's serialized layout with a D1-specific reader
- map into D2 struct fields
- set D2-only fields to neutral D1-safe values
- set `N_weapon_types` to the D1 count while active
- restore D2 data when leaving D1 missions

This should fix spreadfire tuning, most robot weapon behavior, projectile
speeds, damage, hit sounds, homing flags, and impact effects in one focused
change.

### Phase 3: D1 weapon slot gates

Add small helper checks around D2 weapon selection, auto-selection, drops, HUD
lists, and player inventory cleanup:

- D1 primary slots: 0 through 4
- D1 secondary slots: 0 through 4
- D2-only weapon flags should be ignored or stripped while loading a D1 mission
- D1 default weapon order should be used while `EMULATING_D1`

Keep the arrays themselves unchanged where possible. Prefer helper accessors so
the old D2 tables remain intact.

### Phase 4: Import D1 powerup records

Read the skipped D1 powerup table:

- save original D2 `Powerup_info`
- read D1 powerup records
- map D1 vclip numbers through the active D1 vclip overlay
- set `N_powerup_types` appropriately while active
- update loaded object sizes/frame times after applying

Then audit D2-specific `do_powerup` cases and multiplayer powerup accounting
for D1-mode gates.

### Phase 5: Robot AI neutralization

After D1 robot records are loaded, normalize every D2-only robot field:

- `weapon_type2 = -1`
- `firing_wait2 = firing_wait`
- `companion = 0`
- `thief = 0`
- `kamikaze = 0`
- `pursuit = 0`
- `smart_blobs = 0`
- `energy_blobs = 0`
- `energy_drain = 0`
- `death_roll = 0`
- `deathroll_sound = -1`
- `behavior = AIB_NORMAL` or D1-equivalent default
- `aim = D1-equivalent perfectness for D2 formula`

Then compare D1 and D2 AI code for the few places where zero defaults are not
enough.

### Phase 6: Boss and reactor compatibility

Add D1-compatible boss flag helpers and reactor tuning helpers:

- D1 boss flag interpretation
- D1 boss gate list
- D1 boss teleport/cloak timing
- D1 reactor countdown table
- D1 reactor extra-shot behavior
- D1 final boss/death handling where needed

This should be deferred until weapons and robot defaults are stable.

### Phase 7: Tests and introspection

Extend Android introspection with D1-in-D2 gameplay provenance:

- mission descent version
- D1 weapon table active
- D1 powerup table active
- D1 robot table active
- D1 sound/base replacement stats
- selected weapon table values for spreadfire and common robot weapons
- selected robot defaults for class 1 driller and other known robots

Add a low-cost validation to an existing Trine 2 or D1-in-D2 script:

- launch Trine 2 in D2
- assert D1 data readiness
- assert D1 weapon and powerup overlays are active
- assert spreadfire `Weapon_info[SPREADFIRE_ID]` equals D1-loaded values
- assert at least one D1 robot weapon type points at D1-loaded weapon info

Longer gameplay tests can follow later using input demos.
