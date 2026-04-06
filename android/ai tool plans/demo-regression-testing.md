# Demo-Based Regression Testing System

## Status: RNG infrastructure implemented, pipeline designed

## Core Problem

The .dem format records game STATE (object positions, velocities), not
player INPUTS. During playback, physics and AI are skipped -- the engine
just reads pre-computed positions from the file. This means .dem playback
cannot detect game logic regressions.

We need an INPUT-DRIVEN replay system. The .dem files are the source
material: we extract player inputs from them, then replay those inputs
through the full game engine to establish deterministic baselines.

## How Inputs Are Encoded in .dem Files

Even though the .dem records state, it contains enough data to
reverse-engineer player inputs:

**Linear controls** -- The .dem writes full-precision thrust and velocity
vectors for the player object each frame (via nd_write_object() for
MT_PHYSICS objects). Since:

```
thrust = (fvec * forward + rvec * sideways + uvec * vertical) * max_thrust / FrameTime
```

We can invert:
```
thrust_unscaled = thrust * FrameTime / max_thrust
forward = dot(thrust_unscaled, fvec)
sideways = dot(thrust_unscaled, rvec)
vertical = dot(thrust_unscaled, uvec)
```

**Rotational controls** -- Rotthrust is NOT stored in the .dem, but
orientation IS (at byte precision, ~9 bits). We derive rotational velocity
from consecutive orientations:

```
orient_delta = transpose(orient_old) * orient_new
tangles = matrix_to_angles(orient_delta)
rotvel.x ~= tangles.p / FrameTime  (pitch)
rotvel.y ~= tangles.h / FrameTime  (heading)
rotvel.z ~= tangles.b / FrameTime  (bank)
```

Then solve for rotthrust from consecutive rotvel values using the physics
substep integration (with drag*5/2 applied per FT=1/64 substep).

**Discrete events** -- Weapon fires detected from PLAYER_WEAPON,
PRIMARY_AMMO, SECONDARY_AMMO, SOUND events. Afterburner from
PLAYER_AFTERBURNER events.

## The RNG Problem

The original .dem was recorded with an unknown RNG state (seeded from wall
clock at game start). During our input-driven replay, the RNG will be
different, so:
- Robot AI decisions differ → robots move differently
- Robot firing times differ → player takes different damage
- Player trajectory diverges from .dem at interaction points

**Solution: Per-frame RNG seed snapshots.** Instead of trying to match the
original RNG, we:
1. Run the game with derived inputs + a fixed initial RNG seed
2. Snapshot the RNG seed at every frame boundary during this run
3. Store the per-frame seeds in the .dinput file
4. On future replays, reseed at each frame boundary from the snapshot

This means: within each frame, the RNG call sequence is deterministic. If a
code change adds/removes an RNG call within frame N, only that frame's later
calls are affected. Frame N+1 reseeds from the snapshot, preventing cascade.

**Implementation status: DONE** -- see d2/maths/rand.c, d1/maths/rand.c.
API: d_rand_snapshot_init(), d_rand_snapshot_frame(), d_rand_replay_init(),
d_rand_replay_frame(), d_rand_replay_stop().

## Conversion Pipeline: .dem → .dinput

### Pass 1: Parse .dem, extract raw state

Read the .dem frame by frame using the existing newdemo parser. For each
frame, extract:
- Frame timing (FrameTime from ND_EVENT_START_FRAME)
- Player object: position, orientation, velocity, thrust, segment
  (from ND_EVENT_VIEWER_OBJECT → nd_read_object)
- Delta events: energy/shield/ammo/weapon changes, weapon fires

Store this as an intermediate per-frame state array.

### Pass 2: Compute control inputs

For each frame transition (n → n+1):

