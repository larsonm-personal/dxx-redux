# RNG Sim vs Non-Sim Split

## Goal

Adopt the Doom / Doom 2 approach for the deterministic input-demo replay system: the
simulation RNG is reserved for state that affects ship, weapon, robot, level, and any
networked/saveable game state, and a separate parallel RNG handles non-simulation
consumers (HUD, palette, screen flash, ambient sound jitter, particle/decoration
effects, menu UI, kconfig calibration noise, console diagnostics). Replay only locks
down the simulation stream, exactly like Doom's `P_Random` vs `M_Random` split.

This plan is for a study/refactor, not for fixing one demo. The current `141502`
demo and the previously-investigated `101150` demo both show the same root cause
family: non-simulation RNG calls (palette drift, ambient sound jitter, possibly
menu/HUD effects) are sharing the single global LCG state with simulation calls,
so frame N's render/UI work shifts the state visible to frame N+1's AI/physics.

## Background and Constraints

- Current model (`d1/maths/rand.c`, `d2/maths/rand.c`): one global LCG state
  `d_rand_state`, one call counter `d_rand_call_count`, used by every consumer.
- A macro shim in `d1/include/maths.h` and `d2/include/maths.h` already routes every
  `d_rand()` call site through `d_rand_annotated(__FILE__, __func__, __LINE__)` for
  the rngtrace sidecar. That macro shim is the cheapest possible insertion point for
  a stream selector: every existing `d_rand()` caller can be re-targeted by changing
  one expansion, without touching call sites.
- Replay-side checkpoint, save state, and frame boundary code only persists the
  simulation state today (`d_rand_get_state`, `d_rand_set_state`,
  `d_rand_get_call_count`, `d_rand_reset_call_count`). The non-sim stream must not
  be persisted and must not be compared during replay.
- The `NO_WATCOM_RAND` build path falls back to libc `rand()` and reports replay mode
  `D_RAND_REPLAY_MODE_LIBC_RESEED`. The split design must keep that path working
  (only the LCG path is fully deterministic; the libc path stays best-effort).
- Repo policy: keep d1/ and d2/ diffs minimal for upstreaming. Prefer header/macro
  changes and a tiny new symbol set over per-call-site edits where possible.

## Design

### Two streams

- `D_RNG_SIM` (default): simulation stream. Used by AI, physics, weapons, level
  spawning, fuel cells, control center, gating, multiplayer-visible randomness,
  anything written to save games or sent over the network. This is the only
  stream replay restores at frame boundaries and the only one persisted in
  checkpoints / save states / rngtrace sidecars.
- `D_RNG_FX`: presentation/UI stream. Used by HUD, palette diminish, screen-flash
  jitter, ambient sound timer noise, kconfig calibration jitter, console flicker,
  menu dust effects, particle decorations that don't affect collision or scoring.
  Never persisted, never restored, never traced into the rngtrace sidecar.

Two streams is enough for the Doom analogy and keeps the audit small. We can split
further later (for example, a third stream for purely-cosmetic networked effects)
if some consumer turns out to need an intermediate guarantee, but the plan starts
with two.

### API shape

Add to `d1/maths/rand.c` and `d2/maths/rand.c` (kept duplicated, same as today):

```c
typedef enum { D_RNG_SIM = 0, D_RNG_FX = 1 } d_rng_stream;

int          d_rand_stream(d_rng_stream s);
void         d_srand_stream(d_rng_stream s, unsigned int seed);
int          d_rand_get_stream_state(d_rng_stream s, unsigned int *state);
int          d_rand_set_stream_state(d_rng_stream s, unsigned int state);
unsigned int d_rand_get_stream_call_count(d_rng_stream s);
void         d_rand_reset_stream_call_count(d_rng_stream s);
```

The existing `d_rand()`, `d_srand()`, `d_rand_get_state()`, `d_rand_set_state()`,
`d_rand_get_call_count()`, `d_rand_reset_call_count()` keep their signatures and
forward to the **sim** stream so replay/save code does not change.

Annotated wrappers gain a stream argument:

```c
int  d_rand_annotated(d_rng_stream s, const char *file, const char *func, int line);
void d_srand_annotated(d_rng_stream s, unsigned int seed, const char *file, ...);
```

