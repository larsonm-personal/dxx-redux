# Fixed forward gameplay view

## Chosen reference and scope

The user chose the original DOS full-cockpit reference after an audit of the
1995/1996 source and original D1 shareware 1.4 / D2 demo 1.0 assets. The active
constants and commented-out fullscreen/status-bar alternatives are centralized
in `android/app/src/main/cpp/shared/gameplay_view_policy.h`, with pinned sources.

The original physical monitor was 4:3; the cockpit 3D viewport was not. Preserve
320x200 screen pixel correction, the inclusive 320x133 cockpit hole and DOS zoom
0x9000. Fixed-point operations produce screen aspect 78643 and projection scales
(32685,65536,36864). This is approximately 58.715507 degrees vertically and
96.877210 horizontally. Angles are explanatory, never rounded production inputs.
AspectX=3/AspectY=4 are correction factors, not width/height. The earlier Android
FoV fix standardized zoom only, retaining actual screen/cockpit shape and rear
orientation. Its visual FoV labels and setting-to-zoom mapping remain unchanged.

The dedicated gameplay collector uses ship position/orientation, with no stereo,
rear, external-camera or cockpit offset. Candidates still come from native portal
traversal and segment object ordering, not a new object-center frustum test.
Native homing's separate angle, range, visibility and target-type tests remain.

## Deliberate behavior changes

- D1 and imported D1 first acquisition use the preceding simulation step's fixed
  forward candidates instead of the last drawn camera list.
- Native D2 first local-player acquisition uses that same bounded list in live
  and replay. This replaces both the wall-clock-window selection/fallback and
  replay-only complete scan. Offscreen/occluded candidates do not gain a new
  complete-scan path. Multiplayer, robot-fired and later reacquisition retain
  their original complete scans; native D2 range cap, reverse order and tie rule
  are unchanged. D1/imported D1 keep their original uncapped first-acquisition
  range and historical 50-slot upper-half overwrite.
- Warning coordinates again use the g3-scaled matrix, as original rendered
  warnings did. The recent simulation-side refactor had accidentally reduced
  the horizontal box half-width to4. The chosen cockpit reference restores it
  to approximately8.020; vertical half-width stays4. Strict comparisons and
  fixed-point rounding are retained. This forward box is not an angular FoV or
  LOS test. Modern simulation-side scheduling and polygon-robot eligibility
  remain; this does not restore every original rendered-object eligibility rule.

## Audited presentation dependencies

The gameplay collector fixes screen pixel correction, viewport size, zoom,
ship-forward orientation, ClassicDepth=0 (modern default ordering), and disables
cosmetic acid vertex wobble during collection. Acid's wall-clock timer is never
queried on this path; the actual visual effect remains unchanged afterward.
Render_depth is the normal build default: MAX_RENDER_SEGS under OGL,20 under
software. Temporary endlevel/detail settings cannot leak into this view. This
preserves current backend coverage rather than silently widening software or
restoring DOS detail presets. OGL headed/headless builds share coverage; universal
software/OGL candidate equality is deliberately not claimed.

The collector restores visual camera/projection globals, canvas pointer, public
segment list and settings. It neither draws nor writes world objects, RNG streams,
automap discovery or visual object-list metadata. Rotation, portal and object-row
scratch is rebuilt, so callers are restricted to simulation/input boundaries;
the next visual consumer must begin its normal render frame. Explicit projection
setup does not reset the temporary-clipping-point allocator. Existing current-
camera collection and actual-renderer equivalence tests retain their contracts.

Automap discovery retains the existing camera policy, including actual rear and
auxiliary camera orientation and viewport geometry. Android's wider custom-FoV
draw remains visual-only: its existing base-zoom camera pass owns discovery.
The recording-format branch separately owns current-camera discovery parity and
its main/auxiliary-view support contract; the combat policy does not replace
those cameras with the canonical view.

## Cache timing and invalidation

