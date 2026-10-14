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
resolved cockpit setting, automap phase, or allowlisted direct command. Every operation
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
- At the initial conversion checkpoint, all 37 size-eligible migration candidates
  passed the original full oracles; the two verbose loot candidates exceed the
  file limit and are not admitted. Final carried-RNG qualification reruns the
  13 admitted choices on both backends, rather than claiming all discarded
  candidates were rerun after later runtime hardening
- All six D1/D2 standalone codec suites and 79 focused Python tests pass; coverage
  includes malformed operations, aliases, duplicate
  keys, UTF-8, command domains, tail/bounds, rewind cancellation and result-write failures
- Native stress probe covers first-input RNG, variable input/simulation time,
  command interleaving, primary/flare/bomb/cycle input, clear and reset-only
  operations, explicit empty batch, simulation-phase guards and terminal effects
- Review fixes prevent D1 live input leakage and paused batch reapplication;
  completion requires an actual successfully closed result file
- Direct recorder callers were audited; weapon-order and Guide-Bot synthetic
  fixture captures now include their required RNG-count flag

The clean source checkpoint is `eba0eee69077fd53e4aee54d04f026f8d7ac9e6c`,
retained as `capture-public-secondary-ordered-v5`. All four secondary scenes were
captured twice with byte-identical replay/oracle pairs from rebuilt binaries with
that embedded revision. Their replay files are 5680-5969 bytes and full oracles
3203-3907 bytes. Optional actual-render screenshots preserve the same bytes.
All 68 native full-17 corpus runs pass, including every unchanged original oracle.
The existing imported graphics scope also passes. All 68 wrong-oracle controls
pass. Final early-abort verification and exact-head hosted CI remain pending at
this publication checkpoint. Development captures remain
diagnostic evidence and are not substituted for these source-qualified fixtures.

## Causal rejection and completed no-collection proof

The carried-RNG guard changes the expected negative-control endpoint. The fixed
corpus verifier pins each rejection phase and frame, validates the launched
input-only mutation, reconstructs every SIM RNG event and absolute count, and
checks the complete partial object/world prefix plus its real gameplay effect.
Natural exit 1 and missing terminal output alone never qualify a negative.
Wrong-oracle controls and both D1 secondary no-fire cases keep their completed
terminal requirements. Reports distinguish retained immutable native evidence
from fresh runs; the original failed-verifier reports remain available.

The D1 loot input mutation rejects at frame 567, before the original collection.
It proves pre-collection divergence only. A separate generated live control keeps
the same ordinary route through the real drop, then leaves controls zero through
all 582 frames. Both replay backends must first match that control's own full
live state, SIM RNG and terminal result with a natural clean exit. Only then does
the unchanged original collection action requirement reject the missing pickup.
The exact live pickup identity, original kill and score remain mandatory. This
generated control is outside the admitted 17-case inventory; the full durable
native matrix now contains 96 checks. Its first diagnostic runs pass on both
backends, with final clean-source recapture pending. An unset-switch capture also
preserves every original loot frame hash, SIM event, terminal result and decoded
input; no original expected oracle was rewritten.

Scoped formatting and automation-catalog checks pass. The master catalog cannot
run in this host's process namespace (`/proc/self/task/.../children` is absent).
Broader Python discovery is not a clean pass: its six source-contract failures
or lookup errors reproduce on unchanged PR29, while five other errors are missing
numpy/clang, denied Unix sockets and two unchanged process-fixture startup
timeouts. The 79 focused replay tests pass independently. Retail-backed native
integrations compile but were not run without their required assets.

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
