# D1-in-D2 remaining semantics audit

Date: 2026-09-07

Status: source and read-only fixture audit complete; implementation and runtime
validation pending

## Scope

Compare the remaining audit areas against this repository's native D1:
robot AI/pathing, level and secret-level transitions, walls/triggers, and
save/demo/multiplayer consistency. Record confirmed source differences,
existing compatibility fixes, targeted validation, and implementation order.

This is an investigation and planning task. No compilation, game execution,
or emulator use was performed. Concurrent work owns existing modified files,
including `d2/main/aipath.c` and `android/outstanding_bugs.md`; those files were
only inspected. Source references describe HEAD `6fda841a` plus the working
tree on this date. Recheck line numbers and concurrent pathing changes when
implementing.

Related plans:

- `plan_d1_in_d2_weapon_behavior_and_projectile_art_20260907.md`
- `plan_d1_in_d2_gameplay_semantics_survey_20260614.md`
- `plan_d1_in_d2_gameplay_semantics_detail_20260616.md`

## Findings and priority

The remaining work includes several concrete gameplay and replay bugs.
Loading D1 tables alone does not resolve them. P1 means a stock-game or
checkpoint correctness blocker; P2 means a confirmed compatibility gap or a
validation gap. These priorities are implementation guidance, not a claim
that runtime reproductions were performed.

| ID | Priority | Finding | Reach established in this audit |
| --- | --- | --- | --- |
| A1 | P1 | Translated D1 checkpoints disable valid triggers | All 19 triggers in the four checked-in D1 checkpoint replays |
| A2 | P1 | D1 secret entry, death, save/load, and return routing retain D2 rules | Stock secret exits in levels 10, 21, and 24; direct secret starts and translated checkpoints |
| A3 | P2 | D1 AI behavior numbers invoke D2 behavior; door path permissions differ | HIDE/FOLLOW_PATH require custom fixtures; brain/run-from and melee initialization need focused stock cases |
| A4 | P2 | Trigger conversion discards combined actions and damage/drain actions | Supported by native D1 code; absent from the inspected stock HOG |
| A5 | P1 | Checkpoint translation discards runtime state used by weapons and morphing robots | Explicit skips in the translator; requires checkpoints captured during those states |
| A6 | P2 | Translated replay startup bypasses metadata agreement checks | D1 checkpoint branch returns before the normal mission/level/difficulty comparison |
| A7 | P2 | Save/replay/network identity does not establish matching effective D1 semantics | Mission name, engine version, and segment checksum do not identify imported gameplay tables |
| A8 | Feature gap | Native D1 classic `.dem` playback is still unsupported in D2 | Explicit game-type rejection in the classic demo reader |

Keep the requested Spreadfire and weapon work as the first gameplay slice.
A1 and A5 must also be addressed before checkpoint replays can serve as
reliable evidence for broader parity. The previously identified boss,
reactor, score, and powerup-table issues remain open; see the related plans
and the implementation order below.

## Read-only stock and replay evidence

Inspected registered D1 data:

- File: `game_data/CD images/Descent - Anniversary Edition (USA)/data_tracks/descent/descent.hog`
- Size: 6,856,701 bytes
- SHA-256: `83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052`
- 30 RDL files: 27 normal levels and 3 secret levels, all level version 1 /
  game-data version 25

A read-only Python scan followed the HOG directory and the engine's RDL
object/trigger layouts. Object records were decoded by movement, control,
and render type; each object section ended exactly at its wall-section
offset. Trigger counts, link counts, and file bounds were checked.

| Stock trigger flags | Meaning | Count |
| --- | --- | ---: |
| `0x001` | Door control | 39 |
| `0x008` | Normal exit | 30 |
| `0x040` | Matcen | 102 |
| `0x100` | Secret exit | 3 |

All 174 stock triggers lack `TRIGGER_ON` (`0x010`). None have one-shot,
shield-damage, energy-drain, or combined action flags. The secret triggers
are `level10.rdl` trigger 6, `level21.rdl` trigger 1, and `level24.rdl`
trigger 1; indices are zero based.