A completed GameProcessFrame publishes the next input batch's candidates. Current
step movement does not replace them midstep. First input after a world reset
prepares the cache before new projectiles enter object rows, preserving native
sort/overflow semantics. The simulation wrapper also prepares before its first
step. Player reset/respawn positioning, object initialization, save-restore reset,
secret return, compression reset, explicit segment teleports, exit placement and
Android co-op warps invalidate the cache. Automation helpers that replace the
player pose also reset it; ordinary movement and obj_relink do not. The header
documents that explicit pose-replacement contract. Invalid/endlevel views
publish an empty list until a subsequent ordinary frame; no extra GPU draw or
simulation RNG is introduced. Automap does not freeze combat geometry based on
presentation state.

## Validation and admission

Focused tests live in `fixed_gameplay_view_fixture.hpp`; real public-level input,
simulation, respawn and save-restore checks live in
`gameplay_view_lifecycle_fixture.hpp`. They exercise exact angular/portal edges,
three physical display aspects, five cockpit modes, four visual zooms, rear and
external cameras, transient depth/ClassicDepth/acid settings, independent overflow
ordering, warning thresholds/signatures, and actual AI avoidance. The existing
CPU-versus-actual-renderer oracle is preserved and strengthened with camera-map
checks. Software coverage requires its own build and is not implied by OGL runs.

Qualification remains in progress; no oracle has been replaced.

- Exact projection/candidate settings matrix: native D1 480 combinations;
  D2/imported D1 960 combinations. Tests include loaded-versus-live bounded
  acquisition, actual AI warning/dodge velocity, stale-signature rejection,
  full projection/temporary-point allocator restoration, angular/portal edges,
  101-candidate independently derived overflow, acid timing and 28-segment depth
- Native public-level lifecycle: D1 8 checks, D2 9, imported D1 9. Actual forward
  lists differ from reverse lists; real yaw crosses a candidate boundary after
  9 frames for D1/imported D1 and 23 for D2. Native save/restore replaces a warm
  opposite-pose list and recovers the saved forward list. The native segment
  teleport helper also replaces a warm reverse list before the first input
- Actual visual renderer: 306 CPU/renderer candidate-list cases each for native
  D1, imported D1 and native D2, including explicit forward/rear automap discovery,
  remain equal. Native D2 adds a public-level 1/2/3 profile; the original
  D1-resource oracle and its default retail levels remain unchanged
- Full native upstream compatibility and existing Android FoV unit targets pass
- Independent source review found no remaining blocking production issue. An
  independent actual-code harness matched warning scaling to g3 bit-for-bit for
  2,000 arbitrary orientations/points per engine
- Scoped mixed-language formatting passes using the real pre-Android upstream
  baseline. Automation catalog validation passes. Master catalog invocation is
  blocked by the host supervisor's missing /proc/self/task/.../children, also
  reproduced on the unchanged recording branch; its test is not weakened

All 26 native repetitions on each headless/graphics backend were compared with
byte-identical old full-frame/RNG/terminal oracles. Nine cases remain unchanged;
four intentionally diverge after the approved warning correction: D1 door,
D1 combat, D2 combat and D2 guidebot release. The earlier cockpit-only snapshot
passed all 13 cases, isolating the newly approved warning change.

D1 door first differs at frame128: robot48 at raw x=-7.785507 is projected to
x=-3.882904, storing the warning that the old raw-width check omitted. Independent
raw-trace review confirms only its warning number/signature change across all
426 frames; world/player state, RNG bytes and terminal result remain identical.
D1 combat additionally changes actual evasion: the first warning at boundary96
produces the exact reconstructed dodge velocity at97. Three10-damage hits now
land at105,105,116 instead of105,115,125, still giving one kill and200 score.
Death moves125->116, loot132->123 and retirement147->138. Reapplying old recorded
RNG boundaries produces an extra drop, so that replay output is not an acceptable
oracle. Fresh continuous native capture rolls26613, drops nothing, and retains
24 powerups,88 shields,200 score and3 lives. The second incoming6-damage hit moves
117->116; its projectile/robot identities and the first hit remain unchanged.

