# D1-in-D2 weapon behavior and projectile art

Date: 2026-09-07

Status: investigation and implementation plan complete; implementation pending

Source baseline: `6fda841a`, with unrelated work already in progress

## Objective and scope

Make D1 weapons behave and render like native D1 when a D1 mission runs in the
D2 executable. Deliver Spreadfire first, then finish the other weapon-specific
differences that affect damage, firing, projectiles, and replay determinism.

This is a planning-only task. No compilation, game execution, or emulator use
was performed. Investigation used source comparison and read-only Python
decoding of local game data. The reported demo was not identified or replayed,
so its exact displayed pixels remain unverified.

This plan updates the weapon portions of:

- `plan_d1_in_d2_gameplay_semantics_survey_20260614.md`
- `plan_d1_in_d2_gameplay_semantics_detail_20260616.md`

The June survey's claim that D1 weapon records are skipped is obsolete. The
current importer reads all 30 serialized records and copies them to
`Weapon_info[0..29]`. Importing the table alone has not completed weapon parity.

General robot AI, bosses, reactor rules, full powerup parity, new demo formats,
network protocol changes, and deleting `d1/` remain separate work. Keep native
D1 as the executable reference until broader compatibility is demonstrated.

## Confirmed findings

### 1. Spreadfire uses the wrong projectile record

Native D1 deliberately uses two different records:

- `d1/main/weapon.c:36`: primary slot 2 uses record **12** for energy, fire wait,
  and firing-count accounting
- `d1/main/laser.h:31`: record 12 is `XSPREADFIRE_ID`
- `d1/main/laser.h:40`: the flying projectile `SPREADFIRE_ID` is **20**
- `d1/main/laser.c:1386`: all three emitted pellets use projectile 20

D2 defines `SPREADFIRE_ID` as **12** in `d2/main/laser.h:33`, and the firing
branch at `d2/main/laser.c:2346` uses it even for D1 missions. The importer at
`d2/main/d1_in_d2.c:2172` preserves record numbers, so new D1-in-D2 shots use
D1 record 12 rather than D1 record 20.

Read-only decoding of the registered D1 PIG used by the existing Trine 2 test
confirmed these values. Fixed-point values use 65536 units per whole number:

| Field | D1 record 12: firing accounting | D1 record 20: flying projectile |
| --- | --- | --- |
| Energy usage | 32768 = 0.5 | 65536 = 1 |
| Fire wait | 13107, approximately 0.2 seconds | 13107 |
| Fire count | 1 | 1 |
| Speed, each difficulty | 7864320 = 120 | 13107200 = 200 |
| Strength, each difficulty | 655360 = 10 | 655360 = 10 |
| Lifetime | 655360 = 10 seconds | 655360 |
| Blob size | 32768 = 0.5 | 32768 |
| Render type / source bitmap | BLOB / 141 | BLOB / 141 |

The current ordinary single-player path therefore uses a speed parameter of
120 instead of 200. A blanket change of the primary slot lookup to 20 would
double energy consumption, creating another bug. Copying record 20 over 12
would also destroy the original meaning of record 12, including robot uses.

The three-pellet pattern itself already matches: gun 6, alternating horizontal
and vertical spreads of `+/- F1_0/16`, with the central pellet producing sound.
`Spreadfire_toggle` already has runtime save/restore accessors in both engines.

### 2. Flying Spreadfire art retains a D1 bitmap index in a D2 bitmap table

`read_d1_weapon_info()` reads `bitmap` at `d2/main/d1_in_d2.c:1418` and
`picture` at line 1433 without translation. The later whole-record assignment
does not remap either field; `hires_picture` receives the same raw picture ID.

The rendering path is direct:

`Weapon_info[obj->id].bitmap` -> `Laser_render()` at `d2/main/laser.c:106`
-> `draw_object_blob()` at `d2/main/object.c:329` -> `GameBitmaps[bmi.index]`

Local PIG directory decoding gives:

| Asset identity | Bitmap index |
| --- | --- |
| D1 `sprdblob` | 141 |
| D2 `gauge02#2` | 141 |
| D2 `sprdblob` | 262 |

This is a concrete asset-reference defect consistent with the reported wrong
flying sprites. Both D1 records 12 and 20 contain bitmap 141, so fixing the
projectile ID alone will leave this defect in place. The actual content of a
live slot can also be affected by palette and custom overlays; the directory
comparison is not a claim that the unidentified demo was observed drawing a
specific gauge image.

