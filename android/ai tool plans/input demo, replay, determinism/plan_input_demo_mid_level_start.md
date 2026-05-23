# Input Demo Mid-Level Start Plan

## Goal

Enable deterministic `.dximdemo` recordings that start after level load by
embedding the actual save file bytes produced by the existing D1 and D2
savegame system at record start, restoring those bytes before frame 0 replay,
and keeping the current `new_level` path intact.

Keep `.dximdemo` single-file, keep launcher logic out of save internals, and
prefer narrow reuse of the existing engine save and restore code over new world
serialization.

## Follow-Up Fixes 2026-04-26

- [x] Broaden mid-level quick-record detection beyond `ThisLevelTime != 0`
- [x] Preserve quick-record mission and level metadata until auto-name build
- [x] Normalize replay checkpoint temp paths to the current runtime players-dir layout
- [x] Skip eager mission loading for `save_checkpoint` replay bootstrap
- [x] Guard Android `start_time()` underflow during replay startup

Notes:

- A real Android quick-toggle can happen after the player has already moved but
  before the level-time gate alone proves it, so recorder start now also treats
  a changed player spawn pose as mid-level and captures a checkpoint
- The quick-stop path used to clear cached mission and level metadata before it
  built the auto-generated filename, which produced names like `level0`; the
  name is now built first and the cached metadata is cleared immediately after
- Replay startup no longer trusts the embedded checkpoint path verbatim. It now
  strips any recorded directory component and writes the temp restore file using
  the current runtime `GameArg.SysUsePlayersDir` layout, so a checkpoint demo is
  portable across Android and desktop players-dir configurations
- Checkpoint replay no longer pre-loads the mission in `inferno.c` before calling
  `state_restore_all_sub()`. Normal save restore already loads the mission from
  the save file, and the extra preload left the menu-side asset state in a bad
  place during Android startup for mid-level demo launches
- The remaining Android crash still reduced to `Assert(time_paused >= 0)` inside
  `start_time()`. For Android builds, unmatched `start_time()` calls during demo
  startup are now logged to breadcrumbs and treated as a no-op instead of aborting
  the process, because the extra unpause appears to be benign compared to killing
  the replay before it can proceed

## Decision

Use the existing save and restore system as the checkpoint authority.

That means:

- capture the raw output of `state_save_all_sub()`
- embed that raw save blob inside the `.dximdemo`
- restore it later through `state_restore_all_sub()`
- treat the blob as opaque bytes in shared input-demo code

Do not invent a second serializer for world state in the input-demo layer.

The only extra checkpoint metadata outside the raw save blob should be replay
support fields that the current save system intentionally does not preserve,
such as absolute start-time bookkeeping and a few transient scheduler values.

## Key Findings

- The schema and fixture metadata already reserve `start_mode:"save_checkpoint"`
  and `start_save`
- The current text parser only accepts `header`, `frame`, and `result` records,
  so there is still no place to store a checkpoint payload
- Replay bootstrap in both `d1/main/inferno.c` and `d2/main/inferno.c` rejects
  every start mode except `new_level`
- D1 and D2 already have direct filename-based save and restore helpers:
  - D1: `state_save_all_sub(char *filename, char *desc)` and
    `state_restore_all_sub(char *filename)`
  - D2: `state_save_all_sub(char *filename, char *desc)` and
    `state_restore_all_sub(char *filename, int secret_restore)`
- The existing save format is already the real engine state snapshot. In D2 it
  saves mission, level, player state, objects, walls, exploding walls, doors,
  cloaking walls, triggers, tmap side data, fuel and matcen state, reactor
  state, AI state, automap state, markers, palette and flash state, and the
  Android coop metadata trailer
- The save format is identified by the on-disk `DGSS` signature. This is the
  payload to embed as-is inside `save_checkpoint` demos
- The restore path already loads mission and level, rebuilds objects, walls,
  AI state, timers, and re-shows `Game_wind`, so mid-level replay can stay a
  narrow bootstrap extension instead of a deep engine rewrite
- Save helpers use PhysFS logical paths. When `GameArg.SysUsePlayersDir` is on,
  temp checkpoint filenames must use a `Players/` prefix
- Save files currently include description and thumbnail data. The first
  functional tranche can reuse that path as-is and optimize size later only if
  needed
- Save files do not encode RNG state. The demo still needs its own per-frame RNG
  record stream, and replay still needs to apply frame 0 RNG from the demo
- The save path intentionally zero-bases `GameTime64` on write, and restore
  brings that zero-based value back. The current demo result comparison treats
  `gt` as exact final `GameTime64`, so checkpoint-backed replay needs an
  explicit `gt` comparison policy
