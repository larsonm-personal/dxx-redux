# Deterministic Input Demo Regression System

## Status

Plan refined on 2026-04-25. The classic `.dem` system remains the visual
state-playback system. The deterministic input demo system should be added as
a parallel recorder/replayer that feeds engine-level controls through normal
physics, AI, weapons, doors, triggers, and networking.

The main purpose is gameplay regression testing, with the explicit long-term
goal of making D1/D2 code sharing and eventual de-duplication safer to attempt.

The earlier version of this plan said RNG snapshot APIs were implemented. That
was not accurate. `d1/maths/rand.c` and `d2/maths/rand.c` currently expose only
`d_srand()` and `d_rand()`, with a private static LCG seed unless
`NO_WATCOM_RAND` switches them to libc `rand()`. The first implementation phase
should be a safe D1/D2 RNG accessor and trace update.

## Current Facts From The Code

- Classic demos in `d1/main/newdemo.c` and `d2/main/newdemo.c` record world
  state. `ND_EVENT_START_FRAME` stores frame timing, `ND_EVENT_VIEWER_OBJECT`
  and render-object events store object state, and playback restores state
  without re-running the full game simulation.
- The generic engine input surface is `control_info Controls` from
  `d1/main/kconfig.h` and `d2/main/kconfig.h`. `kconfig_read_controls()` fills
  it from keyboard, mouse, joystick, touch, or automation, then
  `read_flying_controls()` in `controls.c` turns the timing fields into thrust
  and rotthrust. This is the right abstraction to record.
- The fields to record are the portable engine actions, not physical mappings:
  `pitch_time`, `heading_time`, `bank_time`, `forward_thrust_time`,
  `sideways_thrust_time`, `vertical_thrust_time`, and action states/counts such
  as fire, flare, bomb, weapon cycle/select, afterburner, converter, headlight,
  rear view, and automap where supported. Do not record raw key states,
  joystick axes, mouse axes, binding indices, or Kotlin controller mappings.
- Current coop UDP player data sends ship state such as position, orientation,
  velocity, rotvel, and short/quaternion position packets in `net_udp_send_pdata()`.
  It does not send canonical `Controls` records.
- Android introspection already gives player, position, menu, control timing,
  and multiplayer summaries in `android/app/src/main/cpp/shared/game_introspect.cpp`.
  It does not yet include the full final regression summary requested here, such
  as robots killed, robots active, triggers, doors, powerups, and level-complete
  counters.

## Goals

- Add an input-based demo system beside classic demos without removing or
  destabilizing classic `.dem` recording/playback.
- Support gameplay regression testing first, so future D1/D2 code sharing and
  de-duplication can proceed behind a repeatable safety net.
- Store generic engine input values, not keyboard, mouse, joystick, touch, or
  launcher binding details.
- Make replay deterministic enough for regression tests by controlling initial
  state, `FrameTime`, RNG, and replay ordering.
- Support accelerated and eventually no-render/headless replay so tests do not
  wait for wall clock time or GPU output.
- Support single-player first, then coop recording/replay with per-peer input
  streams and relay-server test orchestration.
- Annotate final state with readable regression fields: player stats, robot
  counts, kills by type, powerups, hostages, doors, walls, triggers, level
  outcome, and multiplayer per-player stats.
- Keep replay orchestration, sparse JSON fixture formats, test runners, and
  Android UI/debug controls under `android/` first.
- Keep changes in `d1/` and `d2/` limited to tiny generic hook points that are
  unavoidable, easy to upstream, and not tied to replay file parsing or test
  policy.

## Non-Goals For The First Tranche

- Do not make classic `.dem` playback simulate physics.
- Do not rewrite the entire input system.
- Do not require physical input mappings to be stable across devices.
- Do not attempt full deterministic network replay before single-player input
  replay and RNG control are working.
- Do not make broad gameplay RNG changes for normal play unless a test-only path
  proves insufficient.

## Core Design

### Parallel Recorder And Replayer