Existing overlays do not provide a general bitmap translation:

- `load_d1_bitmap_replacements()` in `d2/main/piggy.c:2118` uses
  `d2_index_for_d1_index()`, which only maps wall texture references
- `d1_in_d2_apply_powerup_vclips()` actually covers all D1 vclip indices, but
  handles their frame bitmaps, not direct blob references
- Robot object bitmaps get dedicated slots, but direct weapon blob references
  do not use `ObjBitmaps`
- D1 custom bitmap loading can resolve names, but does not rewrite imported
  `Weapon_info[].bitmap` to the resolved slot

Weapon HUD pictures have the same untranslated-reference problem. Weapon
vclips also need review: the existing overlay caps frame counts at the old D2
count and does not copy `sound_num`. Those are separate from the confirmed
Spreadfire BLOB path; do not attempt to fix Spreadfire by changing its vclip.

The April native-D2 Spreadfire OGL investigation is useful background, but
concerned a different case where bitmap 262 already resolved to `sprdblob`.
Revisit GPU upload/blend state only if the corrected D1 asset path still fails.

### 3. Quad Laser damage still applies D2's multiplier

`d2/main/laser.c:705` assigns a `3/4` multiplier to player laser bolts when
Quad Lasers are equipped, without a D1-mode gate. Native D1 at
`d1/main/laser.c:267` explicitly leaves the old multiplier code disabled.

With D1 weapon strengths imported, the D2 branch reduces each D1 quad bolt's
damage by 25 percent relative to this repository's native D1. Fix the runtime
multiplier, not the imported base damage. Include lasers at levels 1 through 4.

### 4. Robot-fired Smart Missiles select player smart children

The D1 importer sets `SMART_ID.children = PLAYER_SMART_HOMING_ID` (19).
`d2/main/laser.c:2601` uses that field for any weapon parent, including a
robot-fired Smart Missile. Native D1's `create_smart_children()` at
`d1/main/laser.c:1547` chooses by the original firing parent's type and the
available D1 weapon table.

For the registered 30-record PIG, robot-owned Smart Missiles should produce
record 29. Record 19 has strength 35 and lifetime 12 seconds; record 29 has
strength 11 through 15 by difficulty and lifetime 8 seconds. Parent-aware
selection is therefore a substantial gameplay fix. Preserve target ordering,
RNG calls, initial speed reduction, bounce interval, and ownership propagation.

### 5. Remaining weapon behavior needs a bounded comparison

- Fusion multiplayer charge scaling differs: native D1 uses a multiplayer
  scale of 2 and halves the resulting multiplier; D2 uses the single-player
  formula and its separate `multi_damage_scale`
- D2's `Netgame.OriginalD1Weapons` contains hard-coded Spreadfire speed and
  damage overrides. This is a native-D2 network option, not the D1 mission
  semantics switch, and should not override an active D1 data set
- Primary firing readiness/autoselection differs between the two engines;
  test empty Vulcan ammo, insufficient energy, and a weapon switch at a firing
  boundary before deciding which narrow compatibility branch is needed
- D2 inventory code still exposes ten slots and permits D2-only weapon paths;
  the first five ammo caps already agree with D1, so do not replace them blindly
- Several homing and collision differences are already guarded, including D1
  homing orientation scale 8, robot muzzle positioning, and explosion origins
  in `collide.c`. Verify remaining behavior rather than reimplementing fixes

## Implementation sequence

### A. Fix Spreadfire behavior and art together

1. Add a named D1 Spreadfire projectile ID and a small mode-aware projectile
   selector in the existing D1 compatibility layer. Use
   `d1_in_d2_use_d1_gameplay()` for normal play and replay alike
2. Use projectile 20 for D1 Spreadfire emission in `do_laser_firing()`, while
   retaining record 12 for primary energy/cadence/fire-count lookup. Keep
   original IDs in `Weapon_info`, robot records, live objects, and checkpoints
3. Add a D1 bitmap-reference resolver using the loaded D1 bitmap directory.
   Resolve a source index to its asset name/frame, then to an engine bitmap
   slot. For a matching named asset such as `sprdblob`, reuse the existing
   named D2 slot and install the D1 pixels through the validated bitmap loader
4. Share mappings for identical source references, including records 12, 20,
   and 23. Support a bounded, registered fallback slot for D1 assets without a
   D2 name match; coordinate allocation with existing robot, guidebot, and DXA
   virtual slots. Never hard-code 141 or 262 into engine behavior
