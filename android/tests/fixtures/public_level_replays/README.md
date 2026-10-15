# Compact public level-start recordings

The corpus preserves production-recorder captures with pinned Descent 1 v1.4
shareware and Descent 2 v1.0 demo data. Recordings start from ordinary authored
level spawns; each scenario declares its level in the manifest. No player
pose, inventory, object or geometry is staged; no save/checkpoint data is stored
in the replay or its companions. All seventeen scenes were freshly recorded twice from
source `77bd4d8e4b5636499730f7bc3f7857e6920c2911`. Their native
recordings, compact inputs, RNG traces, summary reports and observation oracles
were byte-identical across the independent captures. These are direct format-5
captures; obsolete conversion sidecars have been removed.

## Size and evidence contract

Each replay is plain uncompressed UTF-8 JSONL. Admission rejects any individual
replay or oracle file larger than **100000 bytes**. There is no compression, base64, split recording or hidden save.

The engine first records complete live frame state/diagnostics and actual RNG
events into temporary capture output. The packer retains ordinary controls,
per-frame SIM RNG state and the exact terminal result, replacing bulky state
observations with a SHA-256 oracle for every complete frame. A separate digest
covers every normalized SIM RNG event, including values, order and actor context.
Expected hashes are never generated from replay output.

Only explicitly reviewed diagnostic event kinds are removed. Format 5 executes only its ordered dispatch operations. Public admission allows
reviewed ordinary player commands and rejects debug spawning, warps and unknown
commands, including operations in the terminal input tail.
Removed telemetry has a provenance hash/count and is explicitly not claimed as
replay-compared. Unknown event kinds fail admission rather than being ignored.

Each oracle pins the engine source revision, capture executable, capture fixture
source and all extracted public asset hashes. `demo_import_oracles.json` and
`game_data/test_data_manifest.json` identify the source distributions. Every oracle points to the same identified source commit, built before
fixture admission; `capture_source_sha256` identifies the exact capture fixture
header. Ordered-input packing and admission require the recorder's compiled Git
identity to match that source revision. Reconfigure CMake after selecting the capture commit
so stale generated build metadata cannot be admitted. Format 5 restores SIM RNG once before the first input batch, then checks the
carried pre-input and computed post-input boundaries; it does not seed the separate FX stream. Capture leaves ordinary FX
initialization untouched, including the level-entry starfield draws.

Every fresh recording embeds complete `asset_fingerprints` for the actual loaded
HOG/PIG/HAM files. Strict verification matches these against playback data before
execution. No FX RNG baseline is recorded or restored. This source preserves the
historical algorithms while assigning gameplay-affecting RNG draws to SIM.
`asset-regeneration-validation.json` records independent capture equality,
unchanged frame/SIM/terminal/action oracles, 34 strict native headless playback
passes, and the separately verified imported flight scope. All 17 recordings
qualify for verified-shareware selection (8 D1, 9 D2); the largest is 93139 bytes.
Headed qualification covers native D1 flight and its imported D1 counterpart,
using the production executable with rendering disabled; other cases were
qualified through the headless engine. The native D1 recording remains a native
D1 capture during imported playback; D2 runtime resources are verified separately.
Full imported object/world parity remains **INCOMPLETE**, with failures retained
in the validation report. PowerShell wrapper execution was unavailable during
regeneration; direct native runs used the shared Python asset, fidelity and
real-action validators.

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