The new system should have its own state machine, file format, and command-line
entry points. It can share small helpers with `newdemo.c`, but it should not make
classic demo playback depend on deterministic replay.

Suggested working names:

- `DXD_STATE_NONE`
- `DXD_STATE_RECORDING_INPUTS`
- `DXD_STATE_REPLAYING_INPUTS`
- `DXD_STATE_REPLAYING_HEADLESS`

The recorder captures the post-mapping `Controls` values once per game frame,
after `kconfig_read_controls()` and touch/automation injection have populated
them, but before `read_flying_controls()` consumes them.

The replayer skips physical input polling for gameplay frames, fills `Controls`
from the input record, sets the recorded `FrameTime`, optionally sets the RNG
seed for that frame, and lets the normal game frame execute.

### Engine Input Record

Use a versioned, explicit schema based on the stable subset of `control_info`.
The record should be shared between D1 and D2 with feature flags for D2-only
fields.

Continuous fields are fixed-point control-time values:

```c
fix pitch_time;
fix heading_time;
fix bank_time;
fix forward_thrust_time;
fix sideways_thrust_time;
fix vertical_thrust_time;
```

Discrete fields are states and counts:

```c
ubyte fire_primary_state;
ubyte fire_primary_count;
ubyte fire_secondary_state;
ubyte fire_secondary_count;
ubyte fire_flare_count;
ubyte drop_bomb_count;
ubyte cycle_primary_count;
ubyte cycle_secondary_count;
ubyte select_weapon_count;
ubyte rear_view_state;
ubyte rear_view_count;
ubyte automap_state;
ubyte automap_count;
```

D2 feature-flagged fields:

```c
ubyte toggle_bomb_count;
ubyte afterburner_state;
ubyte headlight_count;
ubyte energy_to_shield_state;
```

Do not serialize `key_*`, `btn_*`, `joy_axis`, `raw_joy_axis`, `mouse_axis`, or
`raw_mouse_axis`. Those are physical-device and mapping layers.

### Android-First Placement

This work should be Android-first. Put the fixture formats, sparse JSON readers
and writers, replay/session orchestration, launcher settings, result comparison,
and regression scripts under `android/`.

Keep `d1/` and `d2/` limited to small generic hook surfaces such as:

- RNG seed accessors and optional RNG diagnostics.
- A narrow tap point to capture or inject the portable `Controls` subset.
- Small result-counter exports needed by the serializer.

Do not spread replay file parsing, sparse JSON policy, or regression-comparison
logic across both engine trees.

### Start State

For fresh level regression tests, prefer a deterministic level-load start:
mission, level number, difficulty, callsign, player inventory defaults, and a
fixed initial RNG seed. This is smallest and easiest to reason about.

For mid-level recordings, prefer a save/checkpoint snapshot as the simulation
authority. A single classic demo frame is not enough by itself because it can
miss hidden simulation state such as AI locals, trigger timers, materialization
centers, object signatures, and subsystem timers. A classic first frame is still
useful as a preview/provenance chunk and as a start-state sanity check, but the
simulation should resume from a real save/checkpoint when the start is not a
fresh level.

Recommended start modes:

- `new_level`: load mission and level from metadata, seed RNG, start recording
  at frame 0.
- `save_checkpoint`: restore a compact save/checkpoint blob, seed or restore RNG,
  then start feeding inputs.
- `classic_demo_preview`: optional classic demo start frame for visual comparison
  and provenance, not the primary simulation source.

Coop save/checkpoint starts are a later TODO. Initial coop replay work should
use fresh synchronized level starts rather than coop save restore.

### Timing And Accelerated Replay

Replay must use recorded `FrameTime` rather than wall-clock elapsed time. The
accelerated runner should advance one recorded frame per loop iteration as fast
as possible.

Headless replay should be introduced in layers:

1. Android accelerated replay with rendering still present but uncapped where
   possible.
2. Desktop no-render mode that skips draw and flip paths while still running
   simulation and final-state serialization.
3. CI/headless clients for coop relay tests.

