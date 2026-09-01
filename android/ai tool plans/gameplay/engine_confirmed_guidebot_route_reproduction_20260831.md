# Engine-confirmed GuideBot route reproduction

## Goal

Determine whether mission route results can be confirmed by running the real game
simulation with the GuideBot as a proxy player, then define an implementation
that uses those confirmations for headed debugging and deterministic headless
regression generation.

The confirmation must require real engine traversal for keys and fly-through
triggers. Shootable switches may use the shared route planner hittability proof,
but their effects must be applied through the normal engine trigger path. Door
and wall interactions must obey the simulated actor's current keys and the real
wall state.

## Required invariants

- The static planner proposes objectives but cannot certify physical traversal.
- A successful leg requires the engine actor to reach the objective using the
  normal GuideBot path, movement, collision, and wall constraints.
- Key acquisition requires physical contact with the key object. Reaching its
  segment is not sufficient.
- Fly-through trigger activation requires physical traversal of the trigger
  side. Reaching either adjacent segment is not sufficient.
- Switch activation uses the shared planner's hittability rules and the normal
  engine trigger mutation path.
- Door traversal and opening use current engine state and held keys.
- Headed and headless runs use the same simulation controller and result schema.
- Route regression status cannot be `ok` when engine confirmation fails.
- A relaxed, presumed, or closest-approach result remains visibly `partial`
  unless its presumption is explicitly permitted by this contract.
- Player and GuideBot dimensions come from the loaded mission and level data.
  The canonical confirmation radius is the larger of the two.
- The canonical routing sandbox contains no active ordinary robots. It retains
  only the GuideBot, key carriers, bosses, and reactors needed for progression.
- A key carrier is stationary and harmless, becomes armed when spawned or first
  released into the actor's reachable world, and is killed through the normal
  damage path after exactly three seconds of simulation time.
- Bosses are stationary and harmless. A boss or reactor is destroyed on the
  first fixed simulation frame where the actor has reached an approved firing
  point and the live engine confirms sight to the target.
- A reproduction is deterministic enough that two runs with identical inputs
  produce the same normalized objective and interaction trace.

## Work status

- [x] Establish the requested confirmation semantics and non-negotiable invariants
- [x] Survey reusable headless runtime, headed automation, and deterministic tick support
- [x] Trace GuideBot goal selection, path creation, movement, pickups, triggers, doors, and switches
- [x] Evaluate architectural options and identify the minimum engine hooks
- [x] Define the reproduction input, deterministic execution contract, and output schema
- [x] Fix one canonical RNG seed and exact simulation-time schedule for all levels
- [x] Define compact successful objective-timing metadata
- [x] Define planner-to-simulator feedback and regression status rules
- [x] Produce a staged implementation and validation plan
- [x] Implement the shared route-confirmation feature gate and initial engine controller
- [x] Implement headed automation and headless executable front ends
- [x] Fix canonical seed 1, 60 Hz rational timestep, Hotshot difficulty, and D-tick boundary
- [x] Record compact objective seconds plus detailed radius and RNG evidence
- [x] Confirm Counterstrike levels 1-3 twice with byte-identical headless results
- [x] Confirm Counterstrike level 1 twice through the maintained Android headed fixture
- [ ] Integrate confirmed results into mission regression generation
- [ ] Regenerate and audit the mission corpus

The implemented initial controller now covers direct keys, key carriers,
shootable switches, fly-through triggers, hidden and blastable walls, keyed and
automatic door frontiers, reactors, bosses, and exact exit-side dispatch.  The
canonical sandbox removes ordinary robots, freezes retained bosses and key
carriers, kills a selected carrier after three fixed simulation seconds, and
pauses the reactor countdown after proving reactor destruction.  Counterstrike
level 2 exposed and now covers repeated restorer objectives and extension of a
single semantic goal through temporarily opened physical frontiers.

The two Android headed level 1 runs were exact within the platform: seed `1`,
fixed Hz `60`, `3237` frames, and cumulative objective seconds `20.583328`,
`49.166656`, and `53.949997`.  Windows headless level 1 follows the same three
semantic objectives and confirms at frame `2980`; platform-specific engine
physics timing is therefore recorded, not treated as cross-platform RNG drift.

## Executive conclusion

This is feasible for Descent 2 and D1 missions running in the D2 engine. It is
not merely a new metadata check. It requires a small simulation product inside
the engine, plus two deliberately narrow adapters for actions that the stock
GuideBot object cannot perform today.

The static planner should remain the fast search mechanism. It proposes the
next semantic objective and its allowed interaction. A shared engine
confirmation controller then drives the actual GuideBot through normal AI path
creation, physics, collision, walls, and animations. The controller accepts a
leg only after it observes the required engine event and verifies the resulting
world state. It then asks the live planner for the next goal from that mutated
state. It must not replay a stale precomputed route blindly.

The same controller can run:

- Headed, through game automation or a debug command, with the camera following
  the GuideBot and the current objective visible.
- Headless, in a new full-engine executable with dummy video and audio, a fixed
  timestep, deterministic random state, and ordered JSON output.

The most important existing gaps are bounded:

1. A robot does not normally pick up a powerup. GuideBot to powerup collision is
   disabled and the robot-powerup collision handler does not grant inventory.
2. Segment-crossing trigger processing is normally performed only for the
   player object. The trigger code itself already accepts a companion robot, but
   normal GuideBot movement does not call it.

Both gaps can be addressed without replacing physics or granting progress from
metadata. The verifier observes an actual GuideBot collision or crossed side,
then invokes the existing authoritative player pickup or trigger behavior for
the designated verification owner.

Technical feasibility is high. The no-ordinary-robots contract removes most
combat nondeterminism, and the explicit carrier, boss, and reactor rules make
the remaining progression actors deterministic. Integration risk is still
medium to high because this will expose real geometric and AI movement failures
that the topological planner currently hides. That is the point of the
verifier, not a reason to weaken its result.

## Canonical routing sandbox contract

The first implementation verifies route geometry and progression, not general
combat. It starts the full engine and preserves the level's walls, triggers,
doors, objects, physics, animations, and progression state, but normalizes
combat actors before route execution.

### Ordinary robot policy

- Retain the existing GuideBot.
- Retain robot instances that directly contain a blue, gold, or red key.
- Retain bosses.
- Remove all other existing robot objects without running their death, score,
  drop, or explosion behavior.
- Disable matcen production of ordinary robots. If a dynamically created robot
  is a key carrier or boss, retain it under the policies below; otherwise remove
  it before its first active AI frame.
- Record the stable signatures and counts of removed, retained, and dynamically
  filtered robots in setup output.

The initial static planner still scans the unnormalized level for mission intent
and metadata. The engine confirmation route snapshot is taken after sandbox
normalization, so its hash describes the world the actor actually traverses.

### Key-carrier policy

A retained key carrier has attack, pathing, velocity, and ordinary AI disabled.
It remains a physical object at its authored or spawned position until its
timer expires. The carrier's `armed_at` time is the first of:

1. The frame in which a dynamic carrier is created.
2. Confirmation start, if an authored carrier is already in the actor's live
   reachable component after cage release and sandbox setup.
3. The first frame in which a door, wall, or trigger transition brings an
   authored carrier's segment into that live reachable component.

This reachability transition is the precise verifier meaning of "released". It
avoids mission-specific guesses about which trigger owns a carrier. The result
records the event that changed reachability and the carrier's stable signature.

At `armed_at + 3 * F1_0`, the controller applies lethal damage using the
verification owner as killer. It then runs ordinary engine frames until the
normal death sequence produces the contained key. A long death roll is allowed
within the world-settle budget. The controller must find the actual dropped key
and make the GuideBot physically collide with it. It may not grant the key from
the carrier record.

If a carrier never becomes reachable, never dies, or fails to drop the expected
key, the carrier objective fails. If it is initially reachable, the three-second
timer starts at confirmation start rather than waiting for the GuideBot to see
it.

### Boss and reactor policy