**Linear controls:**
```
// Thrust is the output of read_flying_controls() before scaling
// It was scaled by max_thrust/FrameTime in controls.c
// The raw control time = component * FrameTime / max_thrust
thrust_fwd = dot(thrust_n, orient_n.fvec)
thrust_side = dot(thrust_n, orient_n.rvec)
thrust_vert = dot(thrust_n, orient_n.uvec)

// These are in the same units as FrameTime (control time = hold duration)
forward_thrust_time = fixmuldiv(thrust_fwd, frame_time, max_thrust)
sideways_thrust_time = fixmuldiv(thrust_side, frame_time, max_thrust)
vertical_thrust_time = fixmuldiv(thrust_vert, frame_time, max_thrust)
```

**Rotational controls:**
Since rotthrust is not stored, we must derive it from orientation changes.

Step A: Estimate rotvel from consecutive orientations
```
delta_orient = transpose(orient_n) * orient_{n+1}
tangles = matrix_to_angles(delta_orient)
rotvel_after = tangles / FrameTime
```

Step B: Solve for rotthrust that produces the observed rotvel change
The physics integration is:
```
For each FT substep:
    rotvel += rotthrust/mass
    rotvel *= (1 - drag*5/2)
For remainder fraction k:
    rotvel += rotthrust/mass * k
    rotvel *= (1 - k*drag*5/2)
```

Let n = FrameTime/FT (full substeps), k = (FrameTime%FT)/FT
Let d = drag*5/2, a = rotthrust/mass

The closed-form solution (for one axis):
```
rotvel_final = (rotvel_initial + a) * (1-d)^n ... + remainder terms
```

Solving for a (the acceleration = rotthrust/mass):
This is a geometric series that can be inverted analytically. See the
implementation for the exact formula.

Step C: Convert rotthrust to control times
```
pitch_time = rotthrust.x * FrameTime / max_rotthrust
heading_time = rotthrust.y * FrameTime / max_rotthrust
bank_time = rotthrust.z * FrameTime / max_rotthrust
```

Note: orientation stored at byte precision (9 bits) introduces error in
the rotvel estimate. This is where iterative refinement helps.

**Discrete controls:**
- fire_primary_count: set to 1 on frames with primary ammo decrease or
  weapon fire sound
- fire_secondary_count: set to 1 on frames with secondary ammo decrease
- afterburner_state: set from PLAYER_AFTERBURNER events
- weapon switches: detected from PLAYER_WEAPON events
- flares, bombs: detected from specific SOUND or ammo events

### Pass 3: Replay with derived inputs, record RNG + verify

Run the game engine with the computed control inputs:
1. Load the mission and level from the .dem header
2. Seed RNG with a fixed value (e.g., 0x12345678)
3. Each frame:
   a. Snapshot the RNG seed (d_rand_snapshot_frame)
   b. Set Controls struct from derived controls
   c. Run normal game frame (physics, AI, everything)
   d. Record resulting player state
4. Compare player state against .dem at each frame

The player trajectory will APPROXIMATELY match the .dem for frames without
robot interaction, and diverge where robots behave differently.

### Pass 4 (optional): Iterative refinement of rotational controls

For frames where the player position/orientation diverged from the .dem
(beyond the expected divergence from different robot behavior):

1. Identify frames with orientation error > threshold
2. For each such frame, adjust rotational control inputs to reduce the
   error at frame N+1
3. Re-run from that frame forward with adjusted inputs
4. Repeat until convergence or max iterations

The refinement is local: changing frame N's rotational input affects
N+1's orientation but shouldn't affect earlier frames. We can process
frames sequentially.

Convergence criteria:
- Position error < 1 fix unit (below shortpos precision anyway)
- Orientation error < 1 byte matrix step (below .dem precision)