The stock CT_AI behavior counts are: zero/default 50, STILL 669, NORMAL 415,
RUN_FROM 56, and STATION 1,037. There are no serialized HIDE (`0x82`) or
FOLLOW_PATH (`0x84`) robots in this HOG. Thus A3's numeric alias problem is
confirmed in code, but this stock-data scan is not a reproduction of it.

The four checked-in D1 input demos all start from a DGSS v15 checkpoint.
Their checkpoint payloads were base64-decoded, zlib-decompressed, and
verified against each recorded size and SHA-256. The saved world records
contain:

| Fixture under `android/regression_demos/` | Triggers | Flags present | Would receive `TF_DISABLED` |
| --- | ---: | --- | ---: |
| `d1_descent_level5_20260616_202713.dximdemo` | 4 | 3 matcen, 1 exit | 4 |
| `d1_descent_level15_20260617_154210.dximdemo` | 2 | 1 matcen, 1 exit | 2 |
| `d1_descent_level16_20260618_201843.dximdemo` | 6 | 1 door, 4 matcen, 1 exit | 6 |
| `d1_descent_level18_20260618_202117.dximdemo` | 7 | 6 matcen, 1 exit | 7 |

This inspection used the native packed records: player 116 bytes, object
264 bytes, wall 24 bytes, active door 16 bytes, and trigger 54 bytes.
Checkpoint level, player object, wall side, and trigger link bounds were
checked. It did not execute the translator or replay any frames.
None of these four checkpoints contains an active morph object or active
door; level 15 has one live weapon and the other three have none. They are
insufficient coverage for A5 or secret-level transitions.

## A1. Valid checkpoint triggers become disabled

Evidence:

- Native D1 loads v25 trigger flags directly in `d1/main/gamesave.c:912`,
  and writes them unchanged to saves in `d1/main/state.c:2036`
- `d1/main/switch.c:131` does not require `TRIGGER_ON` before executing an
  action. Stock flags and the checked-in checkpoint bytes confirm that this
  matters in ordinary gameplay
- `d2/main/d1_save_translate.c:1139` sets `TF_DISABLED` whenever the saved
  D1 flags lack `TRIGGER_ON`
- `d2/main/switch.c:505` immediately returns for a disabled trigger
- The fresh D1 level loader in `d2/main/gamesave.c:949` strips `TRIGGER_ON`
  without disabling the trigger, so fresh level and checkpoint starts differ

Effect: translated checkpoint door switches, matcens, and exits stop
executing. The current four fixtures contain 19 such affected triggers.
This is independent of their historical replay pass/fail results.

Plan: make fresh-load and checkpoint trigger conversion share the same D1
semantics. Do not infer inactivity from this legacy bit. Preserve native D1
execution behavior and add direct assertions for a working exit, matcen,
and door trigger after translation, including triggers that have already
been crossed. Use a small synthetic case for one-shot state; see A4.

## A2. Secret levels need a complete D1 lifecycle

The current compatibility branches cover only part of a transition.

| Path | Native D1 | Current D1-in-D2 difference |
| --- | --- | --- |
| Secret entrance | `PlayerFinishedLevel(1)`, hostage credit, end-level scoring, then the mapped secret level | `TT_SECRET_EXIT` takes the D2 teleporter path; D2 demo-data and multiplayer bans and destroyed-secret checks run first |
| Entry accounting | Finishing the source level credits its carried hostages | `EnterSecretLevel` bypasses `PlayerFinishedLevel`; it shows score glitz only if the reactor is destroyed, then `StartNewLevel` clears carried hostages |
| Death before secret reactor destruction | Lose a life and restart within the same secret level | D2 restores `SECRETB` or advances from `Entered_from_level` |
| Death after secret reactor destruction | Advance using D1's level rules | D2 again follows its base-save return/advance path |
| Manual save/load while in a secret level | Allowed by native D1 | D2's generic negative-level guards reject ordinary save and in-game restore |
| Exit after a direct secret start or cold checkpoint restore | Derive next normal level from `Secret_level_table[-level-1] + 1` | D1 branch uses `Entered_from_level + 1`, a transient variable only assigned on teleporter entry |