### Macro routing (the key design choice)

In `d1/include/maths.h` and `d2/include/maths.h`, today's shim:

```c
#define d_rand()       d_rand_annotated(__FILE__, DXX_RAND_CALLER_FUNCTION, __LINE__)
#define d_srand(seed)  d_srand_annotated((seed), __FILE__, ...)
```

becomes:

```c
#ifndef DXX_RNG_DEFAULT_STREAM
#define DXX_RNG_DEFAULT_STREAM D_RNG_SIM   /* default: preserve today's behavior */
#endif

#define d_rand()        d_rand_annotated(DXX_RNG_DEFAULT_STREAM, __FILE__, __func__, __LINE__)
#define d_srand(seed)   d_srand_annotated(DXX_RNG_DEFAULT_STREAM, (seed), __FILE__, __func__, __LINE__)
#define d_rand_fx()     d_rand_annotated(D_RNG_FX, __FILE__, __func__, __LINE__)
#define d_srand_fx(seed) d_srand_annotated(D_RNG_FX, (seed), __FILE__, __func__, __LINE__)
```

This means:

1. Every existing `d_rand()` / `d_srand()` call keeps using the sim stream by
   default. Today's gameplay feel and existing demos are unchanged.
2. Migrating a non-sim consumer is a one-token edit: `d_rand()` -> `d_rand_fx()`
   at the call site, no `#include` change, no signature change.
3. A subsystem that wants to flip its translation unit's default can do
   `#define DXX_RNG_DEFAULT_STREAM D_RNG_FX` before including `maths.h`. Useful
   for files that are obviously all-FX (palette code, console renderer, kconfig
   visualizers).

The user's "default = non-sim, mark only sim" variant is the inverse and was
considered. The plan picks default-sim because:

- It preserves the diff against upstream d1/d2 to nearly zero until a specific
  consumer is intentionally moved.
- A missed migration is a desync today and remains a desync after the split, so
  default-sim never makes things *worse* than today; it just doesn't help that
  one consumer until it's tagged. Default-FX on a missed simulation site would
  silently break replays of currently-passing demos.
- The set of non-sim consumers is small and identifiable. The set of simulation
  consumers is the entire d1/d2 game.

### Replay, save, and trace integration

- Replay frame boundary code in `d2/main/game.c` (and the d1 mirror) keeps using
  `d_rand_get_state` / `d_rand_set_state` and `d_rand_*_call_count`. Those route
  to the sim stream by virtue of the API contract above.
- Checkpoint save/restore in `android/app/src/main/cpp/shared/input_demo_replay.cpp`
  saves the sim stream only. The FX stream is reseeded from a fixed value at
  replay start (for example `0xC0FFEE`) so replays remain reproducible bit-for-bit
  even if a developer is debugging FX-only code, but FX state is never compared.
- The rngtrace sidecar writer in `input_demo_rng_trace.{h,cpp}` only records sim
  stream calls. Adding the stream tag to `d_rand_annotated` lets the writer skip
  FX entries cheaply, which also reduces sidecar size.
- The host replay wrapper compare path in `android\tests\run_input_demo_replay.ps1`
  needs no change; it already only compares the sim-stream rngtrace.

### Multiplayer and save-game safety

- Anything that currently feeds into save games (`gamesave.c`), the demo recorder,
  or the network packet stream stays on the sim stream by default and must not be
  moved. We will add a short audit checklist (below) to confirm nothing in those
  call graphs is touched during the FX migration.

## Audit Targets (initial non-sim candidates)

The following consumer set is the intended initial migration target. Each is a
strong candidate for the FX stream because none of them feed AI decisions, weapon
behavior, level state, or networked/saveable state. This list is for the audit
phase, not a directive; each one needs a one-line review before the edit.

- `d{1,2}/main/game.c`: `diminish_palette_towards_normal` (`d_rand() < FrameTime*...`)
  and `add_computed_color` (`force_do || d_rand() > 4096`).