For frames where divergence is caused by different robot interactions
(player got hit by robot that wasn't there in original, or vice versa),
refinement won't help and should be skipped. Detected by comparing
shield/energy delta events.

### Pass 5: Save .dinput + generate .result.json5

Write the final .dinput file with:
- Header (game type, mission, level, difficulty, initial RNG seed)
- Per-frame: timing + control inputs + RNG seed snapshot
- Event annotations (which frames have weapon fires, etc.)

Run one more verification pass with the .dinput to confirm deterministic
replay, then generate the .result.json5 baseline.

---

## Proposed File Formats

## .dinput File Format (binary)

### Header (fixed size)
```
magic[8]:           "DXINPUT\0"
version:            uint16 = 1
game_type:          uint8  (2=d1, 3=d2)
initial_rng_seed:   uint32 (fixed seed used at level start)
mission_filename:   char[9] (null-terminated, from .dem header)
starting_level:     int8
difficulty:         uint8  (0-4)
frame_count:        uint32
flags:              uint32 (bit 0: has_rng_seeds)
source_dem_hash:    uint32 (CRC32 of source .dem, 0 if recorded live)
```

### Per-frame record (fixed size, ~52 bytes)
```
// RNG seed snapshot (if flags & 1)
rng_seed:                   uint32

// Timing
frame_time:                 fix    (exact FrameTime for this frame)

// Continuous controls (in "control time" units, 0..FrameTime range)
pitch_time:                 fix
heading_time:               fix
bank_time:                  fix
forward_thrust_time:        fix
sideways_thrust_time:       fix
vertical_thrust_time:       fix

// Discrete controls
fire_primary_state:         uint8
fire_primary_count:         uint8
fire_secondary_state:       uint8
fire_secondary_count:       uint8
fire_flare_count:           uint8
drop_bomb_count:            uint8
afterburner_state:          uint8
select_weapon_count:        uint8
cycle_primary_count:        uint8
cycle_secondary_count:      uint8
energy_to_shield_state:     uint8
headlight_count:            uint8
automap_count:              uint8
rear_view_count:            uint8
toggle_bomb_count:          uint8
_pad:                       uint8
```

~52 bytes/frame with RNG seeds, ~48 without. At 50fps:
- 2 minute demo ≈ 312 KB (with RNG seeds)
- Negligible compared to .dem files

### .test.json5 (test metadata)

```json5
{
    name: "d2-level1-basic-combat",
    game: "d2",  // or "d1"
    input_file: "d2-level1-basic-combat.dinput",
    // original .dem (optional, only for provenance tracking)
    source_dem: "d2-level1-basic-combat.dem",
    mission: "d2",
    level: 1,
    difficulty: 2,
    callsign: "TEST",
    duration_secs: 120, // approximate, for timeout
    description: "Basic movement and combat on Counterstrike level 1",
    tags: ["combat", "movement", "robots", "level1"],
    required_files: ["descent2.hog", "descent2.ham", "descent2.s22"],
}
```

### .result.json5 (expected final state / regression baseline)

Generated from the first successful .dinput replay. NOT from the .dem
(since robot behavior will differ). This IS the ground truth.

```json5
{
    test_name: "d2-level1-basic-combat",
    game: "d2",
    result_version: 1,
    generated_by: "abc1234", // git commit
    generated_date: "2026-04-05",

    player: {
        energy: 67,           // fix >> 16, integer 0-200
        shields: 42,
        score: 12500,
        lives: 3,
        laser_level: 1,       // 0-5 (d2) or 0-3 (d1)
        primary_weapon: 0,
        secondary_weapon: 0,
        flags: 6,             // PLAYER_FLAGS_* bitmask
        primary_weapon_flags: 3,
        secondary_weapon_flags: 1,
        primary_ammo: [0, 200, 0, 0, 0, 0, 0, 0, 0, 0],
        secondary_ammo: [4, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        hostages_rescued: 1,
        hostages_on_board: 0,
    },

    position: {
        segment: 142,
        x: 12345678,          // raw fix values
        y: -8765432,
        z: 3456789,
    },

    level: {
        level_num: 1,
        control_center_destroyed: false,
        robots_alive: 23,
        hostages_remaining: 2,
        powerups_remaining: 15,
    },

    kills: {
        total_kills: 8,
        robots_killed_by_type: {
            "0": 3, "5": 2, "12": 3, // robot_id: count
        },
    },

    timing: {
        total_frames: 6000,
        game_time: 120000000,  // GameTime64 in fix units
    },

    environment: {
        doors_opened: 5,
        walls_destroyed: 2,
        triggers_activated: 3,
        matcens_triggered: 1,
    },
}
```

### Comparison Strategy

All result fields are compared with exact matching (fixed-point math is
deterministic on the same platform). If cross-platform discrepancies emerge,
position tolerances can be added later. Start strict.

On mismatch, report: field name, expected value, actual value, test name.

---

## What Matches the .dem and What Doesn't

**Will match exactly:**
- Player trajectory in segments with no robot/projectile interaction
  (thrust and velocity are full precision in the .dem)
- Wall collisions (walls don't move, same player trajectory = same
  collisions)
- Frame timing (stored per-frame, replayed exactly)
- Pickup events (if player follows same path, same powerups exist)

**Will NOT match:**
- Robot positions and behavior (different RNG = different AI decisions)
- Robot projectile timing and trajectories
- Player damage from robots (different robot behavior)
- Any downstream effects of different damage/robot interactions

**Key insight:** The .dem is a *bootstrap source*, not the ground truth.
The ground truth is the .result.json5 generated from the first .dinput
replay. This baseline captures what happens when the SAME inputs are fed
through the SAME code with the SAME RNG. Future code changes that alter
game logic will produce different results → regression detected.

---

## Execution Modes

### Mode 1: Android emulator, real-time
- Push .dinput and .test.json5 to device
- Launch game with `-regression-test <test_file>`
- Game loads level, seeds RNG, feeds recorded inputs at recorded frame
  times, reseeds RNG each frame from snapshot
- At completion, writes .result.json5 to device storage
- Pull result and compare from host

### Mode 2: Android emulator, accelerated
- Same as above but uncapped frame rate
- FrameTime still uses recorded values (wall clock decoupled)
- Skip vsync, run game loop as fast as possible

### Mode 3: Headless (desktop or CI)
- Build game with headless flag (no window, no GPU, no audio)
- Game loop runs at max speed using recorded FrameTime
- Writes result to file on completion
- Runs in seconds for a 2-minute demo

### Recording New Tests (going forward)

For new regression tests, record inputs LIVE instead of converting .dem:
1. Play the game with input recording enabled (Controls struct captured
   each frame alongside FrameTime and RNG seed snapshot)
2. This produces a .dinput file directly -- no conversion needed
3. Run once headless to generate baseline .result.json5
4. Add .dinput + .test.json5 + .result.json5 to the repo

This is preferred over .dem conversion because:
- Controls are captured at full precision (no lossy inversion)
- No rotational estimation error
- RNG seeds are the ACTUAL seeds from the recording session

### Converting Existing .dem Files

Use the .dem→.dinput converter for existing demo files:
1. `dem2dinput --input game.dem --output game.dinput`
2. Converter runs the multi-pass pipeline described above
3. Generates .dinput + .result.json5
4. Human reviews the replay to confirm it looks reasonable
5. Add to repo as a regression test

## Implementation Phases

### Phase 1: RNG infrastructure [DONE]
- d_rand_snapshot_init/frame, d_rand_replay_init/frame
- Per-frame seed snapshotting and replay
- Both d1 and d2

### Phase 2: Live input recording
- Hook into game loop: after Controls are populated but before
  read_flying_controls(), save the control_info struct to a buffer
- Save FrameTime per frame
- Snapshot RNG seed per frame
- Write .dinput file on demo stop or level end
- Command-line flag to enable

### Phase 3: Input replay mode
- Load .dinput file at level start
- Each frame: set Controls from .dinput, set FrameTime from .dinput,
  reseed RNG from snapshot
- Run normal game loop (full physics + AI)
- At end: serialize game state to .result.json5

### Phase 4: .dem → .dinput converter
- .dem parser (reuse existing newdemo_read_frame_information)
- Inverse physics: compute controls from thrust/velocity/orientation
- Multi-pass replay with iterative rotational refinement
- RNG seed recording during conversion replay

### Phase 5: Headless mode
- Skip rendering, texture loading, sound
- Run game loop at max speed
- Essential for CI

### Phase 6: Test runner infrastructure
- Script to run all tests and diff results
- Integration with existing run_all_tests.ps1
- Report which tests pass/fail and what changed
