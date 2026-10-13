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
control clear, full control sample, controls-only update, rear-view decision,
automap phase, or allowlisted direct command. Every operation
has its actual input-time FrameTime, separate from the later simulation timestep.
The batch retains pre-input SIM state/count; frame RNG is the post-input boundary.
Replay restores the initial SIM state/count once at frame zero, even for an empty
first batch. Every later pre-input and tail boundary checks the carried state and
absolute count before effects; computed residual controls and post-input RNG must
match as well. Unexpected carry differences abort instead of being reseeded.
External reseeding and zero-frame/tail-only recordings are unsupported. Mandatory input_tail preserves effects after the last simulation.
Empty batches are explicit. Bounds are 1024 operations and 256 KiB per batch;
overflow, malformed values, unsupported versions and unknown operations fail closed.

Lossless aliases refer only to the decoded current-frame/tail residual controls
or post-input RNG. Missing sample pulses mean zero; an omitted dispatch batch is
invalid. Diagnostic telemetry is separately allowlisted and hashed, including
terminal diagnostics. Public admission excludes debug spawning and warps.

Native playback accepts format 5 only and gives converter/re-record guidance for
older input formats. General codec tests retain legacy parsing for offline tools.
Rear-view records native semantic decisions without replay timer reads. Automap
records ordered begin/depths/active/close phases with controls-only updates and
resolved D2 marker deletion; replay uses no modal UI.
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

## Approved modal completion

Normal rear-view and automap use remain recordable. Current gameplay camera policy
is preserved; any separate combat-view policy change is outside this format work.
The rear-view reducer keeps native sticky/nonsticky decisions and cockpit effects,
with an explicit initial baseline. Older converted format5 files without that
baseline retain their ordinary native startup state. Automap depth recomputation
is a real world-state effect, recorded independently of successful activation so
partial entry/close ordering is representable. No simulation runs with a virtual
map open; a terminal input tail may end fully active. Pure camera/redraw activity
adds no operations. Confirmed marker deletion pins the exact marker signature.
Debug map-reveal and Android reactor/matcen cheats cancel capture with a diagnostic
rather than silently recording unmodeled world edits.

## Verification status

- The durable native matrix passes all94 checks on D1/D2 headed and true headless
  backends, including both25-frame SDL orders, native rear/map/marker effects,
  carried RNG state/count and terminal-tail negatives, and stale-profile isolation
- All 37 size-eligible migration candidates pass the original full oracles; the
  two verbose loot candidates exceed the file limit and are not admitted
- All six D1/D2 standalone codec suites and50 focused Python tests pass; coverage
  includes malformed operations, aliases, duplicate
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

## View-discovery contract and bounded support

The new visited-segment oracle exposed a pre-existing omission: the headless
candidate collector did not reproduce renderer map discovery. Discovery now has
an explicit CPU-only pass, separate from the pure homing candidate collector.
It uses the current main camera and native default D2 missile-window selection
and geometry, without drawing, publishing candidate lists, waking robots, or
querying timers/RNG. Replay applies it after simulation and before the next input
batch or terminal tail. Ordinary live rendering retains its original behavior.

F3 and the existing reverse helper preserve their resolved cockpit modes as ordered operations.
New production captures include optional `view_projection` metadata with seven
required fields: physical screen width/height, fixed-point screen aspect and base
zoom, portal depth, classic-depth mode, and missile-view-enabled preference (zero
for D1). These values verify initialized native state; replay never allocates or
reconfigures graphics from the metadata. Bounds are 16384 per dimension, 64M total
pixels, positive fixed-point aspect/zoom through 16*65536, and depth through9000.
Migrated files lacking this optional object retain prior startup behavior.
Resolution/aspect/base-zoom/depth/missile-preference changes stop companion capture
with a diagnostic instead of producing a silently mismatched replay. The existing
Android visual-only FoV override remains separate from base discovery semantics.

Active guided-missile and other selected auxiliary cockpit cameras are outside
this bounded qualification and stop the companion input recording with a named
notice; normal live camera behavior and classic demo recording are unchanged.
Cancellation discards the companion rather than claiming an unverified prefix.
Endlevel special-camera map fidelity remains unqualified. These paths were not
represented reliably by the old aggregate format either. Main view, rear view,
F3 and the existing reverse helper, ordinary single-player automap and default missile view are the
supported target of the new native checks. Dead-player camera discovery keeps
its native predicate; it is not replaced by the living ship's view.

New modal and secondary captures use native pixel-aspect defaults3:4. Earlier
public fixture sources used4:3 factors, which were not the same screen projection
on the true headless backend; their published original oracles remain unchanged.

## Native startup isolation

The Unix replay wrapper now assigns a fresh child HOME for every run, including
reused executable sandboxes. It writes the ordinary640x480 configuration in the
native profile directory and stages only an explicitly requested named pilot.
It never imports the parent configuration or arbitrary nameless pilot. Native
integration runs deliberately inherit stale external pilot/config files and
verify their bytes stay untouched. The graphical no-pilot command-line replay
path initializes the same native player defaults as headless and live capture;
it does not set individual fields from expected recording metadata.