- `d{1,2}/main/game.c`: `Fusion_next_sound_time` jitter (sound scheduling only).
- `d{1,2}/main/game.c`: water/lava ambient sound branch in the level ambient block.
- `d{1,2}/main/kconfig.c`: any joystick calibration / kconfig "noise" visual.
- `d{1,2}/main/menu.c`, `newmenu.c`, `scores.c`: any decorative randomness.
- `d{1,2}/main/console.c` or equivalent: console blink/flicker if present.
- `d{1,2}/main/gauges.c` and `hud.c`: HUD decorative randomness.
- `d{1,2}/main/gamefont.c`, `text.c`: font scramble effects if present.
- `d{1,2}/main/titles.c`, `credits.c`: title/credits effects.
- `d{1,2}/main/fireball.c`, `fvi.c`, `fuelcen.c`: review carefully -- these mostly
  belong to sim, but a few "spark"-style cosmetic calls may be FX.
- `d{1,2}/main/lighting.c`: dynamic light flicker (FX) vs lighting decisions that
  feed visibility/AI (sim) -- needs case-by-case review.

Explicitly **not** to be migrated (these stay on the sim stream):

- All of `ai.c`, `ai2.c`, `aipath.c`, `boss.c`.
- `weapon.c`, `laser.c`, `multi.c`, `multibot.c`, anything writing to packet code.
- `physics.c`, `collide.c`, `object.c`, `render.c` collision-relevant paths.
- `gamesave.c`, `state.c`, `playsave.c`.
- `cntrlcen.c`, `endlevel.c`, `gameseq.c`.
- Anything called from the replay frame boundary write or the save game writer.

## Phased Work Plan

### Phase 1 -- Two-stream RNG core (no behavior change)

- [x] Extend `d{1,2}/maths/rand.c` with two parallel `d_rand_state[]` and
      `d_rand_call_count[]` arrays indexed by `d_rng_stream`.
- [x] Add the `*_stream` API and route the existing non-stream entry points to
      `D_RNG_SIM`.
- [x] Keep `NO_WATCOM_RAND` path working: both streams call libc `rand()` (it
      remains best-effort there; replay mode reporting unchanged for sim).
- [ ] Add an internal seeding hook so `D_RNG_FX` is seeded from a fixed constant
      at process start and reseeded from the same constant at replay start.
- [x] Header changes in `d{1,2}/include/maths.h`: introduce `d_rng_stream`,
      `d_rand_fx()`, `d_srand_fx()`, default-stream macro override.
- [x] Add `d_rand_get_replay_mode_for_stream(d_rng_stream)` for the (rare) places
      that branch on replay mode.
- [x] Build d1, d2 on Windows. No call sites change yet, no behavior changes.

### Phase 2 -- Trace/replay integration

- [x] Update `d_rand_annotated` / `d_srand_annotated` to take a stream argument
      and route to the right counter and state.
- [x] Update `input_demo_rng_trace_record_rand` / `..._srand` to accept the stream
      and skip FX entries (or tag them and write only sim entries to the sidecar).
- [ ] Confirm `input_demo_replay.cpp` checkpoint save/restore only reads/writes
      sim state. Add an explicit assert or comment noting FX is never persisted.
- [x] Re-run the host replay wrapper on a current recorded demo and confirm the
  replay infrastructure still runs. There is no passing demo yet, so `141502`
  serves as the current validation/demo-triage target instead.

### Phase 3 -- FX stream migration, narrow start

- [x] Migrate the two `d{1,2}/main/game.c` palette consumers
      (`diminish_palette_towards_normal`, `add_computed_color`) to `d_rand_fx()`.
- [x] Migrate the ambient water/lava sound randomness in `d2/main/game.c` to
  `d_rand_fx()`.
- [ ] Review `Fusion_next_sound_time` and split warmup-audio jitter from damage
  timing before moving any of that branch to `d_rand_fx()`.
- [x] Build d1 and d2 on Windows; run the focused RNG stream checks.
- [x] Re-run the host replay wrapper on `141502` and capture the new mismatch
  position. The frame-1 boundary mismatch moved to frame 2; the remaining
  early mismatch is legacy palette data from the old pre-split sidecar.

### Phase 4 -- Broader audit and migration

- [ ] Walk the audit target list above. For each candidate, confirm via call-graph
      inspection that the consumer does not feed save state, networking, AI,
      physics, or scoring. Migrate to `d_rand_fx()` only after that confirmation.
