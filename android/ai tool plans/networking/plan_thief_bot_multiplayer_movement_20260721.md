# Thief Bot Multiplayer Movement Investigation

## Goal

Identify why the D2 thief robot visibly snaps or warps between positions in coop while other robots move more naturally, without introducing simulation desync.

## Plan

- [done] Trace thief AI movement, multiplayer ownership, send scheduling, and remote position application
- [done] Compare the thief's movement and packet cadence with ordinary robots and the guidebot
- [done] Confirm the root cause through the ownership and movement control flow
- [done] Design ownership, simulation, packet scheduling, collision, and RNG behavior for the fix
- [done] Design focused diagnostics and automated regression coverage
- [done] Define implementation phases, risks, and validation gates
- [not requested] Implement the fix

## Constraints

- D1 has no thief robot, so source changes are expected to be D2-only unless shared infrastructure is involved
- Preserve authoritative robot simulation and eventual convergence; visual smoothing must not create divergent gameplay state
- Preserve unrelated worktree changes

## Findings

- `do_ai_frame()` calls `do_thief_frame()` without first calling `ai_multiplayer_awareness()`
- `do_thief_frame()` changes paths, orientation, and velocity using the local peer's `ConsoleObject`
- The thief's `AIM_THIEF_*` modes intentionally do nothing in the later generic mode switch, so they never reach the ownership checks used by ordinary movement modes
- A non-owner therefore predicts the thief toward its own local player until `multi_do_robot_position()` applies the owner's `shortpos` packet directly with `extract_shortpos()`
- Robot packets are flushed at about 10 Hz, which makes the repeated correction visible as snapping while still guaranteeing eventual convergence
- The guidebot has an explicit early return for remote replicas; the thief has no equivalent ownership gate

## Recommended Fix

Use the existing dynamic robot owner as the sole authority for thief locomotion, while retaining the contacted player as the authority for its own inventory. Synchronize the small amount of state that crosses those two authority domains instead of relying on every peer to run thief AI.

## Authority Model

The fix should make the authority split explicit:

| State or side effect | Authority | Replication |
| --- | --- | --- |
| Position, orientation, velocity, path, AI timers | Current `REMOTE_OWNER` | `MULTI_ROBOT_POSITION` |
| Thief `AIM_THIEF_*` mode | Current movement owner, except contact-forced retreat | Position packet plus thief contact state |
| Local inventory removal and steal RNG | Contacted player's peer | Thief contact state containing the resulting stolen-item snapshot |
| Shared stolen-item list/index | Most recent contacted player's peer | Thief contact state and late-join snapshot |
| Thief flare creation | Current movement owner | `MULTI_ROBOT_FIRE` flare subtype |
| Player collision response and HUD feedback | Contacted player's peer | Existing player-local behavior |
| Robot damage/death/drop creation | Existing robot owner rules | Existing robot damage/explode/powerup packets |

This preserves the important existing rule that only the local player can authoritatively remove items from its inventory. It also removes pathing, pose changes, and simulation RNG from non-owners.

## Proposed Runtime Flow

### 1. Movement owner frame

In the thief block in `do_ai_frame()`:

1. Call `ai_multiplayer_awareness(obj, 80)` before visibility calculation, `do_thief_frame()`, or flare creation
2. Return from the AI frame if the peer is not allowed to move the thief
3. Run `do_thief_frame()` only on the owner
4. Queue `ai_multi_send_robot_position(objnum, -1)` after the thief-specific movement update
5. Create and transmit flares only on the owner

Calling the normal position queue every processed thief frame is cheap because `robot_send_pending` coalesces repeated requests before the 10 Hz multiplayer-data flush. It also refreshes the existing robot-control timeout.

The ownership gate must precede all thief-specific work, not only the movement calls. This prevents non-owners from rebuilding paths, changing mode, consuming thief AI RNG, or creating duplicate flares.

### 2. Remote movement frame

Non-owners should do no thief AI work. Physics continues dead reckoning from the last authoritative velocity, just as it does for ordinary remote robots. Incoming `shortpos` remains authoritative.