- Restore currently resets some transient weapon scheduler globals rather than
  restoring them from file:
  - `Auto_fire_fusion_cannon_time = 0`
  - `Next_laser_fire_time = GameTime64`
  - `Next_missile_fire_time = GameTime64`
  - `Last_laser_fired_time = GameTime64`
- Weapon objects do not preserve `creation_framecount` through the save shim.
  That is a concrete example of the growth opportunity in the save system if
  checkpoint-backed replay proves it matters
- The object and player save comments already acknowledge the right long-term
  pattern: when a field becomes restore-relevant, bump save coverage instead of
  recreating the state somewhere else
- The current replay loader also needs the separate held-state explicit-zero fix
  for keys like `s.f1s:0`. Treat that as an adjacent prerequisite, not the
  checkpoint design itself

## Why The Header Is Not Enough

Mid-level recordings cannot be reconstructed from the current header plus input
frames alone.

The header can describe mission, level, difficulty, and the intended replay
mode, but it does not carry the hidden simulation state that matters once the
level is already in progress:

- AI locals and behavior timers
- Materialization center state
- Trigger and door state
- Object signatures and destroyed/spawned objects
- Reactor countdown state
- Automap, palette, flash, and other gameplay timers

For mid-level starts, the simulation authority must be a real save/checkpoint
restore, not a guessed reconstruction from a first input frame.

## Why Use The Actual Save System

This is the right direction for both short-term delivery and long-term engine
quality.

- It preserves a single source of truth for world state instead of teaching the
  launcher or shared input-demo code how to understand savegame internals
- It keeps the first working implementation narrow: write a save, embed it,
  restore it
- It turns checkpoint-backed replay into a concrete audit of save coverage.
  When replay fails because a field was not serialized, that is evidence that
  the save system itself should improve
- Pre-release Android code does not need backwards compatibility, so using the
  current save format directly is acceptable for now

The pragmatic rule should be:

- world state belongs in the save blob
- per-frame deterministic replay state still belongs in the demo stream
- only a very small set of replay-only extras should live beside the save blob
  until the save system absorbs them or proves they do not matter

## Recommended File Shape

Keep `.dximdemo` as newline-delimited JSON and extend the record order to:

1. `header`
2. optional `checkpoint` record when `start_mode == "save_checkpoint"`
3. `frame` records
4. `result`

### Header

Keep the existing metadata keys.

- `start_mode` becomes either `new_level` or `save_checkpoint`
- `start_save` becomes the embedded logical checkpoint name, not a host OS path

### Checkpoint Record

Use one record with one base64 payload line, not chunk records.

Example:

```json
{"type":"checkpoint","format":"dgss","encoding":"base64","size":123456,"sha256":"...","save_name":"inputdemo_start.dgss","start_gt":124125,"next_laser_fire_delta":0,"next_missile_fire_delta":0,"last_laser_fired_delta":0,"auto_fire_fusion_delta":0,"data":"..."}
```

Rules:

- Present only for `save_checkpoint`
- Must appear immediately after the header and before the first `frame`
- `data` is the exact raw output bytes of `state_save_all_sub()` with no custom
  reserialization in the input-demo layer
- `size` and `sha256` validate the decoded save bytes before restore
- `save_name` is the temp PhysFS logical filename used when restoring
- `start_gt` is the absolute `GameTime64` at checkpoint capture time
- `*_delta` fields are replay-only values stored relative to capture-time
  `GameTime64`, so they can be reapplied after the zero-based restore
- `new_level` demos reject checkpoint records

If more state is found to be missing, prefer improving the save system itself.
Do not start mirroring object graphs or AI internals in separate checkpoint JSON
fields.

## What The Save System Already Gives Us

The current save path already covers most of what a mid-level replay needs:

- mission and level selection
- player inventory and stats
- full object list and object links
- wall, door, cloaking wall, and trigger state
- tmap side state and changed light state
- fuel centers, matcens, reactor state, and countdown state
- AI state
- automap and marker state
- flash and palette state

That is why the raw save blob should be the checkpoint authority.

## What The Save System Does Not Give Us For Free

These are the concrete gaps visible in code today:

- RNG state is not part of the save file. Replay must still use the demo's RNG
  frames
- `GameTime64` is intentionally written as zero in save files, so checkpoint
  replay cannot compare absolute final `gt` the same way `new_level` replay does
- Restore resets weapon-fire scheduler globals instead of restoring them from
  disk
- Weapon object `creation_framecount` is not preserved through the save shim,
  which may matter for active tracking weapons around the checkpoint boundary
- Future object or player fields can still be omitted until the save version is
  expanded

The checkpoint plan should treat these as explicit audit items.

## Recorder Flow

### Start Mode Selection

At record start on the game thread:

- If the run starts on the first frame of a fresh level, keep `start_mode` as
  `new_level`
- If the player starts recording after the level is already in progress, switch
  to `save_checkpoint`

Concrete rule for the first tranche:

- `ThisLevelTime == 0` and the current first-frame guard holds: `new_level`
- otherwise, if single-player and deterministic RNG mode is valid:
  `save_checkpoint`

### Checkpoint Capture

For `save_checkpoint`, capture a real temp save before
`newdemo_start_recording(1)`:

1. Build a deterministic temp PhysFS path such as `Players/inputdemo_start.dgss`
2. Call a narrow D1 or D2 helper that wraps `state_save_all_sub()` with a fixed
   description string and no UI
3. Read the raw save bytes back into the recorder session without parsing them
4. Capture replay-only extras at the same boundary: `start_gt = GameTime64`,
  `next_laser_fire_delta = Next_laser_fire_time - GameTime64`,
  `next_missile_fire_delta = Next_missile_fire_time - GameTime64`,
  `last_laser_fired_delta = Last_laser_fired_time - GameTime64`, and
  `auto_fire_fusion_delta = Auto_fire_fusion_cannon_time - GameTime64`
5. Fill header metadata: `start_mode = "save_checkpoint"` and
  `start_save = "inputdemo_start.dgss"`
6. Delete the temp save once the bytes are safely buffered in memory

The save blob is the checkpoint. The extra fields exist only because the
current save system does not preserve those replay-relevant globals yet.

### Flush

When the demo is written:

1. write `header`
2. write the single `checkpoint` record if present
3. write `frame` records
4. write `result`

## Replay Flow

### Loader

Extend the shared loader so `input_demo_file` and `input_demo_replay_session`
retain:

- the raw checkpoint bytes
- `save_name`
- `start_gt`
- the replay-only scheduler deltas

### Bootstrap

In D1 and D2 startup replay handling:

- `new_level`: keep the current flow
- `save_checkpoint`:
  1. decode the base64 save blob
  2. verify `size` and `sha256`
  3. write the decoded bytes to a temp PhysFS logical path under the writable
     directory, using `Players/` when required
  4. call a narrow restore helper:
     - D1: wrap `state_restore_all_sub(temp_path)`
     - D2: wrap `state_restore_all_sub(temp_path, 0)`
  5. reapply the replay-only globals the current save system drops:
     - `Next_laser_fire_time = GameTime64 + next_laser_fire_delta`
     - `Next_missile_fire_time = GameTime64 + next_missile_fire_delta`
     - `Last_laser_fired_time = GameTime64 + last_laser_fired_delta`
     - `Auto_fire_fusion_cannon_time = GameTime64 + auto_fire_fusion_delta`
  6. validate that restored mission, level, and difficulty match the demo
     metadata
  7. leave the replay session loaded so the normal frame loop can apply frame 0
     RNG state and controls

The key split is:

- save restore establishes world state
- replay frame 0 applies deterministic RNG and control state

### Result Comparison Policy

`save_checkpoint` replay cannot use the existing absolute `gt` comparison as-is
because the save system deliberately zero-bases `GameTime64`.

Recommended first-tranche policy:

- keep recorded result `gt` as the original absolute final `GameTime64` for
  diagnostics
- store `start_gt` in the checkpoint record
- for `save_checkpoint` comparison, compare relative final game time:
  - expected relative `gt = expected.gt - start_gt`
  - actual relative `gt = actual.gt`

This keeps the current engine restore semantics intact and avoids trying to
rebase every timer in the live engine after restore.

### Cleanup

- Delete the temp extracted checkpoint on replay completion or unload
- Delete it on bootstrap failure before returning to menu
- Replace the current Android-only `start_time()` underflow guard with a traced,
  reasoned fix for the unmatched pause or unpause path. The guard is a temp
  crash-stop, not the desired final behavior
- Add checkpoint payload compression for `save_checkpoint` demos before the
  cleanup tranche is considered done. The embedded `DGSS` payload is currently
  raw base64 and wastes substantial space; evaluate a pinned, easy-to-include
  option such as zlib and keep checksum validation on the decompressed bytes

## Minimal Code Shape

### Shared Helpers

- `input_demo_fixture.h/cpp`
  - add single-record checkpoint parsing and writing
  - validate that `save_checkpoint` requires a checkpoint record
  - keep the raw `DGSS` payload opaque
- `input_demo_replay.h/cpp`
  - store checkpoint blob and replay-only extras
  - add a helper that materializes the payload into PhysFS
  - adjust result comparison for relative `gt` when `start_mode` is
    `save_checkpoint`
- `input_demo_recorder.cpp`
  - let recorder session optionally carry the raw checkpoint blob and replay-only
    extras

### D1 And D2 Game Code

Add one small wrapper per game tree instead of threading replay-specific logic
through the generic save UI path.

Recommended wrappers:

- `input_demo_capture_checkpoint(const char *path)`
- `input_demo_restore_checkpoint(const char *path)`

