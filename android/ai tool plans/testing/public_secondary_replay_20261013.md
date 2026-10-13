# Public secondary-fire capture and playback

## Approved implementation and bounds

1. Minimize the two-tap mismatch at ordinary public level starts in both engines
2. Preserve actual native input dispatch in format 5 without changing gameplay timing
3. Convert old public recordings using at most three whole-recording schedules, checked against every unchanged original oracle
4. Admit four small SDL event-order scenes, keeping each replay and each oracle at or below 100000 uncompressed bytes
5. Qualify D1/D2 headed and true headless replay, natural exit, causal negatives and existing recorder callers
6. Publish exact source provenance and a focused stacked draft, then monitor exact-head CI

No embedded save/checkpoint, warp, injected world state, changed expectation or merge is authorized for the public corpus

## Diagnosis

At fixed timestep 3276, native secondary presses at frames 2 and 12 place the
second press eight ticks before the loaded 32768-tick concussion cooldown. At
frame 13, a zero-motion mouse event before the release causes another firing
check and spends the second missile. Release before mouse motion spends only one.
Both games reproduce this through SDL_PushEvent, event_poll and ReadControls from
ordinary level-one starts. This controlled fixed-timestep native SDL dispatch
capture explicitly drives the production input/simulation order; it is not a
real-time event_process-loop capture.

The old aggregate controls are identical for these different live outcomes. Old
playback picks two shots and first diverges at frame 13 for release-first. This is
missing protocol information. Altering missile cooldown or live dispatch timing
would hide the cause and change ordinary gameplay.

## Format 5

Each frame carries an explicit bounded ordered dispatch batch: marker reset,
control clear, full control sample, or allowlisted direct command. Every operation
has its actual input-time FrameTime, separate from the later simulation timestep.
The batch retains pre-input SIM state/count; frame RNG is the post-input boundary.
Replay checks its computed residual controls and post-input RNG without replacing
a mismatch. Mandatory input_tail preserves effects after the last simulation.
Empty batches are explicit. Bounds are 1024 operations and 256 KiB per batch;
overflow, malformed values, unsupported versions and unknown operations fail closed.

Lossless aliases refer only to the decoded current-frame/tail residual controls
or post-input RNG. Missing sample pulses mean zero; an omitted dispatch batch is
invalid. Diagnostic telemetry is separately allowlisted and hashed, including
terminal diagnostics. Public admission excludes debug spawning and warps.

Native playback accepts format 5 only and gives converter/re-record guidance for
older input formats. General codec tests retain legacy parsing for offline tools.
Rear-view wall-clock input and modal automap cancel ordered capture explicitly.
Android rewind also cancels companion input recording because its saved world
boundary does not include pending ordered input history; gameplay rewind and
classic demo recording keep their existing behavior. Continued input recording
after rewind requires a new recording.

## Bounded migration

Try sample-only, reset-before-sample, and prior-held-then-current for each whole
recording. Preserve original replay/oracle hashes and immutable full baseline
state/RNG/terminal evidence. No per-frame search or case-specific engine heuristic.
A candidate must reproduce every old expected frame/diagnostic/SIM event, terminal
state and independent scene assertion. Multiple passing schedules establish
behavioral equivalence, not reconstruction of historical SDL event order.
Oversized candidates are size-ineligible, not semantic failures. Old oracle bytes
remain unchanged; a sidecar binds the converted replay to its original oracle.

## Verification status

- Development SDL captures: both event orders match all 25 live frame, diagnostic,
  SIM-event and terminal expectations in D1/D2 on headed and true headless backends
- All 37 size-eligible migration candidates pass the original full oracles; the
  two verbose loot candidates exceed the file limit and are not admitted
- D1/D2 standalone codec suites pass malformed operations, aliases, duplicate
  keys, UTF-8, command domains, tail/bounds, rewind cancellation and result-write failures
- Native stress probe covers first-input RNG, variable input/simulation time,
  command interleaving, primary/flare/bomb/cycle input, clear and reset-only
  operations, explicit empty batch, simulation-phase guards and terminal effects
- Review fixes prevent D1 live input leakage and paused batch reapplication;
  completion requires an actual successfully closed result file
- Direct recorder callers were audited; weapon-order and Guide-Bot synthetic
  fixture captures now include their required RNG-count flag

Final-source runtime matrices, existing native fixture checks, source-qualified
capture admission, formatting/catalog checks and exact-head hosted CI remain to
be completed before the draft can be reported fully qualified. Development
captures are diagnostic evidence, not final provenance-qualified fixtures.
