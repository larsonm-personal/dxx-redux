# Deterministic Input Demo Schema

## Purpose

This document defines the Android-first single-file schema for deterministic
input demo recording and replay.

The goal is gameplay regression testing. Demo files should be small enough to
commit, readable in diffs, deterministic across supported builds, and useful as
a safety net while D1, D2, Android, and launcher code keep evolving.

This is not a long-term public file format promise. It is the current internal
schema for Android-side and host-side regression work.

## File Shape

The canonical extension is `.dximdemo`. A `.dximdemo` is a newline-delimited
JSON file. It is not a zip file and it is not a directory wrapper.

Required record order:

1. Header record as the first non-empty line
2. One frame record per replay frame, ordered from frame `0` through
   `frame_count - 1`
3. Result trailer record as the final non-empty line

Example:

```jsonl
{"type":"header","version":1,"game":"d2","mission":"d2","level":1,"difficulty":2,"start_mode":"new_level","rng_mode":"lcg_state","frame_count":3}
{"type":"frame","f":0,"ft":3276,"input":{"s":{"f":44}},"rng":{"s":100}}
{"type":"frame","f":1,"input":{"p":{"f1":1}},"rng":{"s":100}}
{"type":"frame","f":2,"input":{"s":{"f":0}},"rng":{"s":102,"c":3}}
{"type":"result","result":{"v":1,"g":"d2","m":"d2","l":1,"d":2,"fr":3}}
```

## JSON Rules

- All records are strict JSON parsed with `nlohmann::json`
- Generated records use fixed key order
- Generated records use short documented keys where frame volume matters
- Omit default values wherever possible
- Use sparse objects instead of long zero-filled arrays
- Held-state fields persist until changed explicitly
- Pulse/count fields default to `0` on frames where they are not present
- A demo is complete only when the result trailer is present

## Header Record

Purpose: describe replay start conditions and the number of frame records that
must follow.

Key order:

1. `type`: literal string `"header"`
2. `version`: schema version integer, currently `1`
3. `game`: `"d1"` or `"d2"`
4. `mission`: mission filename or short mission id
5. `level`: signed level number
6. `difficulty`: integer difficulty level
7. `start_mode`: `"new_level"` or future `"save_checkpoint"`
8. `rng_mode`: `"lcg_state"`, `"libc_reseed"`, or future `"output_log"`
9. `frame_count`: total replay frames
10. `classic_preview`: optional provenance payload name
11. `start_save`: optional checkpoint name, required for checkpoint starts

Validation:

- `version` must be `1`
- `game` must be `d1` or `d2`
- `mission` must be non-empty
- `frame_count` must be positive
- `start_save` is omitted for `new_level`
- unknown keys fail validation until explicitly added

## Frame Records

Purpose: replay one engine frame with input and RNG state interleaved in the
same record.

Top-level frame key order:

1. `type`: literal string `"frame"`
2. `f`: frame index
3. `ft`: `FrameTime`, required on frame `0`, omitted to reuse the previous
   value
4. `input`: sparse held-state and pulse updates for this frame
5. `rng`: RNG replay state for this frame

Frame records are never run-length encoded. Every frame has exactly one frame
record and every frame record includes both `input` and `rng` objects.

### Input Object

Input keys:

- `s`: held-state updates that persist until changed again
- `p`: one-frame pulse or count updates applied only on frame `f`

Held-state keys under `s`:

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
- `ab`: D2-only `afterburner_state`
- `es`: D2-only `energy_to_shield_state`

Pulse keys under `p`:

- `f1`: `fire_primary_count`
- `f2`: `fire_secondary_count`
- `fl`: `fire_flare_count`
- `db`: `drop_bomb_count`
- `cp`: `cycle_primary_count`
- `cs`: `cycle_secondary_count`
- `sw`: `select_weapon_count`
- `rv`: `rear_view_count`
- `am`: `automap_count`
- `tb`: D2-only `toggle_bomb_count`
- `hl`: D2-only `headlight_count`

Validation:

- `f` values must be contiguous and match physical file order
- frame `0` must include `ft`
- `input` must be an object, even if empty
- unknown `input`, `s`, or `p` keys fail validation
- D1 demos reject D2-only control keys

### RNG Object

RNG keys:

- `s`: RNG seed or replayable state value for this frame
- `c`: optional RNG call-count diagnostic for this frame

Semantics:

- `s` is applied at the start of the replayed frame
- on the internal LCG path, `s` is the exact RNG state snapshot
- on libc `rand()` builds, `s` is a deterministic frame-start reseed value and
  must not be described as libc internal state
- `c` is diagnostic only

Validation:

- every frame record must include an `rng` object
- every `rng` object must include `s`
- RNG records are not run-length encoded in `.dximdemo`
- unknown RNG keys fail validation

## Result Trailer

Purpose: sparse final regression baseline embedded at the end of the same demo
file.

The trailer shape is:

```json
{"type":"result","result":{...}}
```

The nested `result` object uses the same compact result schema that the shared
result helper reads and writes as standalone JSON for actual replay output.
During replay, the engine writes an external actual result file beside the demo
using the path `<demo>.actual.json`, then compares that actual result to the
embedded trailer.

Top-level result keys:

- `v`: result schema version
- `g`: game id
- `m`: mission id
- `l`: level number
- `d`: difficulty
- `fr`: total frames replayed
- `gt`: final `GameTime64`, optional until a baseline needs it
- `p0`: player 0 summary
- `pos`: primary-player final position summary for single-player demos
- `lv`: level summary counters

Player summary keys under `p0`:

- `e`: energy
- `s`: shields
- `sc`: score
- `li`: lives
- `ll`: laser level
- `pw`: primary weapon
- `sw`: secondary weapon
- `fl`: player flags
- `pa`: sparse primary ammo object keyed by weapon index
- `sa`: sparse secondary ammo object keyed by weapon index
- `hk`: hostages on board or rescued summary

Position keys:

- `sg`: segment
- `x`: raw fixed-point x
- `y`: raw fixed-point y
- `z`: raw fixed-point z
- `fx`: forward vector x, optional
- `fy`: forward vector y, optional
- `fz`: forward vector z, optional

Level summary keys:

- `ra`: robots alive
- `rk`: robots killed
- `hr`: hostages remaining
- `pr`: powerups remaining
- `cc`: control center destroyed flag
- `el`: endlevel completed flag

Comparison rules:

- Compare all present expected keys exactly
- Treat omitted keys as schema defaults
- Compare sparse maps by key/value equality after implicit zero defaults
- Report mismatches using the short key plus a friendly expanded label from the
  result helper

## Versioning

- Header `version` is the demo file schema version
- Result `v` is the result trailer schema version
- Readers reject newer major versions they do not understand
- New optional keys may be added without changing the major version when omitted
  defaults preserve behavior

## Future Extensions

- checkpoint starts with a `start_save` header field and embedded or adjacent
  save payload design
- coop player records after the single-player file shape is stable
- optional classic preview data for human playback provenance
- optional intermediate checkpoints for long replay diagnostics
- optional per-call RNG output traces for debugging only