Boss AI, velocity, teleportation, cloaking, attacks, and gate or spew behavior
are disabled for the confirmation run. The authored boss object and collision
volume remain at their initial position. Reactors remain unchanged until their
firing condition is met.

A boss or reactor firing condition is true only when all of these are true in
the same simulation frame:

1. The live GuideBot goal identifies that exact object as the current semantic
   objective.
2. The actor has physically reached the planner-approved firing segment or
   candidate tolerance using normal GuideBot navigation.
3. A fresh engine FVI query from the actual actor firing origin to the target
   aim point is unobstructed or first hits the intended target object.
4. The candidate meets the canonical shared sight-quality policy.

On the first qualifying frame, the controller applies lethal damage through
`apply_damage_to_robot` for a boss or `apply_damage_to_controlcen` for a reactor,
using the verification player object as the authorized damage owner. It then
waits for and verifies the normal boss or reactor death state, triggered walls,
countdown state, and route-state change.

These are certifying presumptions because they are explicitly part of the
current contract. They prove that the actor can reach a valid firing point and
has live sight, but do not claim to simulate weapon damage rate or boss combat.

### Interaction policy summary

| Objective | Required physical proof | Permitted activation | Certifying |
|---|---|---|---|
| Key | GuideBot contacts key object | Existing owner pickup path | Yes |
| Fly-through trigger | GuideBot crosses exact side | Existing companion trigger path | Yes |
| Shootable switch | GuideBot reaches accepted firing candidate | Presumed hit through normal trigger path | Yes |
| Door or blastable wall | GuideBot reaches and contacts the blocking side | Rule-checked companion shot through wall path | Yes |
| Key carrier | Carrier becomes reachable, dies after three seconds, GuideBot contacts dropped key | Timed owner damage, then normal drop and pickup | Yes |
| Boss | GuideBot reaches firing point and has live sight | Immediate owner damage and normal boss death | Yes |
| Reactor | GuideBot reaches firing point and has live sight | Immediate owner damage and normal reactor death | Yes |
| Exit | GuideBot crosses exact exit side | Existing trigger and end-level path | Yes |

The optional physical-flare mode can strengthen switch and door proof, but its
absence does not weaken the canonical contract above.

## Existing engine support

### Full headless simulation already exists

`android/app/src/main/cpp/headless/input_demo_headless_main.cpp` initializes a
full D2 engine and advances it with `calc_game_time()` and
`GameProcessFrame()`. The shared input-demo support can start a new level with
`StartNewGame(level)`, and the existing headless targets already provide dummy
screen, canvas, audio, mission loading, and game-data initialization.

`android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp` proves that
missions and arbitrary levels can be loaded without a real display. It is only
a static scanner today, so it cannot be extended by adding a loop around the
metadata scan. The route verifier should reuse the full-game bootstrap and tick
model from the input-demo executable.

### GuideBot pathing is already physical

The live GuideBot path originates in `d2/main/guidebot_route.c` and is consumed
by `d2/main/escort.c`. `d2/main/aipath.c` creates the path, and ordinary AI and
physics code follow it. Object size participates in FVI checks, path polishing,
and movement. A verifier that keeps this path and movement pipeline intact will
test substantially more than the metadata route graph:

- Whether the route actor can fit through each portal.
- Whether path points can be generated and polished.
- Whether the actor can turn, accelerate, and physically traverse the path.
- Whether closed, keyed, blastable, hidden, and trigger-controlled walls have
  the expected live state.
- Whether a segment-level route endpoint is precise enough to touch the real
  objective.

### The route model contains suitable interaction targets

`android/app/src/main/cpp/shared/level_metadata_scan.h` already records route
step and activation kinds, target segment and side, activation and aim
positions, switch guidance candidates and quality, expected opened links, key
or carrier identity, and the path terminal. These records are sufficient to
state an objective and to decide which interactions are permitted after the
GuideBot reaches the relevant pose.

### Headed automation can host the same controller

`android/app/src/main/cpp/shared/game_automate.cpp` already exposes route
guidance and direct objective-completion actions. Those actions are useful as
planner tests but are not physical confirmation because they teleport the
player and apply effects directly. A new automation action should start and
observe the shared confirmation controller rather than duplicating its logic.

## Required engine adaptations

### 1. Make live route support platform-independent

Most live route integration in `d2/main/guidebot_route.c`, `d2/main/escort.c`,
and `d2/main/aipath.c` is currently guarded by `__ANDROID__`. A Windows or Linux
headless build therefore cannot call exactly the same goal selection and path
rules even though it can compile the shared metadata planner.

Introduce a product feature macro, tentatively
`DXX_GUIDEBOT_ROUTE_PLANNER`, that is enabled for:

- The Android game.
- The new D2 route-confirmation headless target.
- A desktop headed debug build when explicitly requested.

Move only route-planning and route-path behavior under that macro. Keep Android
logging, JNI, filesystem, and UI behavior under their existing platform guards.
Do not define `__ANDROID__` for a host executable.

This extraction is mandatory for exact headed/headless parity. Calling the
metadata planner directly from the headless front end would reproduce a related
algorithm, not the actual in-game GuideBot goal provider.

### 2. Add a designated verification actor and owner

The controller should use the real companion robot object after normal level
startup and cage release. It should retain robot AI and physics but bind
inventory and player-authorized interactions to a designated player owner.

The canonical actor radius is:

```
max(loaded player ship radius, loaded GuideBot object radius)
```

Both source radii and the effective radius must be written to the result. The
actor should physically use the effective radius, rather than applying it only
to the static graph. This proves that any route marked confirmed accommodates
both actors. An optional diagnostic profile may also run the natural GuideBot
radius to distinguish an overly narrow player passage from a general routing
failure, but that profile cannot certify the canonical result.

### 3. Give the actor physical key pickup semantics

Current collision setup disables robot-powerup collision, and the robot
powerup collision handler is effectively empty. Globally allowing every robot
to collect powerups would change normal gameplay and is not appropriate.

Add a verifier-only collision observer for the designated actor:

1. Use the real physics motion and a swept object overlap test against the
   specific key object selected by the planner.
2. Require actual geometric contact at the effective actor radius.
3. On contact, invoke the authoritative powerup acquisition path for the
   verification owner so normal key flags, messages, sound suppression, object
   removal, and inventory rules are applied as closely as practical.
4. Confirm that the correct key flag is now present and the key object no longer
   exists.

The adapter must never grant a key because the GuideBot entered the target
segment or reached a path endpoint.

For a key carried by a robot, the verifier first reaches the planner-approved
combat pose and performs the permitted presumed-combat action. It then lets the
normal death and egg-drop sequence run, discovers the resulting key object by
stable type and ID rather than object number, replans to it, and physically
collects it through the same collision adapter.

### 4. Give the actor physical fly-through trigger semantics

Player physics requests a traversed-segment list and player object processing
calls `check_trigger` for each crossed side. GuideBot physics does neither by
default. However, `d2/main/switch.c` already accepts the companion robot as a
valid trigger activator.

In confirmation mode only:

1. Request the physics segment list for the designated actor.
2. Resolve every actual segment transition to its crossed side.
3. Call the normal `check_trigger(segment, side, Buddy_objnum, 0)` path.
4. Record each crossed side and any trigger it activated.
5. Verify the expected trigger state and wall or world changes after the frame.

This tests an actual crossing, including multi-segment motion, disabled and
one-shot triggers, rather than assuming that a route through the target segment
would activate the trigger.

### 5. Presume switch hittability only after physical positioning

The user-approved presumption applies to the shot, not the travel needed to
take it. The verifier must fly to a shared-planner firing or guidance candidate
using normal navigation. Once it reaches that pose and the candidate still
passes the shared hittability policy, it may invoke the existing trigger path
as a companion shot:

```
check_trigger(source_segment, source_side, Buddy_objnum, 1)
```

The controller then waits for engine effects and verifies the expected opened
links or other trigger outcome. A call that produces no expected state change
does not confirm the objective.

Candidate quality is part of the result. The canonical policy should accept
only qualities explicitly designated as confirmed by the shared planner. A
steep or approximate candidate may be useful for headed diagnosis but should
produce a partial result unless the route contract later elevates it.