Do not add interpolation in the first fix. Removing contradictory local acceleration should eliminate the thief-specific large corrections. Generic interpolation can be evaluated separately if small 10 Hz corrections remain visible for all robots.

### 3. Mode synchronization

Extend D2 `MULTI_ROBOT_POSITION` by one byte and bump the D2 multiplayer protocol version.

Suggested appended byte:

```text
thief_mode = AIM_THIEF_ATTACK, AIM_THIEF_RETREAT, AIM_THIEF_WAIT, or 255
```

- Send `255` for non-thief robots
- On receive, apply only the three valid thief modes and only to a live thief object
- Ignore and diagnose invalid values rather than writing an arbitrary mode
- Apply the mode before the next local collision/physics frame

Appending one byte to the fixed-size packet keeps packet parsing simple and costs one byte for every robot position packet. Sending mode only in a variable-length thief packet would conflict with the fixed `multi_message_lengths` parsing model.

Mode synchronization is required because `collide_robot_and_player()` uses `AIM_THIEF_ATTACK` to decide whether the local player can be robbed. Without it, owner-only AI would leave non-owner collision state stale.

### 4. Contact and theft state

Replace the current snapshot-only semantics of `MULTI_STOLEN_ITEMS` with a versioned, fixed-size thief state packet. The D2 protocol bump means backward packet compatibility is unnecessary.

Suggested fields:

```text
type
sender_player
flags                 // CONTACT or SNAPSHOT
remote_thief_objnum
thief_object_owner
thief_mode
Stolen_items[MAX_STOLEN_ITEMS]
Stolen_item_index
```

Contact behavior:

1. The contacted player's peer performs the existing inventory checks and steal RNG
2. It changes its local thief mode to `AIM_THIEF_RETREAT` immediately to prevent a second local steal
3. It sends a CONTACT thief state even if no item was successfully stolen, because the original behavior retreats after every attempt
4. Every receiver applies the stolen-item snapshot and validated retreat mode
5. If the receiver currently owns thief movement, it rebuilds a retreat path avoiding the sender player's segment, resets the retreat timer, and queues a position update
6. Non-owners update only collision-visible mode/timer state and wait for the owner pose

Snapshot behavior:

- Late join continues to send the shared stolen-item snapshot
- If a live thief can be mapped, include its current mode and object identity
- Do not force a new path or retreat when the SNAPSHOT flag is set
- If no live thief exists, use an invalid object sentinel and update only the stolen-item list/index

The packet handler must validate sender, mapped object range/type, thief flag, mode, and stolen index before applying object-specific state. The global stolen-item snapshot can still be applied when the object is absent, matching current late-join behavior.

### 5. Local contact path mutation

`attempt_to_steal_item()` currently creates a retreat path on whichever peer owns the contacted player, even when that peer does not own the robot. Refactor the retreat transition into a helper with two modes:

- Local movement owner or single player: set retreat state and create the path immediately
- Remote movement replica: set collision-visible retreat state, do not mutate path/velocity, and send CONTACT state so the owner builds the path

This keeps local inventory RNG where it belongs while eliminating non-owner path RNG and movement changes.

### 6. Ownership handoff

Keep the existing dynamic robot claim/release system. Do not permanently assign the thief to the host or nearest player in the first fix.

When `multi_add_controlled_robot()` grants local ownership of a thief, call a small thief-specific preparation helper:

- Validate the synchronized thief mode
- Invalidate stale local path indices left from an earlier ownership period
- Rebuild an attack or retreat path relative to the new owner's local player when appropriate
- Preserve the received authoritative position, orientation, and velocity until the new path begins steering
- Queue a forced first position after the claim

This avoids resuming a path allocated when the peer previously owned the thief or before it joined. The helper belongs with thief AI in `escort.c`, with a narrow declaration in `escort.h`; the generic ownership code should only call it for `Robot_info[id].thief`.

If diagnostics show frequent claim churn, add a thief-specific minimum ownership duration as a later tuning step. Do not pin ownership preemptively because collision-driven handoff helps the thief interact with different players.

### 7. Flare replication

The thief directly creates `FLARE_ID` with `Laser_create_new_easy()` and currently relies on every peer executing thief AI. Owner-only AI therefore requires explicit flare replication.