Source anchors:

- `d1/main/switch.c:148`, `d1/main/gameseq.c:1034` (`PlayerFinishedLevel`),
  `d1/main/gameseq.c:1178` (secret return mapping), and `DoPlayerDead`
- `d2/main/switch.c:546` (`TT_SECRET_EXIT`),
  `d2/main/gameseq.c:1502` (`EnterSecretLevel`),
  `d2/main/gameseq.c:1553` (`PlayerFinishedLevel`),
  `d2/main/gameseq.c:1711` (secret return), and
  `d2/main/gameseq.c:1795` / `1819` (secret death branches)
- `d2/main/state.c:2219` and `2711` (save/restore guards)
- `d2/main/gameseq.c:479` (new-level hostage reset)

`Entered_from_level` has no serialization or checkpoint restoration in the
inspected code. A direct start can therefore choose level 1 or a stale
mission's return destination. Manual D1-in-D2 secret saves are currently
blocked, so that particular cold-load route becomes testable after the save
guard is fixed; direct starts and translated D1 checkpoints already expose
the missing state dependency.

Multiplayer has a second defect: `d2/main/net_udp.c:1944` sets the secret
flag merely because the current D1 level has a secret exit. Meanwhile
`AdvanceLevel` asserts the incoming flag is false and does not use the flag
returned by end-level synchronization to select a secret level. Removing
the teleporter ban alone would leave both discovery and routing incorrect.

Plan: select D1 transition policy before entering D2 teleporter processing.
Use the mission's secret table for D1 return routing, and carry the actual
selected exit through end-level synchronization. Apply D1 life, inventory,
hostage, bonus, and key-reset rules together. Scope save/restore guard
changes to D1 gameplay. Verify normal exits and endgame as controls.

Validate all three stock secret routes: `10 -> -1 -> 11`,
`21 -> -2 -> 22`, and `24 -> -3 -> 25`; include live/destroyed source
reactors, death before/after secret reactor destruction, direct start,
save/load in a fresh process, and co-op entry/exit. Check inventory and
hostage totals as well as the destination level. Include an existing D2
secret-level return/revisit test to protect D2 behavior.

## A3. AI behavior identity and door permissions remain incomplete

The same serialized values mean different things:

| Value | D1 | D2 |
| --- | --- | --- |
| Behavior `0x82` | HIDE | BEHIND |
| Behavior `0x84` | FOLLOW_PATH | SNIPE |
| Local mode `5` | HIDE | BEHIND |

Definitions are in `d1/main/aistruct.h:49` and `d2/main/aistruct.h:49`.
The level reader (`d2/main/gamesave.c:409`) and checkpoint translator
(`d2/main/d1_save_translate.c:575` and `881`) retain the numbers.
`d2/main/ai2.c:101` maps them using D2's initial modes. The unguarded
SNIPE block at `d2/main/ai.c:947` then runs sniper behavior in single player
or rewrites the robot to NORMAL/CHASE in multiplayer. D2's mode-5 branch
at `ai.c:1375` maneuvers behind the player; native D1's HIDE handling
at `d1/main/ai.c:2678` follows the hide path.

Existing D1 guards in firing and path code recognize some of these aliases,
but do not cover initialization and this top-level dispatch. A blanket
numeric remap to D2's new FOLLOW behavior is insufficient. Preserve the
upstream serialized IDs and interpret them using the selected gameplay
semantics throughout initialization, pathing, per-frame AI, and restore.

Two additional source differences belong in the same focused AI pass:

- `d2/main/ai2.c:174` forces every `attack_type` robot to NORMAL at
  initialization; native D1's `init_ai_object` retains the supplied
  behavior. Test melee robots given STILL/STATION behavior