No projectile ballistics are required for this first implementation. The
normal collision path already treats a companion projectile as player-owned
for trigger purposes, so the synthetic shot preserves the relevant activation
semantics without pretending to prove aim, weapon range, or line timing.

### 6. Shoot doors and walls through normal wall rules

The GuideBot's path-openability test can recognize that its owner has a key,
but actual robot collision behavior is not guaranteed to open every keyed door
while the bot is in `AIM_GOTO_OBJECT`. A route could therefore be planned
through a door and then stall against it.

When the designated actor contacts or stalls at a door on the current path, the
controller may issue a companion/player-authorized synthetic shot through the
normal wall hit path. The wall code, not the controller, decides whether it
opens based on:

- Current owner key flags.
- Door key requirement.
- Locked and buddy-proof flags.
- Blastable or hidden wall state.
- Controlling trigger and wall animation state.

The controller waits for the ordinary door or blast animation to make the
portal passable before resuming. A missing key, locked door, buddy-proof wall,
or unchanged state is a failed interaction with the exact wall and side
recorded. This gives the actor the requested ability to shoot doors without
bypassing their rules.

Ordinary blastable GuideBot cage release is setup, not a `Next` objective. It
should use the normal wall destruction path before route confirmation starts
and be recorded separately in the result.

### 7. Implement the deterministic progression-actor rules

Carrier, boss, and reactor activation now have explicit certifying rules in the
canonical contract. They should be implemented as controller policies rather
than changes to ordinary gameplay AI:

- Key carriers are frozen, armed by live reachability, killed after three game
  seconds, and allowed to produce their real contained key through the normal
  death path.
- Bosses are frozen and have teleport, cloak, attack, gate, and spew behavior
  suppressed. They receive lethal owner damage on the first frame with both an
  approved firing pose and verified live sight.
- Reactors receive lethal owner damage under the same firing-pose and live-sight
  condition.

All three rules must emit the exact policy event that authorized them and verify
the later engine mutation. The controller does not directly manufacture a key,
mark a boss dead, set the reactor-destroyed flag, or open resulting walls.

The result calls these `timed_carrier_death` and `sighted_instant_kill`, not
`physical_weapon_kill`, so the scope of the proof remains clear.

### 8. Suppress companionship distractions, not navigation constraints

Unmodified `do_escort_frame` can return to the player, fight enemies, speak,
refresh its goal on timers, or choose social behavior unrelated to the route.
Those behaviors make certification noisy and can erase the selected objective.

Add a route-confirmation AI mode that locks the companion to the verifier's
current semantic goal while retaining:

- Live route goal selection.
- `create_guidebot_route_path_to_segment` or its exact live equivalent.
- Ordinary `ai_follow_path` movement.
- Ordinary physics, FVI, collision, wall, and animation processing.
- Replanning when the world changes or the path becomes invalid.

Disable only return-to-player, combat diversion, chatter, player-distance
timeouts, and unrelated escort commands. The player object becomes a
non-interfering observer and camera anchor in canonical runs.

Two execution profiles are useful:

- `routing_sandbox`: removes ordinary robots, filters ordinary matcen spawns,
  freezes the retained carrier and boss actors, and applies the deterministic
  progression rules above. This is the canonical topology and progression
  proof.
- `full_world`: retains ordinary hazards and AI for reproducing live gameplay
  interference. This is diagnostic and must not replace the canonical result.

### 9. Optional physical flare shots

The GuideBot already creates flares with `Laser_create_new_easy` using its own
orientation, position, object number, and `FLARE_ID`. Existing weapon-wall
collision recognizes companion projectiles and lets flares open doors and
activate wall triggers. This makes a real-shot stretch profile practical.

Add `interaction_mode=physical_flare` after the canonical presumed-hit mode is
stable:

1. Fly to the same accepted switch or door firing pose.
2. Turn the GuideBot toward the exact wall aim point through normal orientation
   updates, with an angular tolerance before firing.
3. Create one companion flare through the existing GuideBot weapon path.
4. Track the flare by object signature through ordinary weapon physics.
5. Require its first relevant collision to name the expected wall and side.
6. Let ordinary collision invoke the door or trigger behavior.
7. Verify the same world postcondition as the presumed-hit mode.

Use bounded aim, shot, and impact budgets. A flare that expires, sticks, bounces
away, or hits another wall is recorded as a physical-shot failure. Permit only
a small deterministic retry count and delete leftover verifier flares through
normal object cleanup before replanning.

This mode is initially additional evidence. A physical-flare failure does not
invalidate a canonical result whose contract permits presumed switch
hittability or a rule-checked door shot. The result stores both outcomes so the
physical mode can become canonical later if corpus results show that it is
stable. Bosses and reactors continue to use sighted instant kill; a flare's low
damage should not be mistaken for the contracted destruction action.

## Shared confirmation controller

Place the engine-neutral controller and serializer in shared sources, for
example:

- `android/app/src/main/cpp/shared/route_confirmation.h`
- `android/app/src/main/cpp/shared/route_confirmation.cpp`

It owns no renderer, command-line parser, or automation protocol. Its public
surface should be limited to start, tick, stop, query status, and serialize
result. Both front ends call the same functions.

### State machine

1. `BOOT_LEVEL`
   - Validate mission, level, difficulty, actor, and route feature generation.
   - Capture mission and level fingerprints, radii, RNG, and initial world hash.
2. `RELEASE_CAGE`
   - Find and release the ordinary GuideBot cage through normal wall behavior.
   - Do not add it to the semantic route.
3. `REFRESH_ROUTE`
   - Rescan current world state and ask the live GuideBot route provider for the
     first pending semantic goal.
   - Compare its signature to any expected route oracle, but do not let the
     oracle drive the simulation.
4. `BUILD_PATH`
   - Invoke the same path creator as the in-game GuideBot.
   - Refine or append only the final waypoint needed for a real key collision,
     trigger-side crossing, or switch firing candidate. Validate added path
     segments through FVI at the effective radius.
5. `FLY`
   - Advance complete fixed-timestep engine frames.
   - Track position, segment, path index, crossed sides, wall contacts,
     distance, and progress watchdog state.
6. `INTERACT`
   - Key: wait for actual collision adapter event.
   - Fly-through trigger: wait for actual crossed-side event.
   - Switch: require reached firing pose, then use permitted synthetic shot.
   - Door or blastable wall: request a normal rule-checked wall shot on contact.
   - Carrier: arm on release, wait three game seconds, apply owner damage, and
     wait for the normal key drop.
   - Reactor or boss: require reached firing pose and live sight, then apply the
     contract's sighted instant kill and wait for normal death effects.
   - Exit: require physical crossing and latch success before level transition.
7. `WAIT_WORLD_SETTLE`
   - Continue frames while doors, explosions, drops, or trigger effects settle.
8. `VERIFY_EFFECT`
   - Test objective-specific postconditions against the live engine.
9. `REFRESH_ROUTE`
   - Select the next objective from the newly mutated world.
10. `DONE`, `PARTIAL`, `FAILED`, `TIMEOUT`, or `ENGINE_ERROR`
   - Finalize normalized trace and world hash.

### Progress and failure rules

Progress must be measured by more than distance to the target. Track segment
changes, waypoint index, distance along the current path, meaningful target
distance reduction, and successful world interactions.

On a stall:

1. Record the actor pose, segment, current waypoint, nearest wall and side, FVI
   result, current keys, and closest achieved target distance.
2. Permit a bounded deterministic replan against the current world.
3. Optionally reject the implicated edge and make one avoidance attempt if that
   behavior is also available to the live GuideBot.
4. Terminate the leg when its frame, replan, or no-progress budget expires.

Reaching a closest possible segment is valuable diagnostic evidence but never
confirms a key collision or trigger crossing. It remains `partial`.

## Headless reproduction

Add a D2 target, tentatively `dxx-redux-d2-headless-route`, using
`cmake/dxx-headless-targets.cmake`. It should reuse a factored common bootstrap
from the existing input-demo and metadata headless programs rather than
maintaining a third copy of engine initialization.