Add a named `MULTI_ROBOT_FIRE` subtype for a center-fired flare, rather than using an unexplained numeric value. The receive handler should create `FLARE_ID` at the robot center using the transmitted direction. Queue it through the existing per-robot fire buffer.

The guidebot uses the same direct center-fired flare pattern and is already owner-only on Android. Route both guidebot and thief flares through the new subtype so the shared behavior is correct and testable.

## Packet and Compatibility Changes

- Bump `MULTI_PROTO_VERSION` in D2 only
- Increase the fixed size of `MULTI_ROBOT_POSITION` by one byte
- Increase and redefine the fixed size of `MULTI_STOLEN_ITEMS`
- Keep D1 unchanged because it has no thief and its protocol is independently compiled
- Confirm endian handling for the appended object number in both little-endian and `WORDS_BIGENDIAN` paths
- Keep packet field validation before all object and array access
- Document the new fire subtype beside the send and receive code

Do not assume that save files need no change. The normal D2 AI save block contains `Ai_local_info`, paths, and `Stolen_items`, but the current `Stolen_item_index` does not appear in that block. Before implementation, trace the coop restore wrapper to determine whether it reconstructs or separately preserves the index. If it does not, either add the index to the existing Redux save extension with an appropriate save-version change or define and test a deterministic reconstruction rule. The first post-restore thief state packet must then synchronize the restored value to all peers.

Coop restore should clear stale runtime robot-control slots, retain the restored thief AI mode/list/index, and let normal ownership acquisition plus the first position/state packet establish live authority. Add an explicit restore regression because path indices and `Point_segs` are restored while `robot_controlled[]` is runtime-only.

## Diagnostics Before and After the Fix

Add temporary or narrowly throttled Android `COOPLOG` diagnostics before changing behavior, as required for an on-device multiplayer issue:

- Ownership claim/release: object, signature, old/new owner, slot, reason
- Owner AI sample: mode, segment, position, velocity, target player, pending send
- Remote position receive: sender, mode, segment, correction distance, previous/new velocity
- Contact state: victim, success count, old/new mode, stolen index, mapped thief
- Flare send/receive: object, owner, mode, direction

Rate-limit movement samples to approximately once per second per thief. Always log ownership transitions and unusually large corrections. A useful large-correction threshold is one robot radius, with the exact delta logged in fixed-point units.

Success evidence should show:

- Exactly one peer runs thief AI at a time
- Non-owners consume no thief path/flare RNG
- Remote correction deltas fall sharply after the initial ownership claim
- Contact always produces synchronized retreat state, including failed steals
- One flare is created per firing event on every peer

## Automated Test Design

### Native focused tests

Extract only small policy/validation helpers where this produces real test value. Cover:

- Valid and invalid network thief modes
- CONTACT versus SNAPSHOT application policy
- Owner versus non-owner retreat-path decision
- Non-thief and invalid-object packet rejection
- Stolen index bounds
- Flare subtype decode

Avoid mocking the full AI engine or duplicating packet parsing logic in a test-only implementation.

### Introspection extensions

Add a D2-only `thief` introspection object containing:

```text
present, object_num, signature, segment, position, velocity,
remote_owner, remote_slot, ai_mode, path_index, path_length,
stolen_item_index, stolen_items
```

Add diagnostic counters for owner AI frames, remote gated frames, position packets, cumulative/max correction distance, contact states, and flare sends/receives. These counters make the regression assert behavior without screenshot analysis.

### Dual-emulator integration test

Build on `android/tests/test_dual_emu_setup.ps1` and the existing JSON5 automation framework:

1. Launch a D2 coop level containing a thief on two emulators
2. Drive players to deterministic positions on opposite sides of the thief, using a purpose-built test save if normal level traversal is too slow
3. Wait for a stable movement owner
4. Assert only that owner accumulates thief AI frames
5. Sample both peers for several seconds and compare owner, mode, segment, and bounded pose error
6. Cause contact with the non-owner player
7. Assert inventory change when an eligible item exists, synchronized stolen state, and retreat mode on both peers
8. Assert the movement owner processes the contact retreat and pose error reconverges without repeated large corrections
9. Exercise a no-eligible-item contact and assert retreat still synchronizes
10. Place the thief by a door and assert one flare event per peer and matching door state

