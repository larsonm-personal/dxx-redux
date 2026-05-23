# Engine determinism roadmap 2026-05-10

## Goal
- step back from individual demo symptoms and identify reusable engine nondeterminism classes
- turn recent replay/debug evidence into a prioritized plan for removing nondeterminism from the game engine
- preserve the demo suite as regression protection for later refactors

## Steps
- [completed] mine recent replay notes, plan files, and instrumentation results for recurring nondeterminism patterns
- [completed] group findings by engine subsystem and likely root cause class
- [completed] propose focused instrumentation and fixes that reduce nondeterminism rather than just document demo behavior
- [completed] summarize the recommended regression-demo/test strategy

## Recent evidence to reuse
- demo 102637: replay-only debris wall hit creates object 21230 while frame-level RNG metadata is still aligned, so this is an object motion, collision, or ordering class rather than a first-order RNG class
- demo 102738: first shared mismatch is player velocity after a player weapon creates a badass explosion and applies blast force, followed by wall resolution, so this is a player blast/contact class
- checkpoint signature seed: missing runtime allocator state caused object identity drift, proving that transient engine globals are part of replay state
- segment object order after save/load: FVI walks segment object linked lists and uses the first closest hit it finds, so restored order can alter hit detection without changing RNG

## Workstreams

### 1. Detection and triage infrastructure
- re-record the current regression demos with object count/hash metadata on both expected and actual traces
- added compact object-slot bucket diagnostics so aggregate object hash divergence reports the changed object index range
- add a fuller first divergent object report later: signature, type, id, segment, position, velocity, life, parent, and linked-list neighbors
- added a segment object list hash per frame, so save/load ordering bugs surface before hit detection diverges
- keep the full labeled RNG trace, but treat it as one lane of evidence, not the only truth, because physics and ordering can diverge with identical RNG state

### 2. Runtime state fidelity
- maintain an explicit runtime-state inventory for every global or static value that affects simulation after a checkpoint
- add a host round-trip test that loads a checkpoint, writes runtime state, restores it, and asserts object allocator state, object hashes, segment object list hashes, RNG state, AI path state, weapon state, effect state, and timing deltas are identical
- audit static values in object, laser, AI, AI path, control center, effect, morph, stuck object, afterburner, physics, and game tick code
- for input-demo checkpoint loads, make restore fallback paths noisy or failing when they alter simulation order, especially segment relink-by-index fallback

### 3. Object allocation and segment traversal order
- preserve exact segment linked-list order across save/load and checkpoint restore, including prev and next pointers
- add a test fixture that constructs several objects in one segment, saves and restores, then asserts FVI sees the same ordered candidates and returns the same hit
- decide the engine policy for equal-distance FVI ties: either preserve historical list order exactly, or add an explicit deterministic tie-break by distance, signature, and object number
- avoid silent canonical relinks in replay paths unless the resulting order hash is proven identical to the saved order

### 4. Collision, FVI, and player contact physics
- turn the current motion probes into reusable contact probes around FVI candidate order, chosen hit, wall normal, hit point, remaining sim time, retry list, and final velocity
- use 102738 as the first regression for blast force plus wall-resolution determinism, not as a one-off blast story
- add focused tests for player blast impulse near walls and corners, debris wall contact, weapon robot hits, and persistent-object retry behavior
- look for unstable branch thresholds where one fixed-point unit changes fate, such as wall bounce, illegal wall intersection repair, retry count, and hit-segment recovery

### 5. RNG discipline
- moved random custom music track selection in D1/D2 `songs.c` and `jukebox.c` to the FX RNG stream so audio presentation no longer consumes simulation RNG there
- moved D2 seismic sound-only rescheduling to the FX RNG stream, while leaving seismic start and shake physics randomness on simulation RNG
- split more simulation RNG from presentation or sound RNG where possible, especially random sound timers and other non-gameplay work that currently consumes the shared stream
- label every simulation RNG call with a stable subsystem and object context in input-demo traces
- first non-matching RNG origin reports now include state/result and object context fields, but should still be read after checking whether object or physics state diverged earlier
- review FrameTime-scaled random gates in AI and pathing, because small timing or pose differences can change whether a later RNG call happens
- random robot sound timers and collision sound jitter still need a targeted pass because related timers are stored in AI or static state

### 6. FrameTime and math determinism
- replaced D1/D2 AI path velocity smoothing float math with `dxx_ai_path_smoothing_delta()` and added a host `test_deterministic_math` target in both builds
- continue replacing float math in simulation paths with fixed-point equivalents when feasible
- audit FrameTime-dependent thresholds that combine random gates, path decisions, drag, turnroll, and physics substeps
- add cross-build host tests for deterministic traces where practical, especially Windows debug/release and Android debug/release
- keep load-time float parsing lower priority unless a loaded value differs across platforms, because the replay failures so far are runtime divergence

### 7. Regression suite shape
- keep a small named suite of deterministic probes: allocator identity, segment object order, player blast contact, debris wall contact, weapon robot FVI, AI path RNG, homing weapon frame state, and level transition checkpoint restore
- each fixed nondeterminism should get either a short input demo or a host fixture that fails before the fix and passes after it
- keep demo logs as diagnostics, but make pass/fail depend on state hashes, object-list hashes, first divergent object reports, and labeled RNG-call comparisons
- re-record demos after intentional schema or checkpoint-version changes, then keep the older traces only as migration diagnostics