Suggested invocation:

```
dxx-redux-d2-headless-route \
  --mission castaway_redux.mn2 \
  --mission-dir <directory> \
  --level 2 \
  --difficulty 2 \
  --profile routing_sandbox \
  --output castaway_redux-l2.route-confirmation.json
```

The executable must not sleep to real time. It runs fixed simulation frames as
fast as the CPU permits, with dummy audio and rendering. It exits nonzero for
engine errors and can use distinct status codes for failed, partial, and timeout
results if scripts need them.

Process-level parallelism is acceptable across levels because each process has
isolated engine globals. Multiple levels must not be simulated concurrently in
one process until the reset behavior is proven complete.

## Headed reproduction

Add a game automation action such as `start_route_confirmation` with the same
input object used by the headless executable. Add introspection for:

- Controller state and overall status.
- Current semantic objective and activation.
- Actor segment, path terminal, and target pose.
- Held keys.
- Last wall, trigger, pickup, or presumed shot event.
- Frames since progress and failure reason.

The existing game loop calls the controller once per frame. In visible mode the
camera can follow the GuideBot or remain with the observer player. Rendering and
camera choice must not alter the controller state or result serialization.

Automation scripts can then:

1. Start a mission and level.
2. Start confirmation with the canonical profile, which fixes seed `1` and the
   rational 60 Hz schedule.
3. Wait until the controller is terminal.
4. Assert the normalized result or selected leg fields.
5. Save a screenshot or input-demo checkpoint when a leg fails.

This is also the preferred headed reproduction for a headless failure. The JSON
input and optional checkpoint should be portable between the two front ends.

## Determinism contract

Determinism is part of route validity. A result is canonical only when it uses
the fixed seed, timestep, sandbox policy, event ordering, and build generation
defined here. Repeating the run is a validation of the result, not an averaging
process. If two canonical runs differ, the route is not confirmed until the
nondeterminism is fixed.

### One seed for every mission and level

Define one compile-time canonical seed:

```
ROUTE_CONFIRMATION_CANONICAL_SEED = 1
```

Use `1` for every mission, every level, every machine, and every canonical run.
There is no per-mission derivation and no time-based fallback.

Seed both existing RNG streams immediately before `StartNewGame(level)`:

- `D_RNG_SIM` controls path randomization, physical drops, explosions, and other
  state that can affect the route.
- `D_RNG_FX` controls presentation effects and must remain isolated from the
  simulation, but giving it the same fixed seed also makes headed presentation
  more reproducible.

Reset both stream call counters at the same boundary. Do not reseed after the
level loads, because level initialization is part of the deterministic
simulation and may legitimately consume random values. Any earlier engine or UI
RNG history is intentionally discarded at this pre-level boundary.

Canonical confirmation runs in a local single-player simulation even when the
mission supports cooperative play. Network packet timing and multiplayer spawn
selection are outside the route proof. Reject an active network game rather
than allowing its time-based spawn seeding or remote events into a canonical
run.

The canonical CLI should not require a seed argument. It always uses seed `1`
and records it. An optional `--diagnostic-seed` may be added for investigation,
but any run using it is labeled noncanonical and cannot update effective route
status or checked-in objective timing.

### RNG ownership rules

- Live engine frames and normal GuideBot path creation may consume `D_RNG_SIM`.
  That consumption is part of the reproduction.
- Route snapshotting, semantic goal selection, serialization, introspection,
  logging, cache lookup, and rendering must not consume `D_RNG_SIM`.
- Sandbox setup iterates objects in ascending object index and must not consume
  RNG when removing ordinary robots or freezing retained actors.
- At controller boundaries that should be RNG-neutral, capture simulation state
  and call count before and after. A change is a verifier invariant failure.
- Treat any simulation-stream reseed after the canonical pre-level seed as an
  invariant failure. Use the existing annotated RNG boundary to report the
  reseeding call site.
- Capture simulation RNG state and call count after level load, after sandbox
  setup, at route start, and after each completed objective in the detailed
  artifact. These checkpoints localize the first divergence.
- Never restore RNG merely to hide an unexpected call. Unexpected consumption
  is fixed at its source unless the operation is intentionally defined as
  state-preserving, as existing route parity probes already are.

Ordinary robots and matcens are absent from the canonical sandbox partly to
remove large sources of unrelated simulation RNG consumption. Stationary
carriers and bosses also cannot consume AI movement, aiming, teleport, cloak,
gate, or firing randomness before their contracted activation.

### Exact fixed 60 Hz simulation time

Do not use wall-clock timer deltas and do not assign the truncated constant
`F1_0 / 60` on every frame. Since `F1_0` is 65536, that constant does not sum to
one exact fixed-point second after 60 frames.

Use a 64-bit integer remainder accumulator that emits the deterministic sequence
of 1092 and 1093 fixed-point units needed for:

```
sum(FrameTime[0..59]) == F1_0
```

The schedule is equivalent to taking successive differences of
`floor(frame_index * F1_0 / 60)`. It uses no floating point, repeats exactly,
and makes 180 frames exactly `3 * F1_0`. Set `FrameTime` from this schedule before
each `calc_game_time()` and `GameProcessFrame()` call. Headed confirmation uses
the same controller-provided frame delta rather than display refresh or Android
frame timing.

Route elapsed time starts at zero after level load, cage release, sandbox
normalization, actor sizing, and initial world hashing, immediately before the
first live goal selection. Setup time is recorded separately only in detailed
diagnostics and is not included in objective timings.

An objective completion timestamp is captured after the first engine frame in
which its complete postcondition is observed. For example:

- A key completes when the physical contact has produced the correct owner key
  flag and removed the key object.
- A trigger completes when the crossed-side call has produced its expected
  world mutation.
- A door completes when its portal becomes physically passable, not when the
  opening animation begins.
- A carrier objective completes when the dropped key is physically collected,
  not when the carrier timer fires or the carrier dies.
- A boss or reactor completes when normal engine death state and required route
  effects are observable, not when lethal damage is requested.
- An exit completes on the latched physical exit-side event.

### Stable execution ordering

For each fixed frame, use one documented order:

1. Controller pre-frame policy, including due carrier timers and actor goal
   lock.
2. Fixed `FrameTime` assignment.
3. Normal engine frame processing.
4. Controller post-frame observation of movement, collision, triggers, walls,
   deaths, and pickups.
5. At most one semantic objective completion.
6. World hash, RNG checkpoint, event append, and next-goal refresh if needed.

When multiple events occur in one frame, sort observations by event class and
then stable object, segment, side, wall, or trigger identity. Object setup and
cleanup also use ascending stable indices. Do not run two levels in one process
or parallelize work inside the engine. Corpus parallelism remains one isolated
process per level.

All automation input is zeroed except explicit verifier actions. Network state,
menus, pause behavior, audio callbacks, renderer cadence, and wall-clock time
must not influence controller decisions.

### Cross-mode validation

Every maintained fixture is run at least:

1. Headless canonical run A.
2. Headless canonical run B in a fresh process.
3. Headed canonical run with rendering enabled.

Compare, without tolerance:

- Ordered objective signatures.
- Objective completion frames and fixed-point times.
- RNG states and call counts at objective boundaries.
- Progression world hashes.
- Interaction outcomes and terminal status.

Presentation-only FX state and wall-clock runtime are excluded. A Windows host
and Android comparison is also required for the high-value Castaway and
Obsidian fixtures. If semantic state or timing differs across platforms, record
the first mismatching boundary and treat it as an engine determinism bug rather
than loosening the comparison.

### Compact successful regression metadata

Checked-in mission JSON should remain small. When and only when canonical engine
confirmation succeeds, store:

```
"route_confirmation": {
  "status": "confirmed",
  "generation": 1,
  "seed": 1,
  "fixed_hz": 60,
  "objective_seconds": [4.516667, 11.250000, 19.083333],
  "total_seconds": 19.083333
}
```

