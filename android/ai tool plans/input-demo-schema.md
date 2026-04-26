# Deterministic Input Demo Schema

## Purpose

This document defines the Android-first fixture schema for deterministic input
demo recording and replay.

The goal is gameplay regression testing. The fixtures should be:

- Small enough to commit to git comfortably
- Sparse and readable enough to review in diffs
- Stable enough to serve as a safety net for future D1/D2 code sharing and
  de-duplication

This is not a promise that the generated files are a long-term public file
format. It is the first stable schema for Android-side regression fixtures.

## Scope

The schema covers:

- `demo.json5` metadata
- `inputs.pN.jsonl` per-player sparse control streams
- `rng.pN.jsonl` per-player RNG checkpoints and diagnostics
- RNG stream entries are frame-start replay controls. On the current internal
  LCG path they may be true state snapshots. On libc `rand()` builds they are a
  deterministic reseed schedule, because libc does not expose current RNG state.
- `result.json` final sparse regression baseline

This schema is Android-first. The format readers, writers, validators, and test
orchestration should live under `android/`.

The engine trees should only expose the small hook points needed to support the
schema, such as RNG seed access, control capture/injection, and final counter
queries.

## File Set

Example fixture directory:

```text
android/test_fixtures/input_demos/d2-level1-basic/
    demo.json5
    inputs.p0.jsonl
    rng.p0.jsonl
    start.save
    classic-preview.dem
    result.json
```

Required files for a fresh-level single-player fixture:

- `demo.json5`
- one `inputs.p0.jsonl`
- one `rng.p0.jsonl`
- `result.json`

Optional files:

- `start.save` for `save_checkpoint` starts
- `classic-preview.dem` for hybrid provenance or sparse classic state
- additional `inputs.pN.jsonl` and `rng.pN.jsonl` files for coop later

Coop save/checkpoint starts are a later TODO. Early coop fixtures should use a
fresh synchronized level start.

## JSON Rules

- Hand-authored metadata uses JSON5.
- Generated streams and results use strict JSON or JSONL.
- Generated files use a fixed key order.
- Generated files use short documented keys.
- Omit default values wherever possible.
- Use sparse objects instead of long zero-filled arrays.
- Per-frame streams use one JSON object per line.
- Final result files stay pretty-printed for git review. Do not fully minify
  committed baselines.

Defaults:

- Numeric default: `0`
- Boolean default: `false`
- String default: empty or omitted
- Optional section default: omitted
- Pulse/count fields default to `0` on frames where they are not present
- Held-state fields persist until changed explicitly

## Versioning

- `demo.json5.version` is the fixture schema version
- First version is `1`
- Readers must reject newer major versions they do not understand
- New optional keys may be added without changing the major version if omitted
  defaults preserve behavior

## Metadata File: `demo.json5`

Purpose: describe the fixture layout and replay start conditions.

Example:

```json5
{
    version: 1,
    game: "d2",
    mission: "d2",
    level: 1,
    difficulty: 2,
    start_mode: "new_level",
  rng_mode: "lcg_state",
    frame_count: 6000,
    streams: [
        { player: 0, input: "inputs.p0.jsonl", rng: "rng.p0.jsonl" },
    ],
    classic_preview: "classic-preview.dem",
    result: "result.json",
}
```

Key definitions:

- `version`: schema version integer
- `game`: `"d1"` or `"d2"`
- `mission`: mission filename or short mission id
- `level`: signed level number
- `difficulty`: integer difficulty level
- `start_mode`: `"new_level"` or `"save_checkpoint"`
- `rng_mode`: `"lcg_state"`, `"libc_reseed"`, or future `"output_log"`
- `frame_count`: total replay frames
- `streams`: ordered array of per-player stream descriptors
- `classic_preview`: optional hybrid provenance payload
- `start_save`: optional checkpoint path, required when `start_mode` is
  `"save_checkpoint"`
- `result`: relative path to the final baseline file

Stream descriptor keys:

- `player`: player slot number
- `input`: relative path to `inputs.pN.jsonl`
- `rng`: relative path to `rng.pN.jsonl`

Metadata omission rules:

- Omit `start_save` when `start_mode` is `"new_level"`
- Omit `classic_preview` when the fixture is not hybrid
- Omit future coop-only fields for single-player fixtures

## Control Stream: `inputs.pN.jsonl`

Purpose: replay portable engine controls without storing physical bindings.

Each line is a sparse record. The replay still advances frame by frame, but the
file only stores changes and one-frame pulses.

Line object keys:

- `f`: starting frame index for this record
- `n`: run length in frames, default `1`
- `ft`: `FrameTime` for the run, omitted to reuse the previous `ft`
- `s`: held-state updates that persist until changed again
- `p`: one-frame pulse or count updates applied only on frame `f`

Example:

```json
{"f":0,"n":48,"ft":3276,"s":{"f":3276}}
{"f":48,"p":{"f1":1}}
{"f":49,"n":12,"s":{"h":910,"ab":1}}
{"f":61,"s":{"f":0,"h":0,"ab":0}}
```

Semantics:

- Replay maintains a held-state cache
- `s` updates replace only the keys they mention
- unmentioned held-state keys keep their previous value
- `p` keys are treated as zero on all other frames
- `n` compresses repeated frames with identical held state and no pulse changes

### Held-State Keys: `s`

Common keys:

- `p`: `pitch_time`
- `h`: `heading_time`
- `b`: `bank_time`
- `f`: `forward_thrust_time`
- `x`: `sideways_thrust_time`
- `z`: `vertical_thrust_time`
- `f1s`: `fire_primary_state`
- `f2s`: `fire_secondary_state`
- `rvs`: `rear_view_state`
- `ams`: `automap_state`

D2-only held-state keys:

- `ab`: `afterburner_state`
- `es`: `energy_to_shield_state`

Held-state value type:

- control-time fields are signed integer fixed-point values written as raw ints
- state fields are `0` or `1`

### Pulse Keys: `p`

Common keys:

- `f1`: `fire_primary_count`
- `f2`: `fire_secondary_count`
- `fl`: `fire_flare_count`
- `db`: `drop_bomb_count`
- `cp`: `cycle_primary_count`
- `cs`: `cycle_secondary_count`
- `sw`: `select_weapon_count`
- `rv`: `rear_view_count`
- `am`: `automap_count`

D2-only pulse keys:

- `tb`: `toggle_bomb_count`
- `hl`: `headlight_count`

Pulse value type:

- unsigned integer count for the current frame only

### Ordering Rules

Within each JSONL line, write keys in this order:

1. `f`
2. `n`
3. `ft`
4. `s`
5. `p`

Within `s`, write keys in this order:

1. `p`
2. `h`
3. `b`
4. `f`
5. `x`
6. `z`
7. `f1s`
8. `f2s`
9. `rvs`
10. `ams`
11. `ab`
12. `es`

Within `p`, write keys in this order:

1. `f1`
2. `f2`
3. `fl`
4. `db`
5. `cp`
6. `cs`
7. `sw`
8. `rv`
9. `am`
10. `tb`
11. `hl`

### Control Stream Validation

- First record must include `f: 0`
- `f` values must be strictly increasing
- `n` must be positive
- if `ft` is omitted, a previous `ft` must already exist
- unknown keys must fail validation
- D1 fixtures must reject D2-only keys

## RNG Stream: `rng.pN.jsonl`

Purpose: carry per-frame RNG replay controls and optional diagnostics.

This is separate from `inputs.pN.jsonl` so control diffs and RNG diffs stay easy
to read independently.

Line object keys:

- `f`: starting frame index
- `n`: run length in frames, default `1`
- `s`: RNG seed or replayable state value for the frame run
- `c`: optional RNG call count diagnostic for that frame run

Example:

```json
{"f":0,"n":48,"s":305419896}
{"f":48,"s":305420112,"c":3}
{"f":49,"n":12,"s":305421004}
```

Semantics:

- `s` is the value applied at the start of each replayed frame
- for the current internal LCG path, `s` can be the exact RNG state snapshot
- for libc `rand()` builds, `s` is a deterministic frame-start reseed value and
  must not be described as a snapshot of libc internal state
- `n` allows repeated seeds or call counts to compress cleanly
- `c` is diagnostic only and should not be required for first implementation

Validation:

- First record must include `f: 0`
- `f` values must be strictly increasing
- `n` must be positive
- every covered frame must have an RNG seed by expansion time

## Final Result File: `result.json`

Purpose: sparse git-friendly final regression baseline.

The file should use short stable keys, fixed ordering, and omitted defaults.
It should be pretty-printed to keep diffs readable.

Example:

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

### Top-Level Keys

- `v`: schema version
- `g`: game id
- `m`: mission id
- `l`: level number
- `d`: difficulty
- `fr`: total frames replayed
- `gt`: final `GameTime64`
- `p0`, `p1`, ...: per-player summaries keyed by slot
- `pos`: primary-player final position summary for single-player fixtures
- `lv`: level summary counters
- `env`: environment summary counters
- `rbt`: sparse robot-kill counts keyed by robot id
- `mp`: multiplayer summary, coop later
- `det`: optional determinism diagnostics such as hashes and RNG mode

### Player Summary Keys

- `e`: energy
- `s`: shields
- `sc`: score
- `li`: lives
- `ll`: laser level
- `pw`: primary weapon
- `sw`: secondary weapon
- `fl`: player flags, omit when `0`
- `pa`: sparse primary ammo object keyed by weapon index
- `sa`: sparse secondary ammo object keyed by weapon index
- `hk`: hostages on board or rescued summary, exact split can be added later
- `dk`: death count for multiplayer fixtures
- `kk`: kill count for multiplayer fixtures

### Position Keys

- `sg`: segment
- `x`: raw fixed-point x
- `y`: raw fixed-point y
- `z`: raw fixed-point z
- `fx`: forward vector x, optional
- `fy`: forward vector y, optional
- `fz`: forward vector z, optional

### Level Summary Keys

- `ra`: robots alive
- `rk`: robots killed
- `rr`: robots remaining, omit if redundant with `ra`
- `hr`: hostages remaining
- `pr`: powerups remaining
- `cc`: control center destroyed flag, omit when false
- `el`: endlevel completed flag, omit when false

### Environment Summary Keys

- `do`: doors opened
- `wd`: walls destroyed
- `tr`: triggers activated
- `mc`: matcens triggered

### Result Validation

- Required top-level keys for single-player baseline: `v`, `g`, `m`, `l`, `d`,
  `fr`, `gt`, one `pN`, and one of `pos` or a richer per-player position map
- Ammo and robot-count maps must be sparse objects, not arrays
- Unknown keys should fail validation until explicitly versioned in
  implementation

## Comparison Rules

- Compare all present keys exactly
- Treat omitted keys as schema defaults
- Compare sparse maps by key/value equality after applying implicit zero defaults
- Report mismatches using the short key plus a friendly expanded label from the
  schema table

## Future Extensions

- packed `.dxdemo` or `.dxidemo` container with chunked payloads
- coop `p1`, `p2`, and relay-session metadata
- coop final multiplayer stats under `mp`
- optional intermediate checkpoints
- optional classic sparse state snapshots
- optional per-call RNG output trace for debugging

## Open TODOs

- Decide whether `result.json` should keep `pos` as a single-player shortcut or
  move all position state under per-player records once coop lands
- Decide whether held-state `ft` omission should be allowed across fixture file
  boundaries or only within one JSONL stream
- Decide whether call-count diagnostics belong in `rng.pN.jsonl` or a separate
  debug-only stream
- Add a small implementation-facing validator under `android/` once Phase 2
  starts