The checked-in automation should use introspection assertions and debug counters, not rendered images. Save scripts under `android/game_scripts/` and orchestration under `android/tests/`.

### Manual acceptance pass

Use `android/tests/test_manual_lan_coop.ps1` on two emulators or phones with Network and Coop Desync logs enabled. Observe the thief during approach, abrupt retreat, door navigation, ownership transfer, and player contact under both low latency and relay/NAT simulation.

Acceptance criteria:

- No repeated visible backward/sideways warps during ordinary thief movement
- A single correction may occur at ownership transfer, but it should settle within the next packet interval
- Both players see the same attack/retreat behavior and flare events
- Theft removes items only from the contacted player and stolen drops remain synchronized
- No robot ownership timeout, claim storm, duplicate flare, or duplicate drop diagnostics

## Implementation Phases

### Phase A: Instrument and reproduce

- Add throttled thief ownership, movement correction, contact, and flare diagnostics
- Add thief introspection and counters
- Capture a two-peer baseline proving non-owner AI execution and correction size

### Phase B: Establish owner-only locomotion

- Add the thief ownership gate before all thief AI work
- Queue owner positions consistently
- Append validated thief mode to robot position packets
- Add ownership-acquisition path preparation
- Bump the D2 protocol version

### Phase C: Preserve contact semantics

- Redesign `MULTI_STOLEN_ITEMS` as thief CONTACT/SNAPSHOT state
- Send contact state on successful and unsuccessful attempts
- Move retreat path creation to the movement owner
- Verify late join and coop restore snapshots

### Phase D: Replicate flares

- Add the named center-flare robot fire subtype
- Use it for thief and guidebot direct flares
- Verify door opening and one-flare-per-peer behavior

### Phase E: Regression and cleanup

- Add focused native policy tests
- Add the dual-emulator JSON5 regression
- Run scoped code quality on changed files
- Run the D2 Windows CMake build and tests through `run-windows-build.ps1`
- Build/install the Android debug APK and run the dual-emulator regression
- Run the manual acceptance pass with debug logs
- Remove temporary high-volume logs or leave only throttled transition/error diagnostics

## Risks and Mitigations

| Risk | Mitigation |
| --- | --- |
| Stale attack mode causes an incorrect steal | Carry validated thief mode in every robot position packet |
| Failed theft does not make owner retreat | Send CONTACT state even when zero items are stolen |
| New owner resumes an obsolete local path | Invalidate/rebuild thief path on ownership acquisition |
| Owner-only AI removes remote flares | Add explicit center-flare robot fire replication |
| Position packet change breaks parsing | Fixed-size append plus D2 protocol bump and packet-size tests |
| Contact packet races with an older position | Owner immediately changes to retreat and queues a new position; log and test ordering under simulated latency |
| Ownership churn causes a correction | Preserve existing minimum control time, measure churn first, tune only if needed |
| Coop restore leaves an invalid owner slot | Reset runtime control slots, validate mode, and let normal claim establish ownership |
| Coop restore loses `Stolen_item_index` | Audit the Redux save extension, persist or reconstruct the index, and assert it after restore |
| Scope expands into generic interpolation | Treat interpolation as a separate follow-up only after owner-only behavior is measured |

## Files Expected to Change During Implementation

- `d2/main/ai.c`: owner gate, position queue, flare send
- `d2/main/escort.c` and `escort.h`: retreat transition and ownership preparation helpers
- `d2/main/multibot.c` and `multibot.h`: mode byte, claim preparation, flare subtype
- `d2/main/multi.c` and `multi.h`: thief CONTACT/SNAPSHOT packet and protocol version
- `d2/main/collide.c`: keep local collision authority while avoiding remote path mutation
- `android/app/src/main/cpp/shared/game_introspect.cpp`: D2 thief state and counters
- `android/game_scripts/`: host/joiner thief regression scripts
- `android/tests/`: dual-emulator thief regression orchestration and focused policy tests