`objective_seconds` is cumulative simulation time from route start and is in
the same order as the confirmed semantic objective chain already stored for the
level. It has one entry per actionable objective, including exit, and excludes
the start marker, cage release, sandbox setup, and diagnostic closest-approach
steps. Its length must match the confirmed actionable-objective count.
`total_seconds` equals the last objective time but is retained for easy
reporting.

Serialize seconds directly from fixed-point ticks with deterministic integer
division and six fractional decimal digits. Do not convert through `float` or
locale-sensitive formatting. The detailed temporary artifact retains leg
durations, frames, RNG checkpoints, paths, and failure diagnostics; these do not
belong in successful checked-in mission JSON.

An unsuccessful run stores status and the minimum reproduction identity needed
by the regression format, but it does not publish successful objective timing.

## Reproduction input contract

The complete reproducibility key is:

- Mission filename and content fingerprint.
- Extra mission directory or archive fingerprint.
- Level number and difficulty.
- Game and route algorithm generation.
- Route topology snapshot or generation hash.
- Loaded player radius, loaded GuideBot radius, and effective maximum radius.
- Canonical fixed 60 Hz rational timestep schedule.
- Canonical simulation and FX seed `1`, initial states, and call counts.
- `routing_sandbox` or `full_world` profile.
- Per-leg, no-progress, world-settle, and total frame budgets.
- Replan and avoided-edge budgets.
- Accepted switch guidance qualities.
- Optional expected initial planner status and ordered objective signatures.
- Optional checkpoint for a mid-level or later-leg reproduction.

The expected route is an oracle used to detect planner drift. It is not a script
that tells the engine which objective to complete next. The live goal provider
remains authoritative.

## Detailed reproduction artifact schema

The temporary headed or headless reproduction artifact uses stable, ordered JSON
with a schema name such as `dxx-guidebot-route-confirmation-v1`. Avoid
wall-clock timestamps and unstable object numbers in normalized comparison
fields. This detailed schema is not copied wholesale into mission JSON.

Top-level fields should include:

- Mission and level identity and fingerprints.
- Engine, planner, topology, and confirmation generations.
- Profile, seed, fixed timestep, and budgets.
- Player, GuideBot, and effective actor radii.
- Static planner status and engine confirmation status.
- Overall result: `confirmed`, `partial`, `failed`, `timeout`, or `unsupported`.
- Setup actions including cage release.
- Initial and final world-state hashes.
- Ordered live objective signature digest.
- Ordered leg records.

Each leg should include:

- Objective signature, step kind, activation kind, target, and source wall or
  trigger identity.
- Switch candidate and guidance quality when applicable.
- Actor start and end pose and segment.
- Requested path endpoint, actual terminal, path length, and replans.
- Frame count, simulated time, and traveled distance.
- Crossed sides and observed trigger events.
- Door or wall shot attempts and engine decisions.
- Key object signature and actual contact event.
- Contract interaction kind, such as `timed_carrier_death` or
  `sighted_instant_kill`, and the event that authorized it.
- Expected and observed world effects.
- Before and after objective world hashes.
- Outcome and exact failure reason.
- Stall edge, FVI result, and closest achieved position when incomplete.

Detailed per-frame traces are optional even in this artifact. Successful
checked-in mission JSON uses only the compact seed and objective-time metadata
defined above. Unsuccessful checked-in output keeps only status and the minimum
reproduction identity required by the regression format; full failure details
stay in temporary artifacts.

## Regression integration and status rules

The checked-in route status should distinguish planning from confirmation:

```
static_route.status
route_confirmation.status
effective_route.status
```

Suggested effective status rules:

- `ok`: the static planner found a complete route and the matching engine run
  confirmed every required leg under the canonical profile.
- `partial`: the planner was partial, the engine reached only a permitted
  closest approach, an approximate switch proof was used, or some legs remain
  unconfirmed.
- `failed`: no valid planned progression exists, a mandatory physical leg
  failed, or an expected interaction could not mutate the world correctly.
- `not_run`: confirmation has not been generated for this exact mission,
  engine, route generation, radius, and profile input hash.
- `unsupported`: the engine or mission type lacks a supported verification
  actor or interaction.

During rollout, existing planner `ok` entries must not silently be presented as
engine-confirmed. They should retain their static result and show confirmation
as `not_run`. Once confirmation is required for effective `ok`, generation
scripts and all consumers must use `effective_route.status` consistently.

Any change to movement, FVI, object size, GuideBot path creation, door logic,
trigger behavior, pickup behavior, route planning, or switch proof generation
invalidates the confirmation generation or input hash. The canonical seed,
rational timestep policy, sandbox event ordering, and seconds serialization are
also part of that generation.

## Performance strategy

Do not replace the metadata planner with brute-force simulation. Use the planner
to reduce the simulation to one expected progression chain, then use the engine
as the physical oracle.

Run in tiers:

1. Focused reproduction for a single mission, level, or failed leg.
2. Changed-route verification for levels affected by the current diff.
3. All current partial and failed levels plus high-value known-good fixtures.
4. Full corpus verification in scheduled or explicit regeneration runs.

Headless simulation can advance without rendering, audio, or real-time waits,
but GuideBot flight may still require thousands of frames per level. Benchmark
before fixing corpus-wide budgets. Cache results by the complete reproduction
key. Use one process per level for safe parallelism and early terminate on a
mandatory failed leg.

## Scope boundaries and known risks

- Initial support is D2 and D1 missions loaded by the D2 engine. Native D1 has
  no GuideBot and should report `unsupported` until a clearly labeled synthetic
  actor design is approved.
- The canonical run has one verification owner. Cooperative ownership can be a
  later profile because keys are per-player and GuideBot ownership matters in
  network games.
- Exact key contact and trigger crossing require new hooks, but neither requires
  a replacement collision or trigger implementation.
- Final path points currently tend toward segment centers. Physical pickup and
  switch confirmation will require careful final-waypoint refinement without
  bypassing FVI.
- Dynamic carrier drops cannot rely on object indices. Use object type, ID,
  creation provenance, and position signatures.
- Door and explosion animations need explicit settle budgets and postconditions.
- Exit triggers can start end-level transitions immediately. Latch the expected
  event and terminate the current verification cleanly before another level is
  loaded.
- Full-world simulation may kill or distract the actor. Keep that result
  diagnostic and separate from the canonical routing sandbox.
- Floating-point and AI randomness can cause drift. Fixed frames and RNG state
  are necessary, and every fixture must be run twice to check normalized trace
  equality.
- A topologically valid path can still fail physical navigation. Such failures
  are high-quality regression findings and must not be converted to success by
  teleporting, shrinking the actor, or granting the objective directly.

## Staged implementation plan

The phases below are ordered so every phase leaves a buildable, testable product
and produces evidence needed by the following phase. The first useful vertical
slice ends at Phase 7 with physical blue-key acquisition in Castaway Redux level
2. Later phases expand the supported activation types without replacing the
controller.

### Phase 0: Freeze the contract and select fixtures

Likely files:

- This plan.
- `android/app/src/main/cpp/shared/route_confirmation.h`.
- A new schema or serializer test under the existing Android native test area.

Implementation tasks:

1. Define input enums for profile, interaction mode, terminal status, and
   accepted switch sight quality.
2. Define stable objective signatures that do not use mutable object numbers as
   identity. Include type, ID, authored position, segment, side, trigger, wall,
   key kind, and activation kind as applicable.
3. Define setup events, path events, interaction events, leg results, and the
   top-level normalized result.
4. Define the canonical defaults:
   - `routing_sandbox`.
   - Simulation and FX seed `1` for every mission and level.
   - Presumed switch and door shots.
   - Three-second carrier timer expressed as `3 * F1_0`.
   - Stationary carriers and bosses.
   - Rational fixed 60 Hz frame schedule whose deltas total exactly `F1_0` every
     60 frames.
   - Maximum of loaded player and GuideBot radii.
5. Inventory or create fixtures for:
   - Open static key.
   - Narrow path to a key.
   - Keyed door.
   - Fly-through trigger.
   - Shootable switch.
   - Initially reachable key carrier.
   - Carrier released by a switch or door.
   - Boss and reactor with visible and occluded firing points.
   - Exit and secret exit.