- [x] Audit the D2 main-menu autodemo chooser in `main/menu.c` and migrate its
  idle-time movie/demo selection RNG to `d_rand_fx()`.
- [x] Audit the d1/d2 `endlevel.c` RNG consumers and migrate the explosion,
  sound-jitter, and starfield presentation rolls to `d_rand_fx()`.
- [x] Audit the d1/d2 `collide.c` collision-delay helper and keep its jitter on
  the sim stream after deciding collision-adjacent throttles are too close to
  live player/robot contact handling to treat as safe FX-only state.
- [x] Audit the d1/d2 AI sound-timer paths and keep the `next_misc_sound_time`
  scheduling rolls on the FX stream after confirming they only throttle robot
  chatter playback and their own timer update inside `compute_vis_and_vec`.
- [x] Audit the d1/d2 robot death-roll fireball path and move its cadence/size
  rolls plus the shared `object.c` visual fireball/vclip helper randomness to
  `d_rand_fx()`.
- [x] Audit `d1/main/menu.c` plus the planned `scores/newmenu/kconfig/gauges/hud/
  titles/credits/console/gamefont/text` files in both games and confirm they
  currently contain no additional RNG consumers to migrate.
- [x] Audit `d1/d2/main/gameseq.c` and keep its wall-clock reseed and spawn-point
  selection on the sim stream because they directly control multiplayer spawn
  state.
- [ ] For modules that are clearly all-FX (kconfig visualizer, console decoration),
      use the per-TU `#define DXX_RNG_DEFAULT_STREAM D_RNG_FX` to migrate the file
      in one edit.
- [ ] Re-run host replay tests after each migrated subsystem; commit per
      subsystem so a regression is bisectable.

### Phase 5 -- Regression coverage

- [ ] Add a focused unit test under `android/tests/` that drives both streams in
      parallel and verifies they are independent (call counts and states do not
      cross-contaminate, set/get round-trip per stream).
- [ ] Add a high-level integration test that records a short demo, runs the host
      replay wrapper twice (once with FX seeded high, once seeded low) and
      confirms identical sim-stream rngtrace and identical final result. This is
      the durable regression for "FX never bleeds into sim".
- [ ] Run `android\run-code-quality.ps1 --fix` over touched files.
- [ ] Re-run a representative set of recorded demos (current `141502`, prior
      `101150`, prior `074105`) end-to-end on the host wrapper and confirm pass.

## Risks and Mitigations

- **Mis-classified consumer leaks into FX stream and breaks gameplay.** Mitigated
  by default-sim, by per-subsystem migration commits, and by the parallel-streams
  unit test plus full demo replay after each migration step.
- **D1 vs D2 drift.** The two RNG implementations stay duplicated, same as
  today. The plan touches both in lockstep; reviewers should reject any phase
  that lands in only one.
- **NO_WATCOM_RAND fallback divergence.** The libc-backed fallback can't truly
  separate two streams (single global libc state). The plan keeps two counters
  and reseeds appropriately, but documents that full determinism still requires
  the LCG path. The desktop and Android builds we ship use the LCG path, so
  the replay system is unaffected.
- **Save game / network compatibility.** The sim stream's wire format and saved
  layout are unchanged because the public `d_rand_get_state` / `d_rand_set_state`
  signatures and semantics are unchanged.
- **Upstream merge size.** Default-sim plus a one-line `d_rand_fx()` per migrated
  call site keeps the d1/d2 diff small and easy to upstream or carry.

## Out of Scope

- Replacing the LCG with a stronger RNG.
- Per-object or per-AI deterministic substreams (Doom did not need this; we
  don't either, as long as the global sim stream is the only one that touches
  game state).
- Cleaning up the existing `d_rand_*` annotation macros beyond what the stream
  argument requires.

## Open Questions for the User

- Is the FX stream allowed to diverge across runs (real wall-clock seeding) on
  non-replay sessions, or should it always be deterministically seeded so two
  fresh launches look pixel-identical? The plan currently picks deterministic
  for replay reproducibility but is easy to flip.

## Progress

- [x] Default decision made: keep `D_RNG_SIM` as the default stream and tag the
  smaller non-sim surface explicitly with `d_rand_fx()` / `d_srand_fx()`.