The first useful replay harness does not need to be fully headless if it can run
with recorded `FrameTime` and produce a final result file. Full no-window/no-GPU
support can come after the deterministic input loop exists.

### Final State And Regression Annotations

At replay completion, write a result file from C/C++ source-of-truth state, not
from Kotlin copies of gameplay rules. Start by extending or sharing the
introspection serializer so both tests and manual debugging use the same fields.

Generated final result files should be compact and sparse JSON with stable field
ordering, not verbose object dumps.

The result should include at least:

- Game: D1/D2, mission, level, difficulty, Game_mode, elapsed frames, GameTime64.
- Player per slot: callsign, connected state, energy, shields, score, lives,
  hostages, keys, weapons, ammo, deaths/kills in multiplayer.
- Position per slot: segment, position, forward vector, object signature if
  useful.
- Level counters: robots alive, robots killed, robots active, robots by type,
  powerups remaining, hostages remaining, doors opened, walls destroyed, triggers
  activated, matcens triggered, control center destroyed, endlevel complete.
- Determinism: start checkpoint hash, input stream hash, RNG mode, final world
  summary hash.

Result comparison should start strict for fixed-point fields on the same build.
When networked coop enters the suite, compare high-level stats strictly and allow
separate tolerances for known transport-order-sensitive fields only if needed.

## RNG Strategy And Alternatives

### Alternative A: Fixed Initial Seed Only

Record one initial seed and require every replay to call `d_rand()` in exactly
the same order.

Pros:

- Smallest file format.
- Most sensitive to changes in game logic.
- Closest to ordinary deterministic simulation.

Cons:

- One added, removed, or reordered RNG call can cascade for the rest of the run.
- Failures may be noisy and hard to localize.
- Requires every wall-clock `d_srand((fix)timer_query())` path to be bypassed or
  made deterministic during replay.

### Alternative B: Per-Frame Seed Checkpoints

Record the RNG seed at each frame boundary and restore it before replaying that
frame.

Pros:

- Light touch to gameplay code.
- Prevents one frame's RNG call-count change from cascading into every later
  frame.
- Easy to inspect in a test fixture.
- Good first implementation target.

Cons:

- Differences within the current frame still happen if RNG call order changes.
- Can hide cross-frame consequences of extra RNG calls, so tests should still
  report RNG call-count mismatches as diagnostics when available.

### Alternative C: Per-Call RNG Output Log

Record every `d_rand()` output and replay from the output log.

Pros:

- Can reproduce a run even when initial seeding or platform RNG differs.
- Excellent diagnostic mode for proving whether a divergence is RNG-driven.

Cons:

- Larger fixtures.
- A new or missing RNG call shifts the stream unless calls are also counted or
  labeled.
- More invasive than seed accessors.
- Too strong as the default mode because it can mask meaningful RNG-order
  regressions unless mismatch reporting is strict.

### Alternative D: Scoped RNG Streams

Split RNG into named subsystem streams such as AI, weapons, visuals, and music.

Pros:

- Long-term clean determinism model.
- Reduces unrelated subsystem coupling.

Cons:

- Large gameplay-touching refactor.
- Higher risk to gameplay feel.
- Not appropriate as the first tranche.

### Recommended RNG Path

Use Alternative B first, with the small foundation needed for Alternative A and
C diagnostics:

1. Add `d_rand_get_seed()` and `d_rand_set_seed()` to both D1 and D2.
2. Keep the current LCG formula for normal builds.
3. In deterministic replay/test builds, force or assert the LCG path rather than
   libc `rand()`.
4. Add optional counters around `d_rand()` in deterministic mode:
   `d_rand_get_call_count()`, `d_rand_reset_call_count()`, and per-frame expected
   call counts if useful.
5. Record per-frame seed checkpoints in input demos.
6. Later add per-call output logging as a debug mode, not the default fixture
   mode.

This is the safest first phase because it exposes state without changing the
normal RNG sequence or replacing gameplay randomness.

## File And Bundle Strategy

### JSON Style Rules