- `loot` (D1 only): follow ordinary level-one connected passages to authored robot
  39 / ID 20 / signature 40, kill it with starting lasers and collect its explicit
  laser upgrade. The original robot carries one powerup ID 3, so this scene
  exercises the authored-content path without chance/count rolls. It uses the
  existing initial SIM seed 1234; route experiments are recorded in the plan

  Admission ties every shield reduction and the 500-point award to real
  player-owned lasers, checks the original robot's death and retirement, and
  identifies the exact newly created powerup by slot and signature. The pickup
  must remain live until inventory gain, acquire the native consumed flag at
  that boundary and retire on the following frame. Other laser pickups cannot
  explain the gain. The living player advances laser level 1 to 2 while retaining
  primary 0, with no pending autoselection. This is an upgrade to the selected
  starting weapon, not new-primary autoselection coverage

  No-fire preserves the route but removes laser firing and must leave the
  original carrier alive without its drop. No-collect preserves every input
  through the physical drop, then releases flight/fire controls: the same drop
  must remain live, score must remain 500 and the living player must retain the
  original laser level. Both controls finish all replay frames, fail strict
  comparison and fail the causal gate through the real native wrappers

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

- `guidebot-release` (D2 only): ordinary thrust and aiming reach the original
  cage from the level-one spawn. Starting lasers damage the authored barrier at
  frame 101; walls 74 and 75 blast at frame 177. The same original Guide-Bot
  (object 28, ID 33, signature 29) physically exits segment 224 at frame 179,
  then its native talk/release flag changes at frame 188. These are distinct
  boundaries, not interchangeable proxies. The 219-frame scene observes another
  second of ordinary movement while both player and Guide-Bot remain alive

  Each live post-frame escort observation is hashed independently from capture
  into `guidebot_observations`; headed and true headless playback must reproduce
  every one. The semantic gate also requires intact authored cage walls, real
  pre-blast damage, both wall sides blasting, the exact release flag and the
  original bot's first physical cage exit. A no-fire negative keeps all recorded
  route/aim controls and must fail those actual outcomes

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
hides these observations. The manifest's versioned `imported_d1` inventory explicitly
declares every D1 case as required gameplay, diagnostic-only or excluded, with a
reason. New cases without a declaration fail admission, and required flight cannot
be excluded. The level-three door-lifecycle case remains native-only: its D2 import
sets `wall.controlling_trigger` to 5, violating the unchanged D2-only default-state
gate. [PR23's first headed run](https://github.com/larsonm-personal/dxx-redux/actions/runs/38316349764)
retains that failing diagnostic in artifact `11778690791`; all 22 native runs in
that attempt passed. Subsequent reports list the explicit import exclusion and
reason rather than silently broadening or weakening the prior imported scope.
Native console and headed fidelity is qualified only
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

## Public objective-arrival fixture

`test_guidebot_live_navigation.ps1 -NoBuild -PublicDemo -BuildDir <D2-build>
-HogDir <pinned-public-D2-data>` reuses the existing true-headless navigation
fixture with the real demo level-one Guide-Bot and reactor. This is explicitly a
controlled arrival test: it places the player and bot in adjacent segment 158,
grants keys and 1000 player shields, and removes other robots. The ordinary NEXT
command chooses the reactor; native escort navigation and physics must cross
into reactor segment 159. The fixture checks every approach frame and all 720
subsequent patrol frames, requiring the player to survive and the reactor
objective to remain pending while the live reactor still needs player action. It is not a normal-start route recording. The default retail level-11
grate-detour and Maximum level-16 hostage fixtures remain unchanged

The Guide-Bot capture also supports `DXX_PUBLIC_CAPTURE_SCREENSHOTS=1` and
`--scenario guidebot-release`. Its screenshot inventory includes first damage,
both-wall blast, physical exit, talk/release, and a later clear view. Screenshot
captures remain independent from the clean oracle-admission captures

## Ordered input and migration

Format 5 records actual ordered pre-simulation reset, clear, sample, controls-only,
rear-view, automap and direct-command operations, each with input-time `ft`. A mandatory `input_tail` retains
input after the final simulation. `dispatch.pre_rng="="` references that row's
post-input RNG; `sample.s="="` references that row's decoded residual held state;
missing sample pulses are zero. Missing dispatch is invalid; empty ops is explicit.
Packing validates exact decoded semantics before and after these lossless aliases.
The FX stream remains outside the qualification contract.

Legacy public inputs are converted with at most three documented whole-recording
schedules in `convert_public_replay.py`. Every candidate is checked against the
unchanged original full frame, diagnostic, SIM-event and terminal oracles. Multiple
passing schedules show equivalent behavior, not recovered historical event order.
Migration sidecars bind original and converted replay hashes, unchanged oracle
hashes, and full verified baseline evidence. The converter never edits expected
files. Native runtime rejects old input versions with conversion/re-record guidance.

The secondary-release-first and secondary-mouse-first scenes use actual queued SDL
input from normal level-one starts. They differ only in release/mouse order at
frame 13, following a second press eight ticks before the cooldown. Expected concussion ammo is
four and three respectively in both games. No cooldown or gameplay timing changes
are made to obtain replay parity.

The four 25-frame secondary scenes were independently captured twice from clean
source `eba0eee69077fd53e4aee54d04f026f8d7ac9e6c`, retained on
`capture-public-secondary-ordered-v5`. Repeated recordings and complete oracle
files are byte-identical. The capture executable and recipe hashes are in each
oracle; the recorded build identity must match the pinned source revision.
D1 release-first/mouse-first recordings are 5680/5686 bytes, with 3203-byte
oracles. D2 recordings are 5943/5969 bytes, with 3906/3907-byte oracles. These
start at ordinary level spawns and contain no save payload. Optional Xvfb-rendered
proof screenshots also leave both games' mouse-first recording/oracle bytes
unchanged. These four native scenes are distinct from the 13 converted recordings;
all original oracle bytes remained unchanged at that format-only checkpoint.

Native modal integration coverage is separate from these small admitted scenes.
It preserves live rear-view semantic decisions and automap lifecycle/depth effects,
including a terminal tail with an open virtual map and signature-checked D2 marker
deletion. This recording-format layer does not choose the combat view. Rewind and debug map-world
cheats explicitly stop input capture; normal rear-view/map use does not.

The native modal runner compares complete visited-segment and marker observations
in addition to the unchanged gameplay/RNG oracles. Its isolated recording-off
checks cover rear baseline restoration and absolute-slot multiplayer marker
semantics; these synthetic checks are explicitly excluded from public admission.
The actual SDL captures include1,100 map redraws without input-operation growth,
F3 and an active-map terminal tail. Default D2 missile-window discovery
uses the same native selector and canvas geometry as the renderer; isolated
geometry tests require main-only discovery to miss an offscreen region while the
combined CPU pass matches the actual renderer exactly. Guided/other selected
auxiliary cameras and endlevel special-camera map fidelity are not claimed.

## Fixed-forward gameplay recaptures

D1 door/combat and D2 combat/Guide-Bot were independently recaptured from clean
published source `9be53d6d74feede5b3df4cf7dda19fc10b5f9fc2`, retained on
`capture-fixed-forward-gameplay-view`. The historical cockpit projection restores
scaled robot warnings and makes combat visibility independent of the displayed
camera. These four live captures replace their recording/oracle pairs; the other 13
pairs, including all four secondary scenes, remain byte-identical.

The approved causal changes preserve exact scenario objectives. D1 combat's
warning-driven evasion moves death/loot/retirement to 116/123/138 and the second
identified incoming hit to 116. Guide-Bot release/physical exit move to 188/179.
Capture lengths are 426, 139, 263 and 219 frames respectively. D1 door and D2 combat
change warning state only; Guide-Bot has a genuine downstream SIM RNG change.
Historical migration sidecars remain as conversion evidence; the four new oracle
pairs instead bind their fresh native source, executable, recipe and input hashes.

Both native backends pass the original-oracle and action checks using the same
unchanged-input executions that also reject deliberately damaged oracles. These
are reused evidence, not extra independent runs. Altered-input and both partial
and completed no-collection controls retain their strict causal gates. See the
fixed-forward gameplay audit for the complete 142-execution accounting and the
unchanged imported-D1 qualification limits.