- [x] Phase 1 core landed in `d{1,2}/maths/rand.c` and `d{1,2}/include/maths.h`:
  two streams, stream-aware state/counter accessors, and legacy APIs still
  bound to sim.
- [x] Phase 2 trace/replay integration landed: annotated wrappers now take a
  stream and skip FX calls when writing rngtrace sidecars.
- [x] Added stream coverage to `android/tests/test_rng_seed_resume.c`.
- [x] Initial Phase 3 migration landed:
  `d{1,2}/main/game.c:diminish_palette_towards_normal`,
  `d2/main/game.c` ambient water/lava sound, and
  `d{1,2}/2d/palette.c:add_computed_color` now use the FX stream.
- [x] Phase 4 started with a menu/UI audit: D2 main-menu autodemo selection now
  uses the FX stream, and the matching D1 menu plus the planned UI file set were
  checked for additional RNG consumers.
- [x] Phase 4 continued with endlevel/gameseq audit: both `endlevel.c` files now
  use the FX stream for cinematic explosion and starfield rolls, while both
  `gameseq.c` files were explicitly left on the sim stream.
- [x] Phase 4 re-audited the d1/d2 collision sound-delay jitter in `collide.c`
  and restored it to the sim stream.
- [x] Phase 4 re-audited `next_misc_sound_time` in d1/d2 AI and confirmed it
  stays on the FX stream, while the collision-delay helper in `collide.c`
  stays on the sim stream and the shared non-damaging small-fireball/vclip
  helper randomness in `object.c` stays on the FX stream.
- [x] D2 validation passed: `run-windows-build.ps1 -Target d2` and
  `buildd2\maths\test_rng_seed_resume.exe`.
- [x] D1 validation passed: `run-windows-build.ps1 -Target d1` and
  `buildd1\maths\test_rng_seed_resume.exe`.

## New Notes

- `Fusion_next_sound_time` remains FX-owned. The live warmup branch uses the
  random draw only to reschedule the warmup sample cadence after the gameplay
  path has already decided the current fusion charge state.
- Legacy rngtrace sidecars recorded before this split still contain palette-only
  calls. The `141502` demo now reaches the expected sim state by frame 2 once
  those old palette events are mentally filtered out, which means future
  re-recorded demos will be more useful than the current pre-split traces.
- The D2 main-menu autodemo chooser is presentation-only and now uses `d_rand_fx()`.
  The matching D1 main menu has no RNG at that site because it delegates random
  demo selection to `newdemo_start_playback(NULL)` without a local roll.
- `next_misc_sound_time` remains FX-owned. It is serialized in `ai_local_rw`,
  but the live `compute_vis_and_vec` reads only use it to throttle chatter
  playback and rewrite the timer itself; it does not gate visibility,
  awareness, movement, or attack decisions.
- `check_collision_delayfunc_exec()` is also back on the sim stream. Its current
  effect is still sound/explosion throttling, but it sits directly on live
  player/robot collision handling, so the safer classification is simulation.
- D2 `Next_seismic_sound_time` remains FX-owned. The simulation-owned rolls
  already decide whether seismic shaking starts and how much rotation is
  applied; the FX timer only jitters looping rumble cadence and volume updates.
- `gameseq.c` remains simulation-owned. Its `d_srand((fix)timer_query())` and
  `d_rand()` calls directly choose multiplayer spawn positions and therefore must
  stay on the sim stream.
- `endlevel.c` is now treated as presentation-only for RNG purposes. Its current
  consumers only drive starfield generation plus endlevel explosion placement,
  timing, and sound cadence.
- `fuelcen.c` stays on the sim stream. Its RNG chooses robot materialization
  timing and robot type, both of which directly affect gameplay state.
- `fireball.c` still mostly stays on the sim stream. The reviewed callers there
  feed powerup lifetimes, debris/object motion, robot contents, connected-segment
  selection, or damaging wall/robot explosions, so they are not safe bulk FX
  candidates.
- The mixed-system Phase 4 bucket is narrower now: most of the clearly cosmetic
  AI sound/death-roll paths are moved, while remaining `fireball.c` and related
  systems still need case-by-case review rather than blind `d_rand_fx()`
  replacement.