- Hand-authored fixture metadata can stay JSON5. Generated control streams, RNG
  streams, and final results should be strict JSON or JSONL so they stay smaller
  and simpler to parse.
- Use a stable schema order for generated keys. Do not alphabetize on write if
  it would scramble meaningful grouping.
- Use short documented keys in generated files.
- Omit fields whose value is the schema default: `0`, `false`, empty string,
  empty array, empty object, or absent optional section.
- Use sparse objects keyed by index or id for ammo, kills-by-type, per-player
  summaries, and similar high-zero data. Do not emit long zero-filled arrays.
- Use JSONL for per-frame streams so each replay record is diffable by line.
- Final result files should be compact but still pretty-printed, with one
  logical field per line. Do not fully minify committed baselines because that
  hurts diffs more than it saves bytes.

### Short-Term Fixture Format

Use an unpacked, git-friendly directory for regression fixtures while the system
is evolving:

```text
android/test_fixtures/input_demos/d2-level1-basic/
    demo.json5
    inputs.p0.jsonl
    rng.p0.jsonl
    start.save
    classic-preview.dem
    result.json
```

`demo.json5` is metadata:

```json5
{
    version: 1,
    game: "d2",
    mission: "d2",
    level: 1,
    difficulty: 2,
    start_mode: "new_level", // or "save_checkpoint"
    rng_mode: "per_frame_seed",
    frame_count: 6000,
    streams: [
        { player: 0, input: "inputs.p0.jsonl", rng: "rng.p0.jsonl" },
    ],
    start_save: "start.save", // omit for new_level
    classic_preview: "classic-preview.dem",
    result: "result.json",
}
```

### Sparse Per-Frame Control JSONL

Each `inputs.pN.jsonl` line is a sparse replay record. The replay is still
per-frame, but the file does not need to repeat unchanged values.

Record keys:

- `f`: starting frame index for this record.
- `n`: run length in frames, default `1`.
- `ft`: `FrameTime` for this run. If omitted, reuse the previous `ft`.
- `s`: held-state updates. These values persist until a later record changes
  them. Use explicit `0` to release a held value.
- `p`: one-frame pulse or count updates applied only on frame `f`.

Suggested `s` keys:

- `p`, `h`, `b`, `f`, `x`, `z` for pitch, heading, bank, forward, sideways,
  and vertical control-time values.
- `f1s`, `f2s`, `rvs`, `ams` for held fire/rear-view/automap states.
- `ab`, `es` for D2 afterburner and energy-to-shield state.

Suggested `p` keys:

- `f1`, `f2`, `fl`, `db`, `cp`, `cs`, `sw`, `rv`, `am` for fire/count pulses.
- `tb`, `hl` for D2 toggle-bomb and headlight counts.

Sample sparse control stream:

```json
{"f":0,"n":48,"ft":3276,"s":{"f":3276}}
{"f":48,"p":{"f1":1}}
{"f":49,"n":12,"s":{"h":910,"ab":1}}
{"f":61,"s":{"f":0,"h":0,"ab":0}}
```

Semantics:

- The replay runtime carries a held-state cache forward across frames.
- `s` only records what changes.
- `p` always defaults to zero on frames where it is omitted.
- `n` compresses repeated frames with no input changes.

This keeps control streams readable, sparse, and git-diff friendly while still
being exact enough to drive per-frame replay.

### Sparse Final Result JSON

`result.json` should use short stable keys and omit defaults. It should be easy
to diff, so emit it pretty-printed in a fixed order rather than as one minified
line.

Sample:

```json
{
  "v": 1,
  "g": "d2",
  "m": "d2",
  "l": 1,
  "d": 2,
  "fr": 6000,
  "gt": 120000000,
  "p0": {
    "e": 67,
    "s": 42,
    "sc": 12500,
    "li": 3,
    "ll": 1,
    "pw": 0,
    "sw": 0,
    "pa": { "1": 200 },
    "sa": { "0": 4 }
  },
  "pos": {
    "sg": 142,
    "x": 12345678,
    "y": -8765432,
    "z": 3456789
  },
  "lv": {
    "ra": 23,
    "rk": 8,
    "hr": 2,
    "pr": 15
  },
  "env": {
    "do": 5,
    "wd": 2,
    "tr": 3,
    "mc": 1
  },
  "rbt": {
    "0": 3,
    "5": 2,
    "12": 3
  }
}
```