5. Translate weapon `bitmap`, `picture`, and `hires_picture` references with
   proper zero/optional handling. Keep raw source IDs separately for provenance.
   D1 polymodel IDs already refer to the imported model table; do not apply
   bitmap-number translation to model, sound, or weapon IDs
6. Place base weapon art installation before custom bitmap overrides. The
   current level load calls `d1_custom_load_data()` before robot/weapon record
   assignment, so split preparation/installation from final assignment or
   adjust those narrow hooks. Ensure a later base install cannot overwrite a
   custom `sprdblob`; make custom index-based replacements use the same map
   for weapon assets rather than the wall-only mapper
7. Use existing palette conversion and bitmap ownership helpers. Invalidate
   stale GL textures when installing/removing pixels. Account for the shared
   5 MiB replacement arena, mapping lifetime, palette changes, and page-outs;
   do not increase the buffer speculatively
8. Stage and validate required references before publishing an active overlay.
   Report an unresolved required asset rather than silently accepting a raw
   D1 index or an unrelated D2 image

Acceptance: new and restored D1 Spreadfire projectiles use the same D1 ID,
speed, damage, size, and sprite; energy/cadence still use D1 record 12. Native
D2 continues to emit its own projectile 12 with its own HAM data and art.

### B. Complete the confirmed damage and child-selection fixes

1. Disable D2's quad damage reduction under D1 gameplay
2. Select D1 Smart Missile children by original parent type and the active D1
   source count. Use 29 for registered robot smart children and 19 for player
   children; explicitly handle a source lacking robot children without using
   the larger D2 `N_weapon_types` as evidence of D1 availability
3. Match native D1 Fusion scaling for multiplayer/co-op where supported, and
   prevent native-D2 weapon rebalance options from retuning D1 mission weapons
4. Audit weapon ID predicates in collision diagnostics and shared demo probes
   so D1 projectile 20 is identified correctly. Preserve 12/20/23 distinctions;
   do not normalize away the behavior mismatch in trace comparison

Acceptance: per-hit damage, child IDs, child lifetime, and parent ownership
match native D1 in focused scenarios. Single-player Fusion and native D2
remain stable; multiplayer parity is reported separately if not yet exercised.

### C. Finish weapon-specific execution and presentation parity

Compare all five primaries, all five secondaries, flares, robot projectiles,
and reactor projectiles against native D1. Include:

- Fire readiness, cadence, resource usage by difficulty, delayed Plasma shots,
  Spreadfire phase, Vulcan RNG order, and Fusion charge/recoil
- Spawn position/direction, model size, physics flags, mass/drag/thrust,
  lifetime, proximity parent velocity, bounce, homing acquisition/retention,
  turn cadence, and expiry
- Direct and splash damage against players, robots, and reactor; repeated
  collision handling for persistent shots and projectile collision rules
- Muzzle, flying, wall-hit, and robot-hit assets and sounds. Import full D1
  weapon-referenced vclip frame sequences/timing/sound with resolved frames
  instead of clipping to D2's old frame count
- D1's five-slot selection, cycling, autoselection, drops, and restored
  inventory. Keep D2 arrays intact and use narrow mode-aware access/gates;
  update HUD/touch consumers where necessary

Fix demonstrated differences in small tranches with exact scenario evidence.
Broader AI, powerup-ID conversion, and world-physics issues uncovered by long
replays should be recorded in their own plans, not folded into this patch.

### D. Verify activation, restoration, and persistence

- Keep the D1 source weapon count explicit; do not shrink the global D2
  `N_weapon_types` before all D2-only consumers are accounted for
- Verify D2 -> D1 -> D1 custom mission -> D2 transitions, palette changes,
  level reload, save/load, and returning after a rejected asset generation
- Restore HAM tables and release/reset translated bitmap state together;
  retain the existing guidebot asset extension and its separate slot ownership
- Verify checkpoint objects already carrying D1 projectile 20 against newly
  emitted shots. Do not change saved velocities, lifetimes, or phase to make
  old desyncing input demos pass
- Distinguish input replay (`.dximdemo`, which simulates weapons) from classic
  `.dem` rendering. Exercise each supported route; this work does not establish
  complete classic D1 demo-format support in D2

## Diagnostics and tests

Extend existing introspection, not a second weapon model in Python or Kotlin:

- Keep `weapon_samples.primary_spreadfire.id = 12` as the accounting record,
  and add an explicit effective projectile ID/sample for D1 20 or D2 12
- Report source bitmap ID/name, resolved engine bitmap ID/name, D1 base/custom
  provenance, dimensions, flags, and a deterministic pixel hash
- Expose actual emitted object ID, velocity, size, damage multiplier, lifetime,
  parent type, and selected smart child ID in a bounded weapon trace
- Report unresolved required asset counts and active source weapon count;
  `weapon_records_active = true` alone is insufficient

The existing `android/game_scripts/test_trine2_d1_in_d2_custom_textures.jsonc`
checks only that Spreadfire values are present/positive and record 12 is
valid. Retain its accounting-ID assertion, strengthen its exact data checks,
and add a focused reusable weapon integration scenario that actually fires.

| Scenario | Required evidence |
| --- | --- |
| Spreadfire, two volleys | Six objects with D1 ID 20; alternating native-D1 vectors/order; speed parameter 200; record-12 resource/cadence accounting |
| Spreadfire checkpoint mid-flight | Existing and new shots both use 20 and resolved D1 art; phase and fixed-point state match native D1 |
| Spreadfire art | D1 `sprdblob` pixels at resolved slot; size, palette, transparency, and visible in-flight result match the D1 reference |
| Custom projectile art | Custom `sprdblob` survives the complete level-load sequence, save/load, and bitmap page-out |
| Quad Lasers, levels 1-4 | Four bolts, native-D1 multiplier and per-hit damage, exact energy accounting |
| Smart Missile, player and robot owner | Children 19 versus 29 for registered D1; difficulty-dependent damage/lifetime; identical RNG and target sequence |
| Remaining primaries/secondaries and flare | Native-D1 creation and hit trace, including low resource and timing boundaries |
| Native D2 after D1 | Original weapon data, asset content, counts, and firing behavior restored |

During implementation, run relevant host builds/tests with
`run-windows-build.ps1 -Target both`, then the focused integration scenarios.
Use `android/tests/run_input_demo_replay.ps1` with a native-D1 reference and
the same recording with `-D1InD2`; compare the first differing weapon event
and RNG/state boundary. Use fresh native-D1 recordings where existing demos
already fail in native D1. Include a checkpoint with live Spreadfire, since a
start-from-level replay alone misses the old/new projectile-ID inconsistency.

Use a rendered realtime run for sprite verification. A no-render replay can
validate simulation but cannot prove the bitmap was displayed correctly.
After host checks, run Android automation serially and compare the same
introspection evidence plus targeted projectile screenshots. Run the scoped
code-quality command on changed implementation files. All builds and runtime
checks are deferred until the concurrent task no longer owns those resources.

## Evidence provenance

Read-only binary inspection used:

- D1: `game_data/CD images/Descent - Anniversary Edition (USA)/data_tracks/descent/descent.pig`
  - Size: 4920305 bytes
  - SHA-256: `093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe`
  - Weapon table begins at byte 57832; 30 records of 115 bytes
  - Bitmap directory begins at the PIG data offset plus 8; 17-byte entries
- D2: `android/temp/po2_feature_study_20260823/pmh_core/groupa.pig`
  - SHA-256: `facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b`
  - `PPIG` version 2, 2589 bitmaps; directory starts at byte 12 with 18-byte entries

Both hashes match the dependencies already pinned by the Trine 2 automation
script. These are fixture-specific golden values, not assumptions about every
PIG edition or custom mission. The older 4520145-byte registered 1.0 PIG has a
different layout and is excluded by the current registered-table importer;
adding support for that layout is separate work.

## Completion checklist

- [x] Reassess current source instead of implementing the obsolete survey
- [x] Identify Spreadfire accounting/projectile split and confirm local stats
- [x] Trace direct blob rendering and confirm D1/D2 bitmap namespace mismatch
- [x] Identify remaining Quad Laser and Smart Missile behavior differences
- [ ] Implement A: Spreadfire ID and projectile asset resolution
- [ ] Implement B: damage and child-selection parity
- [ ] Complete C: weapon execution/presentation audit and necessary fixes
- [ ] Complete D: lifecycle and checkpoint verification
- [ ] Pass focused host/Android tests and rendered projectile comparison
- [ ] Update the outstanding bug with completed scope and remaining compatibility work

The shared `android/outstanding_bugs.md` was already being edited by another
task and was not changed during this planning pass.