- `d2/main/ai2.c:1803` allows brain, RUN_FROM, and SNIPE robots to plan
  through doors for which the player has a key. Native D1's
  `ai_door_is_openable` at `d1/main/ai.c:1461` allows only brain/RUN_FROM
  robots and only unkeyed, unlocked doors. This is a confirmed path
  eligibility difference; actual door-opening collisions need a separate
  runtime assertion

Use a compact custom mission containing each behavior, keyed/unkeyed and
locked doors, a brain, and a melee robot. Compare mode, goal segment, path,
shot timing, position, and RNG calls from fresh and checkpoint starts.
Preserve the intentionally added guidebot's own path permissions.

## A4. D1 trigger actions are reduced to one D2 type

Native D1's `check_trigger_sub` (`d1/main/switch.c:139`) uses independent
flag tests with a defined order and an early return for secret exits.
It can apply shield damage, energy drain, door control, matcen activation,
and illusion changes on the same trigger.

`d2/main/gamesave.c:952` instead uses an `else if` chain to choose one D2
type; shield damage and energy drain only reach `Int3()`. The checkpoint
converter (`d2/main/d1_save_translate.c:1099`) also chooses one type, but
with different precedence: EXIT beats DOORS there, while DOORS beats EXIT
in the level loader. Damage/drain-only checkpoint triggers fall back to
OPEN_DOOR. Therefore fresh and restored composite triggers can disagree
even after A1 is fixed.

One-shot handling also needs an explicit native-D1 comparison. Native D1
clears `TRIGGER_ON` on the crossed trigger and its opposite-wall trigger
after activation (`d1/main/switch.c:220`), but the runtime does not check
that bit before acting. D2 disables a one-shot before its action and does
not reproduce that paired update. Do not silently treat the flag's name
as stronger evidence than native execution. If the native behavior itself
is to change, document and test that shared change separately from parity.

Plan: preserve D1 action flags in a representation available to fresh
levels and translated checkpoints, and dispatch actions in native order.
Keep D1 data interpretation in engine code. Test combined DOOR+MATCEN,
DOOR+EXIT, damage/drain values, repeat crossings, opposite-side triggers,
one-shot flags, illusion toggles, co-op synchronization, and save/load.
These require custom fixtures; the stock HOG does not exercise them.

## A5. Checkpoint runtime state is only partly restored

`d2/main/d1_save_translate.c:1492` reads selected runtime state but skips
other sections that native D1 saves and restores:

| State | Current translation | Consequence to validate |
| --- | --- | --- |
| Weapon creation frame and complete hit-object list | `skip_weapon_fidelity_state` at 1426; object reader at 549 sets creation frame to zero and retains only `last_hitobj` in the hit list | Persistent weapons can damage previously hit objects again; homing frame scheduling can differ |
| Active morph slots | `skip_morph_state` at 1477; commit calls `init_morphs()` | `d2/main/morph.c:214` marks a morphing object for deletion when its morph data is missing |
| Stuck-object registry | Skipped at 1551 | A saved flare's relationship to its door is not restored |
| Active effect timing and dynamic state | Records read/skipped at 1565 onward without applying them | Animated or one-shot effect continuation is not faithful |
| Generated secret-area runtime data | Skipped at 1578 onward | Repository-specific discovery state also needs a restoration decision |

Native reference: `d1/main/state.c:361` / `375` preserve the complete
weapon fields, followed by morph and other runtime sections. The consumers
include persistent-hit checks in `d2/main/collide.c:1805` and homing age
calculation in `d2/main/laser.c:1503`.

The translator already restores world/AI records, pending fire counts,
Fusion charge, Spreadfire toggle, missile gun, RNG state, object allocator,
and path runtime state. Preserve that work; complete the missing sections
using validated field translation and object/signature references.
Separate required gameplay state from optional presentation state explicitly.