For generated checkpoint/result files:

- Omit false flags and zero counters unless they are semantically important.
- Use sparse objects instead of long arrays for ammo, robot kills, and similar
  mostly-zero collections.
- Add multiplayer sections only when the fixture is multiplayer.
- Keep key names documented once in the schema section rather than spelling out
  long names in every generated baseline.

### Long-Term Hybrid Container

After the format stabilizes, add a single `.dxdemo` or `.dxidemo` chunked file
that can contain both classic and deterministic data:

- `META`: JSON metadata.
- `SAVE`: start checkpoint or save blob.
- `INPT`: one or more player input streams.
- `RNGC`: RNG seed checkpoints and optional call counts.
- `CDEM`: optional classic demo data. For compact fixtures, this can retain only
  first and last classic states or sparse checkpoints rather than every frame.
- `RSLT`: expected final result annotations.

This supports the idea of recording both demo types together while still letting
the repo keep an unpacked, diff-friendly mirror for tests.

Even after a packed container exists, keep the unpacked sparse JSON mirror as
the committed regression-fixture form so git diffs stay useful.

## Recording Flow

### Single-Player Live Recording

1. Player enables deterministic input demo recording.
2. Engine chooses `new_level` or `save_checkpoint` start mode.
3. Engine records metadata, start checkpoint hash, initial RNG seed, and optional
   classic preview start frame.
4. Each game frame:
   - Record `FrameTime`.
   - Record portable `Controls` subset.
   - Record RNG seed at the frame boundary.
   - Optionally record RNG call count for diagnostics.
5. On level end, death, stop-record, or explicit test marker:
   - Write final result annotations.
   - Optionally write a classic final state or final classic frame.

### Converting Existing Classic Demos

Classic `.dem` files can still bootstrap input tests, but they should not be the
primary path for new fixtures.

Conversion is lossy because classic demos do not record controls. The converter
can derive approximate controls from frame timing, player orientation, velocity,
and thrust records, then replay them through the engine and refine. That is a
useful import path for old demos, not a requirement for the first live-recorder
milestone.

Recommended order:

1. Build live input recording first.
2. Build deterministic replay and result assertions.
3. Add classic `.dem` conversion only after live recordings can round-trip.

## Replay Flow

### Single-Player Replay

1. Load metadata and validate asset requirements.
2. Restore `new_level` or `save_checkpoint` start state.
3. Set deterministic RNG mode and initial seed.
4. For each frame:
   - Set `FrameTime` from the record.
   - Restore frame RNG seed if `rng_mode` is `per_frame_seed`.
   - Fill `Controls` from the input stream.
   - Run one normal game frame.
   - Track optional intermediate assertions.
5. Serialize final result annotations.
6. Compare to the fixture baseline `result.json`.

### Accelerated And Headless Replay

The replay loop should decouple simulation time from wall clock. In accelerated
mode, it should execute recorded frames as fast as possible while preserving the
recorded `FrameTime` values.

Headless should mean no interactive input, no required visible window, no GPU
dependency, and no audio dependency. If complete headless is too large for the
first implementation, add a no-render accelerated mode first and leave full
desktop CI headless as a later phase.

## Coop And Relay Recording

The existing coop network sends player state, not inputs. For deterministic coop
fixtures, add a debug/test-only input-demo coordination layer rather than trying
to infer peer controls from received position packets.

Coop save/checkpoint start support is explicitly a later TODO. The first coop
tranche should use synchronized fresh-level starts only.

### Coop Recording Protocol

1. Host starts deterministic demo recording and creates a recording session id.
2. Host sends a cue to peers with game id, mission, level, difficulty, start
   checkpoint hash, frame zero, RNG mode, and input schema version.