Fresh diagnostic native captures of all four affected cases replay exactly on
both native backends, with only those observed event-boundary changes and
Guide-Bot release190->188 / physical exit181->179 needed for action verification.
D2 combat's263-frame trace changes only robot11's warning number/signature,
first at boundary190; its RNG and terminal result stay byte-identical. Guide-Bot
first differs in a warning signature at106 and actual buddy motion at115. That
motion produces a genuine downstream SIM RNG change: the first context/timing
difference is SIM event576 (old139/new138). The224 FX events remain equal. This
is not cosmetic or RNG-identical. Guide-Bot damage101 and cage blast177 remain
unchanged; the natural recording is
219 rather than221 frames. D1 door retains426 frames, D1 combat becomes139 and
D2 combat retains263. These captures preserve each scenario's identity, damage,
death, score, loot and survival objectives. They are causal evidence, not admitted
oracles, because their binary's cached CMake revision predates the source edits.

Use `android/helpers/capture_public_level_fixture.py` for final capture. It checks
clean source HEAD against a full expected published revision, requires a fetched
origin ref containing that revision, queries the fixture's embedded revision,
isolates HOME/XDG state, verifies recorded metadata and checks that the executable
bytes did not change during capture. It records source and executable provenance.
After publication, log a clean-checkout reconfigure and rebuild before using
this guard; never fix a stale build stamp by editing the recorded metadata. The
label check detects stale stamps but is not a source-byte build attestation; the
controlled clean build and retained executable hash are part of admission.

The local recording-format restack preserves actual-camera discovery and passes
all94 native ordered-input, modal, first-input and continuous-RNG carry/tail checks.
Independent integrated source review is clear; the exact D1/Guide-Bot metadata
changes also pass independent strict action verification without weakening their
structural assertions. Software backend, Android device, Windows and macOS
execution remain unrun here.

## Final clean-source admission

The retained `capture-fixed-forward-gameplay-view` branch points to published
source `9be53d6d74feede5b3df4cf7dda19fc10b5f9fc2`, tree
`6ad9b1a1784fdc9f5539a159971c428f336e1483`. Both engines were reconfigured and
rebuilt with `--clean-first` from that clean checkout. The capture guard verified
matching embedded revisions and unchanged executable hashes before and after
four fresh independent native captures. Those captures reproduce the reviewed
causal evidence exactly, apart from build provenance; no replay output was used
to produce their oracles.

Only D1 door, D1 combat, D2 combat and D2 Guide-Bot recording/oracle pairs are
replaced. The manifest changes exactly five action boundaries: D1 death at 116,
loot at 123 and retirement at 138; Guide-Bot release at 188 and exit at 179. The identified D1
second incoming projectile impact changes 117 to 116, with the same identity and
six-shield loss. All target, damage, score, no-drop, physical-exit and survival
assertions remain exact. The other 13 parent pairs (nine original scenes and four
new secondary scenes) and all 14 historical migration sidecars remain byte-identical.
The replaced pairs now bind fresh live provenance directly; the old sidecars are
retained historical conversion evidence, not provenance for these new pairs.

Final native qualification passes 142 distinct replay executions across headless
and graphics: 68 unchanged-input runs serve both original-oracle/action positives
and deliberately wrong-oracle rejection, 68 altered-input runs, four partial
no-collection controls and two complete 582-frame no-collection replays of a separate live
capture. Reused positive evidence is counted once, not as additional independent
runs. Every early altered-input rejection retains its exact phase/frame and reconstructed
RNG proof; no negative-profile boundary changes were needed. The complete
no-collection scene matches its own live oracle on both backends and fails the
unchanged physical-collection action gate, preserving meaningful negative coverage.
Independent review reconciles every run identity and before/after corpus, verifier
and executable hash.

The final clean binaries also pass 480 D1 and 960 D2/imported display combinations,
26 public lifecycle checks,918 actual-renderer equivalence cases and both engines'
upstream/FoV CTests. The existing imported-D1 paired scope passes its required
flight gameplay/SIM checks and native repeatability; broader imported diagnostics
remain incomplete, including the retained door/loot differences. No imported
scope or oracle was relaxed. The subsequent parent CI-budget-only changes do not
alter the retained capture source, engine code or capture recipes.