Tests need a persistent projectile that has hit two live targets, a homing
missile saved at a tracking boundary, an active matcen morph, a flare in a
door that subsequently opens, and an active one-shot effect. Compare
immediately restored state and the first subsequent frames, not only a
long replay's final player state. Connect the weapon cases to the weapon
plan's checkpoint validation.

## A6. Translated replay startup lacks agreement checks

`android/app/src/main/cpp/shared/input_demo_start_shared.c:605` chooses the
D1 checkpoint's mission, level, and difficulty, loads the mission, starts
the game, applies translated state, and returns. It does not perform the
normal branch's mission/level/difficulty agreement check at line 673.
The earlier context preparation checks metadata presence, not agreement.
There is also no explicit post-load assertion in this branch that the
resolved mission has `descent_version == 1` and active D1 gameplay tables.

Checkpoint parsing and difficulty range validation already exist. Payload
SHA-256 validation in `input_demo_replay.cpp` protects the saved bytes;
it does not establish agreement with the replay header or installed assets.

Plan: normalize built-in D1 mission aliases, compare checkpoint mission,
level, and difficulty with the replay header before starting, then verify
the loaded mission and effective gameplay mode. Reject mismatches with a
specific reason. Test each mismatch separately and include the valid
built-in alias case. Treat start-from-level diagnostic replay as a different
start condition, not a replacement for checkpoint fidelity.

## A7. Persistence and multiplayer need effective-data identity

Current identity has useful parts but does not establish D1 semantics:

- D2 save restore reads the mission and loads it before the level
  (`d2/main/state.c:2997` onward). Android save metadata carries engine
  game ID, mission, and level (`android_save_meta.h:64` and `94`), but no
  D1 gameplay revision or imported-table fingerprint. A D2 engine ID is
  correct for this executable; it is not a D1/D2 gameplay-mode assertion
- Input-demo metadata records game, mission, build/git version, settings,
  and start condition (`input_demo_fixture.h:104` onward), but no effective
  D1 data identity. `Get-D1InD2GameConfig` in
  `android/tests/run_input_demo_replay.ps1:398` explicitly empties required
  hashes; directory acceptance checks required filenames. Other test data
  helpers have hash-index discovery, which is not equivalent to validating
  the active D1/D2 overlay for each replay
- UDP verifies program/protocol identity and mission information, then
  compares `segments_checksum` at `d2/main/net_udp.c:5524`.
  `d2/main/gameseq.c:771` computes that checksum from segment fields;
  weapon/robot/powerup tables are not included. Inspected co-op save
  checksums protect saved payloads, not gameplay-asset agreement

Plan: expose one engine-produced identity for selected gameplay family,
compatibility revision, mission content, and the effective imported tables
and relevant asset mapping. Use it in replay diagnostics and validation,
Android save metadata, and network join/restore agreement. Include D1 and
D2 base assets where the overlay still depends on both. Preserve upstream
save layouts; follow the repository's replaceable-format policy for Android
metadata. Version network changes explicitly and reject mismatched joins.

Test matching data, a changed D1 PIG with unchanged level geometry, a changed
D2 asset mapping, wrong mission family with a colliding name, and a
D1 -> D2 -> D1 mission switch. A network match between two D2 executables
running D1 missions is the immediate target; native D1/D2 executable wire
interoperability requires a separate protocol project.

## A8. Classic D1 demos remain a separate feature

`d2/main/newdemo.c:1769` rejects game types below D2's type 3. Native D1
uses registered type 2 and shareware type 1 (`d1/main/newdemo.c:162`).
The input-demo checkpoint translator does not change that reader.

Plan a D1 classic-demo format adapter after the gameplay and asset work:
parse D1 versions/events/object layouts, resolve mission and assets, and
verify playback state and rendering against native D1. Do not just remove
the game-type check. Classic event playback and deterministic input replay
are separate acceptance suites; success in one does not establish the other.

## Wall edges and existing fixes