3. Each peer acknowledges readiness.
4. At the frame-zero barrier, every peer starts recording its own portable
   `Controls` stream and RNG checkpoints.
5. Peers either write their streams locally for export or send compact frame
   input records to the host in a new debug/test packet type.
6. At stop-record or level completion, each peer writes final result annotations.
7. Host bundles streams as `p0`, `p1`, and so on, plus multiplayer final stats.

### Coop Replay Tiers

Tier 1: Multi-stream local replay, no relay. Use this only if the engine gains a
test harness that can apply multiple player input streams in one simulation.
This would be best for pure engine determinism, but it is not how the current
coop runtime works.

Tier 2: Networked replay with relay server. Start N headless or accelerated
clients, connect through the relay server, load the same fixture, and have each
client feed its own stream. The test validates level completion and final stats
across all clients. This matches the real coop path and should be the main coop
integration test target.

Tier 3: Deterministic network schedule. If Tier 2 is too noisy, add a test-only
frame barrier or packet schedule capture so each client advances only when the
relay/host has delivered the expected frame inputs. This is larger and should
wait until single-player replay is stable.

## Advanced Tab And User-Facing Recording Mode

Add an Advanced tab setting with a small enum rather than a boolean:

- `classic`: record classic `.dem` files.
- `deterministic_input`: record input-demo fixtures.
- `hybrid`: record deterministic inputs plus classic preview/checkpoint data.

Before release, Android-specific launcher compatibility does not need migration
support. Store the setting in existing launcher/debug preferences and pass it to
native startup or the native recording toggle. Classic demo behavior should stay
unchanged when the setting is `classic`.

## Implementation Phases

### Phase 0: Plan And Source Study [DONE]

- Confirmed classic demos are state-based.
- Confirmed `control_info Controls` is the generic input surface.
- Confirmed RNG snapshot APIs are not implemented yet.
- Confirmed coop sends player state, not canonical controls.
- Confirmed introspection needs more result-summary fields.

### Phase 1: Safe RNG Foundation [NEXT]

Goal: expose and control RNG state without changing normal gameplay feel, while
keeping non-Android engine edits to tiny generic hook points.

Tasks:

- Add the smallest possible `d_rand_get_seed()` and `d_rand_set_seed()` hook
  points in `d1/include/maths.h`, `d2/include/maths.h`, and the matching
  `rand.c` files.
- Decide how deterministic builds handle `NO_WATCOM_RAND`: either force the LCG
  when deterministic replay is enabled, or make deterministic replay fail fast if
  libc `rand()` is compiled in.
- Add optional deterministic-mode RNG call counters in both D1 and D2.
- Add a tiny host or unit test that seeds, draws a few values, snapshots/restores
  the seed, and proves the sequence resumes exactly.
- Keep replay orchestration, JSON trace writing, and Android test harness code in
  `android/`.
- Run the normal Windows/CMake build path after the D1/D2 edits.

Success criteria:

- Existing gameplay RNG sequence is unchanged when deterministic mode is off.
- Tests can read and restore the private RNG seed.
- Both D1 and D2 build.

### Phase 2: Input Record Schema And Shared Helpers

Goal: define an Android-first portable record layout that can later serve both
D1 and D2 without duplicating format logic in each tree.

Tasks:

- Add a shared input-demo schema under `android/` or a small D1/D2 duplicated C
  header if needed for engine integration.
- Add conversion helpers between `control_info` and the serialized portable
  record.
- Explicitly omit raw input, bindings, and physical-device fields.
- Add sparse JSON and JSONL reader/writer helpers under `android/`.
- Add round-trip tests for the Android-side schema and serializer.

Success criteria:

- A `control_info` subset can serialize and deserialize without involving
  keyboard, mouse, joystick, or touch mappings.

### Phase 3: Live Single-Player Recorder

Goal: record fresh deterministic input streams during normal play.

Tasks:

- Hook after controls are populated and before `read_flying_controls()` consumes
  them.