These hide the D1 versus D2 `state_restore_all_sub` signature difference and
keep replay bootstrap simple.

### Save-System Audit Follow-Up

Treat checkpoint replay failures as save-format audit data.

The first likely candidates to move into the save system itself are:

- weapon-fire scheduler globals
- weapon `creation_framecount` if active projectiles prove nondeterministic
- any newly added object or player fields that affect replay after restore

That improves the engine instead of building more checkpoint-only patches around
it.

## Phases

### Phase 1: Shared Format Extension

Goal: make the single-file format capable of carrying the raw save blob

Status: completed

Tasks:

- Extend `input_demo_file` with optional checkpoint payload fields
- Add a single `checkpoint` record parser and writer
- Validate that `save_checkpoint` requires exactly one checkpoint record before
  any frame records
- Keep `new_level` behavior unchanged

Validation:

- Added parser tests for `save_checkpoint` presence and ordering
- Added round-trip tests for single-record base64 checkpoint serialization
- D1 and D2 `test_input_demo_fixture` pass with the new checkpoint grammar

### Phase 2: Recorder-Side Checkpoint Capture

Goal: produce a valid mid-level `.dximdemo` from a live run using the actual
save system

Status: completed

Tasks:

- Add D1 and D2 checkpoint capture wrappers around `state_save_all_sub`
- Update quick-record start logic to choose `save_checkpoint` when starting
  after the level has already begun
- Capture raw save bytes plus replay-only extras into recorder session state
- Emit the raw save blob as the checkpoint record
- Keep the existing `new_level` path unchanged for first-frame starts

Validation:

- Shared recorder tests now cover `save_checkpoint` output, including raw blob
  base64 and sha256 emission
- `run-windows-build.ps1 -Target d1` passes with the D1 checkpoint capture hook
- `run-windows-build.ps1 -Target d2` passes with the D2 checkpoint capture hook
- Manual mid-level recording verification is still pending

### Phase 3: Replay Bootstrap For `save_checkpoint`

Goal: replay mid-level `.dximdemo` files through the existing engine loop

Status: in progress

Tasks:

- Extend D1 and D2 inferno startup handling to accept `save_checkpoint`
- Extract the embedded save blob to a temp PhysFS file
- Restore through the new per-game wrapper
- Reapply replay-only scheduler values the save system currently drops
- Compare relative `gt` for checkpoint-backed replays
- Keep frame replay, RNG application, actual result writing, and most result
  comparison logic unchanged after restore completes

Validation:

- Add a replay integration test that loads a known checkpoint-backed fixture and
  produces `.actual.json`
- Re-run existing `new_level` smoke tests unchanged

### Phase 4: Save-System Coverage Follow-Up

Goal: use checkpoint replay findings to improve the real save system

Possible follow-ups:

- move replay-only scheduler globals into save and restore if needed
- preserve weapon `creation_framecount` if active projectile tests show drift
- add a thumbnail-less or blank-thumbnail checkpoint save helper if payload size
  becomes a problem
- remove checkpoint-only extras once the save system itself carries them

This growth phase is the upside of using the actual save system as the
checkpoint authority.

## Validation Plan

Minimum validation for the full tranche:

- `run-windows-build.ps1 -Target d1`
- `run-windows-build.ps1 -Target d2`
- from `android/`, `./gradlew.bat :app:externalNativeBuildDebug --no-daemon`
- existing `android/tests/test_input_demo_runtime_smoke.ps1 -Game both`
- one new checkpoint-backed replay test for D1
- one new checkpoint-backed replay test for D2
- one targeted checkpoint replay test with firing or active projectile state at
  the capture boundary, to shake out scheduler and `creation_framecount` gaps
- one manual Android recording started mid-level and replayed successfully on
  host

## Risks And Constraints

- The save and restore code is the correct simulation authority for mid-level
  starts, but it is large and old. Keep new hooks narrow and per-game
- Using raw save blobs ties checkpoint behavior to save-format behavior. That is
  acceptable pre-release, and it is the point of the audit
- Save helpers use PhysFS logical names, so extracted temp checkpoints must be
  written where PhysFS can open them later
- The first tranche should not widen launcher logic. The launcher only lists,
  saves, shares, and installs `.dximdemo` files. C and C++ own checkpoint
  capture and restore behavior
- Current real recordings still need the explicit-zero held-state parser fix.
  That bug is separate from mid-level start design, but it should be fixed
  before trusting replay results from new recordings

## Recommendation

Yes, use the actual save file system as the checkpoint.

The right implementation is to embed the real `DGSS` save blob inside the
single `.dximdemo`, restore that blob through the existing savegame loader, and
let checkpoint-backed replay expose where save coverage should improve.

The only extra checkpoint metadata should be the small replay-support fields the
current save path demonstrably drops today, not a second attempt at world
serialization.