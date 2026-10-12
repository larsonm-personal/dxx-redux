# Compact public level-start recordings

Eleven fresh production-recorder captures use pinned Descent 1 v1.4 shareware and
Descent 2 v1.0 demo data. Nine start from ordinary level-one spawns; the D2 shoot-switch case starts from the
ordinary level-two spawn, and D1 door-lifecycle starts from its ordinary level-three spawn. No player
pose, inventory, object or geometry is staged; no save/checkpoint data is stored
in the replay or its companions. Existing retail recordings remain unchanged.

## Size and evidence contract

Each replay is plain uncompressed UTF-8 JSONL. Admission rejects any replay and
oracle pair totaling more than **100000 bytes**, a stricter limit than 100 KB per
replay alone. There is no compression, base64, split recording or hidden save.

The engine first records complete live frame state/diagnostics and actual RNG
events into temporary capture output. The packer retains ordinary controls,
per-frame SIM RNG state and the exact terminal result, replacing bulky state
observations with a SHA-256 oracle for every complete frame. A separate digest
covers every normalized SIM RNG event, including values, order and actor context.
Expected hashes are never generated from replay output.

Only explicitly reviewed diagnostic event kinds are removed. The engine executes
only `direct_command` events; these recordings forbid those commands entirely.
Removed telemetry has a provenance hash/count and is explicitly not claimed as
replay-compared. Unknown event kinds fail admission rather than being ignored.

Each oracle pins the engine source revision, capture executable, capture fixture
source and all extracted public asset hashes. `demo_import_oracles.json` and
`game_data/test_data_manifest.json` identify the source distributions. The source
revision denotes the engine base for the original seven scenes. Combat and door-lifecycle oracles
point to an exact published source-only commit built before fixture admission;
`capture_source_sha256` also identifies the exact capture fixture header. Combat and door-lifecycle
packing and admission require the recorder's compiled Git identity to match that
published source revision. Reconfigure CMake after selecting the capture commit
so stale generated build metadata cannot be admitted. The protocol restores SIM RNG per
frame; it does not seed the separate FX stream. Capture leaves ordinary FX
initialization untouched, including the level-entry starfield draws.

## Real actions

- `flight`: retreat from the normal spawn while firing into the level. Solid
  geometry stops continued backward thrust; the runner requires at least ten
  live-player contact frames, movement, actual projectiles and energy expenditure
- `pickup`: a deterministic controller follows connected public segments to a
  real existing pickup, using only ordinary thrust. D1 collects energy; D2 collects
  shields. The runner requires the gain at the exact recorded `pickup_gain_frame`
  boundary, separately from the subsequent object-slot retirement `action_frame`
- `door`: ordinary thrust follows a reachable path to an existing unlocked door,
  with firing. The selected door must start closed and actually enter opening or
  waiting state at the recorded boundary

- `shoot-switch` (D2 only): start at level-two spawn segment 217, fly to the real
  switch at segment 84 / side 2 / wall 154, and shoot trigger 17. The authored
  forward door 116 (segment 85 / side 4) starts locked and closed; reverse wall
  117 (segment 235 / side 5) starts unlocked but closed. Admission requires the
  exact successful player-projectile activation, the switch overlay change and
  forward lock removal at the same boundary, both door sides opening, and the
  living player crossing from segment 85 into 235. It rejects a mere firing
  sequence, an already-unlocked passage, duplicate switch activation or a route
  that bypassed the passage. The current scene activates at frame 175, starts both
  sides opening at boundary 187, and crosses at frame 217 (218 total frames)

- `combat`: ordinary thrust and yaw/pitch controls approach an existing robot,
  keep firing range and use the unmodified starting lasers. D1 targets original
  object 8 / robot ID 3 / signature 9, deals three 10-point hits to its 25 shields
  at frames 105, 115 and 125, and retires the original object at frame 147 (148
  total frames). D2 targets original object 11 / robot ID 37 / signature 12,
  deals two 9-point hits to its 12 shields at frames 239 and 241, and retires it at
  frame 262 (263 total frames). Each exact kill awards 200 points and increments
  the real level/total kill counters once while the player stays alive

  The scenes also prove the original loot rules finish after death. D1's loaded
  definition allows one energy pickup with a 3/16 chance; its actual SIM roll
  17896 selects bucket 8, so no count roll or physical drop occurs at frame 132.
  D2's selected robot has a zero-count loot definition, so neither a loot roll
  nor a physical drop occurs at frame 248. These are justified no-drop outcomes;
  neither scene claims a positive physical loot drop. The existing controlled
  loaded-asset combat suite retains its positive physical-drop coverage

  Read-only damage observations identify the original robot, real player-owned
  laser, shields before/after and exact award. Loot observations retain the loaded
  rule, actual decision rolls and resulting object identity. Every observation
  is also hashed from independent live capture into the same size-bounded oracle.
  Admission couples these events to complete raw object/world state, rejects
  collateral scores, ram-only kills, reused-slot confusion and phantom drops,
  and requires the original robot's terminal retirement. Recorded controls alone
  drive replay; the target-following capture controller is never run during replay