- Record `FrameTime`, portable controls, per-frame RNG seed, and optional RNG
  call count.
- Record start metadata and start checkpoint hash.
- Write an unpacked fixture under `temp/` first, then move stable fixtures to
  `android/test_fixtures/input_demos/`.
- Emit sparse JSONL control streams and sparse JSON final results rather than
  verbose dumps.

Success criteria:

- A short Android D2 level-start recording creates metadata, sparse input and
  RNG streams, and sparse final result annotations.

### Phase 4: Single-Player Replay Harness

Goal: feed recorded inputs through the normal engine.

Tasks:

- Add a debug/test command-line mode such as `-inputdemo-replay <demo.json5>`.
- Restore `new_level` or `save_checkpoint` start state.
- Drive `FrameTime`, `Controls`, and RNG per frame.
- Serialize final result annotations.
- Compare actual result with expected result.

Success criteria:

- A short Android D2 recording replays twice with identical final annotations.
- D1 follow-up can wait until the Android-first path and hook surfaces stabilize.

### Phase 5: Final Result Serializer

Goal: make final-state checks useful for regression tests.

Tasks:

- Extend shared introspection/result code with robot, powerup, hostage, door,
  wall, trigger, matcen, endlevel, and multiplayer summary fields.
- Keep gameplay constants and detailed file-format knowledge in C/C++ where
  possible.
- Emit sparse stable-key `result.json` files suitable for git review.
- Add a fixture-level comparison script that reports field-level differences.

Success criteria:

- Result JSON can explain a failure without parsing screenshots or log text.

### Phase 6: Accelerated Android Replay And Later No-Render Mode

Goal: make deterministic demos practical as regression tests.

Tasks:

- Run replay with recorded `FrameTime` independent of wall clock.
- Get Android accelerated replay working before any desktop headless expansion.
- Add no-render or headless mode in the smallest viable layer.
- Disable or stub audio work that blocks accelerated tests.
- Add a test runner wrapper that fails fast on timeout.

Success criteria:

- A two-minute input demo can replay substantially faster than real time.

### Phase 7: Classic Demo Import And Hybrid Container

Goal: use existing `.dem` files as bootstrap material and support combined demo
files.

Tasks:

- Build `.dem` to input-demo conversion after live record/replay is stable.
- Derive controls from classic demo object records only as an import path.
- Add optional classic preview/checkpoint chunks.
- Add packed `.dxdemo` container export once the unpacked fixture format settles.

Success criteria:

- Existing classic demos can produce approximate input fixtures, and new live
  deterministic recordings can be bundled with classic preview states.

### Phase 8: Coop Recording And Replay

Goal: record and replay cooperative input demos with relay-server coverage.

Tasks:

- Add a host cue and peer acknowledgement protocol for deterministic recording.
- Record per-peer input and RNG streams.
- Bundle multiplayer expected results.
- Start relay server and N accelerated/headless clients from a test script.
- Complete a small coop level flow and verify final per-player stats, robots,
  and level outcome.
- Keep coop save/checkpoint starts as an explicit later TODO.

Success criteria:

- A relay-backed coop fixture can complete a level and produce stable final
  stats across repeated runs.

## First Work Item

Start with Phase 1. It is small, testable, and unblocks every later phase. The
patch should touch both D1 and D2 RNG files and headers, add a focused test, and
prove that seed restore reproduces the same `d_rand()` sequence. Do not change
normal seeding call sites yet except to document which ones replay mode will
need to override later. Keep the rest of the replay stack, sparse JSON traces,
and test orchestration under `android/`.

## Open Questions

- Should the initial live recorder write unpacked JSONL only, or write binary
  streams plus a JSONL export for git review?
- Which existing save/checkpoint path is safest to reuse for `save_checkpoint`
  starts without pulling launcher logic into C?
- Should deterministic replay be Android-debug-only at first, or should the host
  desktop build get the harness immediately for faster iteration?
- For coop, should peer streams be transferred over the game protocol, an
  automation side channel, or exported independently from each device and merged
  by the host test script?