The inspected code already has D1 door-close behavior at
`d2/main/wall.c:952` and D1 auto-close waiting behavior at line 1407.
Both games use the same special multiplayer boss-door coordinates
(level 7, segment 595, side 5) in `special_boss_opening_allowed`; this is
not another missing conversion. Existing D1 collision, visibility, aiming,
AI time slicing, path randomization, and nearby-fire guards also remain
relevant. Imported robot records are zero-initialized and disable the D2
second weapon (`d2/main/d1_in_d2.c:1329`); extra D2 robot fields are not
simply left uninitialized.

One source difference remains a targeted validation candidate:
`remove_obsolete_stuck_objects` in D2 (`wall.c:1477`) shortens the referenced
object's life when a door is no longer closed or its signature differs.
Native D1 (`wall.c:1001`) removes an entry for wall zero or a signature
mismatch without that life change. Ordinary door opening also calls
`kill_stuck_objects`, so reachability and object-slot reuse must be tested
before attributing an observed flare bug to this cleanup difference.

Keep paired doors, reversing/obstructed doors, illusion walls, reactor-linked
doors, transparent-wall collision, and stuck-flare save/load in the focused
wall regression matrix. Source inspection here does not certify every wall
and physics path as equivalent.

## Implementation order and acceptance

1. Complete the weapon plan's Spreadfire ID/art, Quad Laser, and robot Smart
   Missile work. Add A1 trigger restoration and A5 weapon-state restoration
   before interpreting checkpoint replay results as parity evidence
2. Complete A5's other gameplay state and A6 startup validation, then run
   focused fresh-start versus restored-state comparisons
3. Fix A2 as one coherent lifecycle change, including co-op exit selection,
   ordinary secret saves, hostage credit, and mission-table return routing
4. Complete AI dispatch/door work in A3 and the already identified D1 boss
   handlers. Boss flags 1 and 2 still reach the unimplemented cases at
   `d2/main/ai.c:772`; standard robot-path fixes do not replace boss cloaking,
   teleporting, or super-boss gating behavior
5. Complete A4 custom trigger semantics and the existing reactor, scoring,
   and powerup-table work. Reactor difficulty timers and firing patterns
   (`cntrlcen.c`), end-level bonus formulas (`gameseq.c`), and activation of
   the parsed D1 powerup table (`d1_in_d2.c`) remain separate confirmed gaps
6. Add A7 identity checks across restore/replay/join and protect D2 mode
   switching. Establish the expanded fixture matrix before claiming general
   D1-in-D2 input-demo compatibility
7. Implement A8 classic-demo support and retain native D1 as the comparison
   engine until gameplay, saves, assets, demos, and supported co-op routes
   have explicit acceptance coverage

Put reusable compatibility policy in the existing D1-in-D2/shared support
layers, with small engine hooks where game-format interpretation belongs.
Avoid broad rewrites of the two upstream engines. Reconcile concurrent
`aipath.c` work before making path changes.

When implementation is authorized and build/emulator use is available,
run the relevant host build and focused integration tests, then serial
Android tests and rendered weapon checks. Use existing replay state and RNG
traces to find the first divergence. Fix engine behavior before recording
replacement demos; do not add replay-specific compensation. Expand tests
for the uncovered cases above rather than repeatedly running only the four
current D1 fixtures.

## Completion checklist

- [x] Audit robot AI initialization, dispatch, path permissions, and existing gates
- [x] Audit normal/secret transition entry, death, accounting, and return routing
- [x] Audit wall edges and fresh/checkpoint trigger conversion and execution
- [x] Audit save guards, checkpoint runtime restoration, and provenance
- [x] Audit input/classic demo boundaries and startup validation
- [x] Audit multiplayer mission/data agreement and secret transition handling
- [x] Inspect all registered stock RDL behavior/trigger records without running the game
- [x] Verify and inspect all four checked-in D1 checkpoint payloads
- [x] Rank findings and specify implementation/validation work
- [ ] Implement the findings in separate scoped changes
- [ ] Complete host and Android runtime validation after the current restriction is lifted