- `door-lifecycle`: use ordinary thrust to reach an authored automatic door, hold
  the player in its doorway beyond the original close delay, clear it, allow a
  complete close, then reopen by player contact and traverse. D1 uses level-three
  walls 72/73 (segments 75/38); its front wall enters closing while the back wall
  remains waiting, and obstruction freezes the active-door timer at zero. D2
  uses level-one walls 76/77 (segments 6/4); both faces stay waiting while the
  player obstructs the doorway. D2 additionally clears and re-enters the still
  passable closing door to prove its automatic reopening behavior before the
  final full close. The D1 player uses authentic starting lasers against the
  real nearby guard and survives; D2 collects an incidental authored shield
  pickup. Neither scene edits the world, disables robots or changes difficulty

  Separate read-only door probes use the engine's actual sphere/side masks and
  front/back collision queries. They identify every blocker by original slot,
  signature, pose and radius, preserve the native D1/D2 weapon/fireball exclusion
  difference, and are hashed from live capture. The semantic gate ties them to
  full object/world state, requires twenty obstructed frames beyond the wait,
  freezes D1 closing or prolongs D2 waiting as authored, checks both collision
  faces, requires clearance and active-door retirement after full close, then
  verifies ordinary reopening and traversal. Removed controls must complete
  playback and fail this lifecycle gate, not merely fail recording admission

The D2 capture startup initializes PIG bitmap flags before loading the level, just
like the desktop game. Those transparency flags affect ambient propagation; the
level-two scene detects an omitted startup step through its lava ambience and
strict AI diagnostic hashes. No world values are staged to make capture match

The route controller is used only to create input recordings. Replays consume the
recorded controls without navigation assistance. Captures end shortly after their
selected action succeeds; combat ends at the original robot retirement boundary. Fixed timesteps are 1/60 second for `flight` and 1/30
second for route scenes, rounded down to engine fixed-point units.

## Reproduce

Prepare public data with `android/helpers/setup_shareware_test_data.ps1 -Game both
-Download`. Build the Debug `dxx-redux-d1-headless` and
`dxx-redux-d2-headless` targets for the primary display-free pass. Build
`dxx-redux-d1` / `dxx-redux-d2` for the headed pass, and `test_upstream_compat`
for route capture. Both replay backends use these same immutable oracles:

```powershell
./android/tests/test_input_demo_regressions.ps1 -Corpus public -D1Exe ./buildd1-replay/main/dxx-redux-d1-headless -D2Exe ./buildd2-replay/main/dxx-redux-d2-headless
# Run this headed path under Xvfb/software-rendered X11 on a cloud machine
./android/tests/test_input_demo_regressions_graphics.ps1 -Corpus public -D1Exe ./buildd1-replay/main/d1x-redux -D2Exe ./buildd2-replay/main/d2x-redux
```

To create candidates in a new temporary directory, use
`capture_compact_public_replays.py --repo . --output temp/new-capture
--capture-d1 buildd1-replay/main/test_upstream_compat
--capture-d2 buildd2-replay/main/test_upstream_compat
--data-d1 game_data/shareware/d1 --data-d2 game_data/shareware/d2
--engine-revision <full-engine-commit>` under the same display. Capture does not
update this corpus. Independently repeat capture and verify clean native playback
before admitting any new expectation.

A read-only D2 trace record reports successful trigger activation with the exact
frame, source wall, trigger, shot flag and player identity. It is emitted after
the normal trigger handler succeeds; replay never executes this observation.
The semantic gate couples it to the raw before/after walls, switch texture and
player movement. Its synthetic negative tests run as `test_public_switch_replay.py`

Native playback must match every live frame hash, terminal field and SIM event
stream twice, plus the action-specific assertions. The existing paired D1 runner
retains its full detailed report and explicit full-simulation INCOMPLETE status.
Its recording-to-native frame/RNG checks are incomplete because compact files do
not carry expanded observations; the independent immutable hash checks provide
that evidence. Native repeatability retains every existing strict check. The separate imported
D1 `flight` qualification compares exactly `f`, `ft`, gameplay `state`, SIM `rng`, the
terminal result and every actual SIM RNG event. It does not qualify diagnostics,
object state or world state: the original report retains those failures and the
full-simulation **INCOMPLETE** status. Native D1 starts the player at signature 1,
while D2 explicitly initializes its player signature to 0; derived identity
hashes differ. Initial effect-animation phase also differs and remains
unqualified. The longer door trace also has two otherwise-identical SIM events whose player
context signatures differ. Door, pickup and combat are native-only required coverage;
their imported runs are diagnostic probes and cannot satisfy the paired gate.
Their original and scoped comparison failures remain visible. No normalization
hides these observations. Native console and headed fidelity is qualified only
for the admitted public scenes. No full-game, Android or imported headless-console
fidelity claim is made.

## Combat route screenshots

For route inspection only, set `DXX_PUBLIC_CAPTURE_SCREENSHOTS=1` while invoking
`test_upstream_compat --public-level-replay <public-data-directory> combat` under
Xvfb. This invokes the actual desktop renderer every frame and saves PNG images
at regular intervals, first damage and death; `capture.json` lists their frame
numbers. Inspect these rendered images, and use separate fresh non-screenshot
captures for oracle admission. Screenshot mode does not inject a player pose,
change inventory or alter robot definitions. Candidate capture can be restricted
with `capture_compact_public_replays.py --scenario combat`