6. Reserve Castaway Redux levels 1 and 2 and Obsidian level 10 as end-to-end
   fixtures.

Tests and evidence:

- Serializer golden test for stable key order.
- Invalid-input tests for missing mission, invalid level, negative budgets, and
  incompatible profile options.
- Schema round-trip or field-completeness test.

Exit condition: every requested success, failure, timeout, setup mutation, and
stretch-flare result can be represented without renderer or CLI state.

### Phase 1: Extract the live route feature from the Android platform guard

Likely files:

- `d2/main/guidebot_route.c` and its header.
- `d2/main/escort.c`.
- `d2/main/aipath.c`.
- D2 CMake source and definition lists.
- Android native CMake definitions.

Implementation tasks:

1. Introduce `DXX_GUIDEBOT_ROUTE_PLANNER` for semantic goals and route-aware
   path generation.
2. Enable it in Android game builds and the future route headless target.
3. Keep JNI, Android logging, assets, and UI under `__ANDROID__`.
4. Replace platform-only logging calls in shared route code with a small
   environment-neutral route log boundary or compile-time no-op.
5. Audit every affected conditional in the three D2 files so the new host path
   selects the same objective and side passability rules as Android.
6. Audit whether any equivalent D1 hooks are required. Native D1 should compile
   unchanged and report route confirmation unsupported rather than accidentally
   gaining a partial GuideBot implementation.

Tests and evidence:

- Existing Windows host D1 and D2 builds remain successful.
- Android native build remains successful.
- A small native test feeds one route snapshot into both build variants and
  compares the selected objective signature.

Exit condition: Android and host D2 can call the same live goal selection and
route path entry points without defining `__ANDROID__` on the host.

### Phase 2: Factor reusable full-engine headless startup

Likely files:

- `android/app/src/main/cpp/headless/input_demo_headless_main.cpp`.
- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`.
- New `android/app/src/main/cpp/headless/headless_game_bootstrap.*`.
- `cmake/dxx-headless-targets.cmake`.
- D2 CMake target list.

Implementation tasks:

1. Extract PHYSFS, configuration, game-data, dummy sound, screen, canvas, and
   shutdown handling shared by full-engine headless tools.
2. Add a start-new-level operation that loads a mission, skips introductions,
   calls the normal new-game path, and reports startup failure without entering
   an interactive menu.
3. Seed `D_RNG_SIM` and `D_RNG_FX` to `1` and reset their call counters at the
   canonical boundary immediately before normal new-level startup.
4. Add a fixed-frame driver with a 64-bit remainder accumulator that emits the
   exact rational 60 Hz `FrameTime` schedule, updates timers, calls
   `calc_game_time()`, and advances `GameProcessFrame()` without wall-clock
   sleeps.
5. Preserve the input-demo runner's current deterministic behavior while moving
   only common initialization.
6. Add `dxx-redux-d2-headless-route` with a minimal CLI and JSON error output.
   Canonical mode does not expose a variable seed or timestep.
7. Guarantee cleanup on success, parse failure, level-load failure, and engine
   exception paths.

Tests and evidence:

- Existing input-demo headless tests remain unchanged and pass.
- New tool loads one built-in D2 level, advances 120 frames, and exits cleanly.
- The 60-frame and 180-frame accumulated deltas equal `F1_0` and `3 * F1_0`
  exactly.
- Level startup records seed `1`, post-load RNG state, and call count.
- Invalid mission and invalid level invocations return stable error JSON.

Exit condition: one command can boot a real D2 level and advance deterministic
full-engine frames without renderer or audio dependencies.

### Phase 3: Implement the controller skeleton and normalized trace

Likely files:

- New `android/app/src/main/cpp/shared/route_confirmation.h`.
- New `android/app/src/main/cpp/shared/route_confirmation.cpp`.
- New `android/app/src/main/cpp/shared/route_confirmation_json.cpp` if keeping
  serialization separate reduces coupling.
- Headless route main and native tests.

Implementation tasks:

1. Implement start, tick-before-frame, tick-after-frame, stop, status query, and
   serialization entry points.
2. Keep all controller state in one explicit structure. Do not spread verifier
   state through unrelated game globals.
3. Capture deterministic input fields, canonical seed, RNG states and call
   counts, mission fingerprint, level fingerprint, route generation, topology
   hash, and radii.
4. Implement terminal-state ownership so only the controller decides confirmed,
   partial, failed, timeout, unsupported, or engine error.
5. Add ordered event recording with fixed-size or bounded vectors and explicit
   overflow failure.
6. Add per-leg, no-progress, interaction, settle, and total budgets.
7. Add world-state hashing for progression-relevant walls, triggers, keys,
   carriers, bosses, reactor, and exit state. Exclude renderer and unstable
   pointers.
8. Track route elapsed fixed-point ticks independently from level setup time and
   capture cumulative completion ticks on the first postcondition-observed
   frame for each objective.
9. Add RNG-neutral guards around route snapshotting, semantic goal selection,
   hashing, serialization, and introspection.

Tests and evidence:

- Pure controller transition tests with a fake view.
- Budget boundary and event-capacity tests.
- Two 120-frame runs produce identical normalized JSON and hashes.
- Integer-only six-decimal seconds formatting golden tests, including repeating
  1/60 values and values above one hour.
- A deliberate RNG call from a neutral controller stage produces an invariant
  failure with the responsible stage.

Exit condition: the headless tool can run the controller to a deliberate
`unsupported_objective` result with a deterministic trace.

### Phase 4: Normalize the canonical no-ordinary-robots sandbox

Likely files:

- Shared route confirmation controller.
- A narrow D2 route-confirmation hook header.
- D2 AI or matcen creation call sites only where a shared controller call cannot
  safely enforce the policy after the frame.

Implementation tasks:

1. Classify every robot object after level startup as GuideBot, key carrier,
   boss, or ordinary robot.
2. Remove ordinary robot objects directly without death behavior or drops.
3. Disable or intercept matcen production. Remove an ordinary dynamic robot
   before its first active AI frame while retaining dynamically created key
   carriers and bosses.
4. Freeze retained carrier attack, movement, and velocity while preserving its
   object, collision volume, contains fields, and death behavior.
5. Freeze boss attack, movement, velocity, teleport, cloak, gate, and spew while
   preserving its authored pose, collision volume, and death behavior.
6. Make the player observer non-interfering while keeping a valid player object
   for inventory and authorized damage ownership.
7. Record every removal, retention, and suppression decision in setup output.
8. Compute the confirmation route snapshot only after normalization.

Tests and evidence:

- Mixed-robot fixture reports the expected retained and removed counts.
- Removed robots do not drop objects, score, explode, block paths, or consume
  simulation RNG.
- Matcen fixture cannot introduce an active ordinary robot.
- Carrier and boss positions remain byte-identical across 600 fixed frames.

Exit condition: the canonical sandbox contains only its allowed progression
actors and produces a stable post-normalization world hash.

### Phase 5: Establish and size the verification actor

Likely files:

- Shared controller.
- `d2/main/escort.c` and route-confirmation hook header.
- Headed or headless object setup support.

Implementation tasks:

1. Find the companion object and fail clearly if the level has none. Do not
   silently spawn a different actor in the first implementation.
2. Release its ordinary blastable cage through normal wall damage as a setup
   operation, not a route objective.
3. Load the active player object size and companion object size from live engine
   data and set the actor's physical size to their maximum.
4. Record natural and effective sizes. Restore the natural size when stopping a
   headed confirmation without leaving the level.
5. Bind pickup and authorized interaction state to the verification player.
6. Add a confirmation-only escort mode that suppresses chatter,
   return-to-player, combat diversion, and away timers but retains path
   following and physics.
7. Keep the player observer outside collision and goal-attraction behavior while
   retaining valid engine ownership.

Tests and evidence:

- Fixture with player larger than GuideBot uses player size.
- Fixture with GuideBot larger than player uses GuideBot size.
- Cage release changes the actual wall state and is absent from route legs.
- Actor does not abandon its locked objective after normal escort timeout.

Exit condition: the same physically sized GuideBot actor is available to both
headed and headless controllers without companionship behavior interference.

### Phase 6: Select live goals and fly normal GuideBot paths

Likely files:

- Shared controller.
- `d2/main/guidebot_route.c`.
- `d2/main/escort.c`.
- `d2/main/aipath.c`.

Implementation tasks:

1. Refresh live route state and call the exact in-game next-goal provider.
2. Capture the semantic goal separately from its current physical endpoint.
3. Build the path through the existing GuideBot route path function.
4. Let normal `ai_follow_path`, FVI, collision, and physics move the actor.
5. Add final-waypoint refinement for a target position or side only after an
   effective-radius FVI check confirms the refinement is traversable.
6. Record requested endpoint, actual path terminal, path points, segment
   transitions, wall contacts, and target distance.
7. Add the progress watchdog and one bounded deterministic replan. Do not add an
   edge-avoidance retry until the equivalent live GuideBot behavior exists.
8. Rescan and reselect after every verified world mutation rather than walking
   an initial static route list.

Tests and evidence:

- Open objective fixture reaches its exact position tolerance.
- Narrow fixture fails at the same effective radius in planner and physics.
- Blocked fixture records its terminal wall, side, FVI result, and closest pose.
- Two runs select identical goals and segment sequences.

Exit condition: the actor can physically reach an open semantic objective and
can localize a real engine navigation failure without any interaction shortcut.

### Phase 7: Implement physical static-key pickup

Likely files:

- Shared controller and route-confirmation hooks.
- Narrow collision or physics hook in `d2/main/collide.c` or the central object
  movement path.
- Existing player powerup acquisition entry point.

Implementation tasks:

1. Register only the designated GuideBot and current expected key with the
   verification collision adapter.
2. Detect swept contact from the actor's previous to current position using the
   effective actor radius and actual key radius.
3. Reject segment arrival or near misses without contact.
4. On contact, invoke the authoritative powerup acquisition behavior for the
   verification owner and remove the real object through normal pickup rules.
5. Verify the exact owner key flag and key object disappearance.
6. Record previous pose, contact pose, radii, key signature, and resulting key
   mask.
7. Refresh the live route and show that it advances from the acquired key.

Tests and evidence:

- Positive direct contact.
- High-speed crossing that requires swept rather than endpoint overlap.
- Near miss in the same segment.
- Wrong-color key does not satisfy the current objective.
- Duplicate already-owned key follows ordinary inventory behavior.
- Castaway Redux level 2 physically acquires its blue key headlessly.

Exit condition: Castaway level 2 advances beyond its blue-key objective only
after a recorded GuideBot-key collision.

### Phase 8: Implement deterministic key-carrier release and drop

Likely files:

- Shared controller.
- Route snapshot reachability helpers.
- Existing robot damage and death paths.

Implementation tasks:

1. Identify retained carrier objects from their instance `contains_type`,
   `contains_id`, and `contains_count` fields.
2. Evaluate the actor's current live reachable component after every relevant
   wall, trigger, or object-creation event.
3. Arm an initially reachable carrier at confirmation start.
4. Arm a sealed carrier on the first frame its segment becomes reachable.
5. Arm a dynamic carrier on its creation frame.
6. Store `armed_at` in game time and fire exactly at `armed_at + 3 * F1_0`, even
   if fixed Hz changes.
7. Apply lethal owner damage through `apply_damage_to_robot`, then wait for
   ordinary death and egg-drop completion.
8. Locate the dropped key by creation provenance, type, ID, and position rather
   than recycled object number.
9. Make the new physical key the next live objective and collect it through
   Phase 7.

Tests and evidence:

- Initially reachable carrier dies at exactly three simulation seconds.
- Sealed carrier's timer does not start early.
- Opening the release wall records the arming event and starts the timer.
- Dynamic carrier uses creation time.
- Long death sequence observes settle budget.
- Missing or wrong dropped key fails without synthesizing inventory.

Exit condition: every carried key is obtained from a real dropped powerup after
the exact contracted timer and a later physical collision.

### Phase 9: Implement physical fly-through and exit-side activation

Likely files:

- `d2/main/physics.c`.
- `d2/main/object.c`.
- `d2/main/switch.c` only if a small observer hook is needed.
- Shared controller.

Implementation tasks:

1. Request the physics segment list for the designated actor in confirmation
   mode as the player object already does.
2. Convert every consecutive segment pair into the exact source side crossed.
3. Invoke `check_trigger` with the GuideBot object and `shot=0` in traversal
   order.
4. Record all sides crossed in a multi-segment frame before processing expected
   objective success.
5. Verify trigger disabled or one-shot state and all expected world effects.
6. For an exit or secret exit, latch the expected crossing before end-level code
   replaces the current level and stop simulation cleanly.

Tests and evidence:

- Single fly-through trigger.
- Multiple segment crossings in one frame.
- Entry into a target segment without crossing the trigger side.
- Disabled and one-shot trigger behavior.
- Normal and secret exit event latching.

Exit condition: no traversal trigger or exit can pass without the exact side in
the actor's physics crossing trace.

### Phase 10: Implement presumed switches and rule-checked door shots

Likely files:

- Shared controller.
- Shared switch-guidance and sight helpers.
- Existing `check_trigger` and wall-hit entry points.

Implementation tasks:

1. Select only a shared-planner switch firing candidate of an accepted quality.
2. Fly to its position tolerance through the Phase 6 path pipeline.
3. Re-run the shared hittability or sight proof from the actor's actual pose.
4. Invoke the normal companion shot trigger path only after that proof passes.
5. Verify the expected trigger mutation and opened links.
6. Detect a blocking door or blastable wall on the actor's current path through
   real wall contact or FVI.
7. Invoke the normal wall-hit path with verification-owner keys and companion
   authorization. Do not pre-open or change wall flags in the controller.
8. Wait for the door or blast animation and resume only when the portal is
   physically passable.
9. Record key mask, required key, lock and buddy-proof flags, wall response, and
   animation completion.

Tests and evidence:

- Accepted and rejected switch candidate quality.
- Switch candidate reached but live sight later blocked.
- Blue, gold, and red keyed doors with and without the key.
- Locked, buddy-proof, hidden, trigger-controlled, and blastable walls.
- Trigger call succeeds but expected wall does not change.

Exit condition: switches and doors certify only after both physical positioning
and verified normal engine world mutation.

### Phase 11: Implement stationary boss and reactor firing rules

Likely files:

- Shared controller.
- Narrow AI suppression hooks in `d2/main/ai.c` and `d2/main/ai2.c`.
- Existing robot and reactor damage entry points.

Implementation tasks:

1. Assert every retained boss stayed at its authored pose before activation.
2. Reject any teleport, cloak, attack, gate, spew, or velocity event as a
   sandbox policy violation rather than silently correcting it later.
3. Navigate to the planner-approved boss or reactor firing candidate.
4. Run a fresh FVI query from the actor's actual firing origin to target aim
   point every fixed frame after entering the candidate tolerance.
5. On the first visible frame, record the ray and apply lethal owner damage.
6. Let `start_boss_death_sequence` or reactor destruction run normally.
7. Verify boss-dead, reactor-destroyed, countdown, wall, trigger, and route
   postconditions before selecting the next objective.

Tests and evidence:

- Visible target dies on the first qualifying frame.
- Occluded target remains alive until sight exists.
- Actor in the wrong segment with incidental sight does not activate target.
- Boss remains stationary for the whole pre-activation trace.
- Boss death roll and reactor countdown settle correctly.

Exit condition: boss and reactor progression is deterministic, sight-gated, and
fully reflected by ordinary engine death state.

### Phase 12: Add the headed controller front end

Likely files:

- `android/app/src/main/cpp/shared/game_automate.cpp`.
- `android/app/src/main/cpp/shared/game_introspect.cpp`.
- New scripts under `android/game_scripts/`.

Implementation tasks:

1. Add `start_route_confirmation`, `stop_route_confirmation`, and optional
   `save_route_confirmation` automation actions.
2. Tick the same shared controller around each normal game frame.
3. Expose controller state, live objective, actor pose, path terminal, keys,
   carrier timers, target sight, last interaction, budgets, and failure details
   through introspection.
4. Add camera-follow as a presentation option that never enters controller
   inputs or normalized output.
5. Add an automation wait predicate for terminal confirmation state.
6. Save normalized output and an optional checkpoint on failure.

Tests and evidence:

- A maintained headed script reproduces the Phase 7 Castaway blue-key run.
- Introspection reports every controller state transition without screenshot
  parsing.
- Camera-follow on and off produce identical normalized traces.

Exit condition: a headless failure input can be replayed visibly through the
same controller with useful introspection and no duplicated route logic.

### Phase 13: Stretch goal for physical GuideBot flare shots

Likely files:

- Shared controller.
- Existing GuideBot flare creation and weapon-wall collision observation hooks.
- Headed introspection for active verifier projectile.

Implementation tasks:

1. Add `physical_flare` as an optional interaction mode for switches and doors.
2. Turn the GuideBot toward the expected wall aim point through normal
   orientation updates.
3. When angular tolerance is met, call the existing companion flare creation
   path using `Laser_create_new_easy`, the GuideBot object number, and
   `FLARE_ID`.
4. Track the projectile signature, path, first relevant wall impact, lifetime,
   bounce or stick state, and world effect.
5. Accept only an impact on the expected segment and side followed by the
   expected normal engine mutation.
6. Add bounded deterministic retries and cleanup.
7. Run presumed and physical modes against the same firing candidate and store
   both results.

Tests and evidence:

- Straight switch flare impact.
- Door opened by a companion flare.
- Wrong-wall impact.
- Force-field rejection.
- Flare expiry or stuck state.
- Presumed mode remains stable when physical mode is disabled.

Exit condition: physical mode proves the actual companion projectile reached
the expected wall, while remaining optional for canonical regression status.

### Phase 14: Harden determinism, budgets, and reproduction artifacts

Likely files:

- Shared controller and serializer.
- Headless CLI.
- Input-demo checkpoint or state-trace integration points.
- Test runner helpers.

Implementation tasks:

1. Make canonical mode use seed `1` unconditionally and reject active network
   state, variable timestep, and diagnostic seed input.
2. Set and record both RNG stream states and call counts at the pre-level seed
   boundary, after load, after sandbox setup, at route start, and after each
   objective.
3. Add an annotated reseed guard that fails if the simulation stream is reseeded
   after the canonical boundary.
4. Add RNG-neutral guards around planner, serializer, introspection, hashing,
   cache, and headed presentation callbacks.
5. Run every fixture twice in fresh headless processes and compare objective
   signatures, completion frames, fixed-point times, RNG states and call counts,
   world hashes, and terminal status.
6. Compare headed and headless traces after removing only presentation fields.
7. Compare Windows headless and Android headed results for Castaway levels 1 and
   2 and Obsidian level 10.
8. Audit intentional simulation RNG use during GuideBot pathing, carrier death,
   key drops, explosions, boss death, and optional flares.
9. Add failure checkpoints at leg boundaries and on terminal failure where the
   existing save system can represent the state safely.
10. Emit a complete reproduction command in every result.
11. Add hard caps for frames, events, path points, replans, settle time, and JSON
   size.
12. Make timeout output preserve the last complete state and remain valid JSON.
13. Add process exit codes for confirmed, partial, failed, timeout, unsupported,
   and engine error without making scripts parse prose.

Tests and evidence:

- Repeated-run equality for all fixtures.
- Headed/headless and Windows/Android semantic trace and objective-time equality.
- Canonical seed and rational timestep rejection tests.
- Deliberate post-start reseed and neutral-stage RNG consumption tests.
- Deliberate timeout and event-overflow tests.
- Replay from a later-leg checkpoint reaches the same failure signature.

Exit condition: every failure is deterministic or explicitly classified as an
engine nondeterminism bug with enough state to reproduce it.

### Phase 15: Integrate confirmation into regression generation

Likely files:

- Host mission metadata regeneration helpers.
- Mission JSON generator and normalizer.
- Regression comparison and summary scripts.
- Consumers that currently read one route status.

Implementation tasks:

1. Run the static planner first and retain its independent result.
2. Build a complete confirmation cache key from mission and level content,
   engine and route generations, profile, radius, seed, timestep, and policy.
3. Invoke one route headless process per eligible level.
4. For a successful canonical run, store only confirmation status, generation,
   seed `1`, fixed Hz `60`, cumulative `objective_seconds`, and `total_seconds`
   in checked-in mission JSON.
5. Convert fixed-point completion ticks to six-decimal seconds with
   locale-independent integer formatting.
6. Store detailed legs, frames, RNG checkpoints, and paths only under temporary
   artifacts.
7. Mark unmatched or stale cached results `not_run`; never reuse by filename
   alone.
8. Change every summary script and report to use `effective_route.status` when
   claiming that routing works.
9. Add comparison output for static success plus engine failure, static partial
   plus engine success, and objective-chain drift.
10. Keep the emulator metadata path capable of producing or consuming the same
   result schema so host and Android reporting do not diverge.

Tests and evidence:

- Generator fixture for confirmed, partial, failed, not-run, stale, timeout, and
  unsupported statuses.
- A planned `ok` plus engine failure cannot serialize as effective `ok`.
- Normalized regeneration is byte-stable when inputs do not change.
- Successful fixture output contains seed `1` and objective seconds aligned with
  its semantic objective chain; failed output contains no success timing array.
- Focused regeneration updates only the requested mission files.

Exit condition: checked-in effective route status is an engine-confirmed claim,
not an alias for static planner completion.

### Phase 16: Audit the mission corpus and tune performance

Implementation tasks:

1. Run Castaway Redux levels 1 and 2 and Obsidian level 10 first.
2. Investigate any downgrade before widening the run.
3. Run all routes changed by the current planner diff.
4. Run all existing partial and failed routes.
5. Run the remaining previously `ok` corpus.
6. Bucket failures into goal selection, clearance, path generation, physical
   movement, key contact, carrier release or drop, trigger crossing, switch
   sight, door rule, boss or reactor sight, exit, timeout, and nondeterminism.
7. Benchmark frames, CPU time, peak memory, output size, and cache hit rate.
8. Add safe process-level parallelism with one engine per process.
9. Set corpus budgets from observed percentiles rather than guessing them in
   advance.
10. Regenerate the complete normalized corpus and audit every prior `ok`
    downgrade and every new upgrade.

Tests and evidence:

- A machine-readable corpus summary by status and failure bucket.
- Exact reproduction commands for every non-confirmed route.
- Cache-on and cache-off runs produce identical checked-in JSON.
- Full host build, native tests, integration tests, scoped code quality, and the
  applicable Android headed fixtures pass.

Exit condition: every mission route has a static status, confirmation status,
effective status, policy and generation identity, and reproducible evidence for
any result that is not confirmed.

## Recommended first implementation slice

The smallest slice that proves the architecture is not a switch. It is a key:

1. Extract the live route feature from `__ANDROID__`.
2. Seed both RNG streams to canonical seed `1` immediately before starting
   Castaway Redux level 2 in the full headless engine.
3. Advance the exact rational 60 Hz schedule.
4. Release the GuideBot and let the live goal provider select the blue key.
5. Drive the ordinary GuideBot path and physics toward the actual key object.
6. Require real GuideBot-key contact before granting the owner key.
7. Record the first objective's cumulative simulation seconds.
8. Rescan the live world and show that the next goal changes.
9. Repeat in a fresh headless process and then headed, comparing objective,
   time, RNG, and world-hash boundaries exactly.

That slice tests the hardest architectural claims at once: exact live goal
selection, full-engine headless movement, conservative radius, physical
objective contact, world-state mutation, replanning, and headed/headless parity.
Fly-through triggers and synthetic switch or door shots can then be added as
interaction adapters without changing the controller architecture.
