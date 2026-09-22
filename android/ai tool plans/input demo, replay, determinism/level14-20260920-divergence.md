# Level 14 recording divergence investigation

Recording: `d1_descent_level14_20260920_184354.dximdemo`, made on Android arm64
on 2026-09-20. Native host replay still diverges from the recording because of the homing
acquisition issue below. After the compatibility fixes, D1-in-D2 matches the
native replay across all 5,696 recorded frame summaries

The user observes a small ship-angle mismatch after acquiring/using Spreadfire,
followed by missed shots. Treat the recording as fresh evidence, and locate the
first state/RNG difference before attributing the cause to weapon selection

1. Capture native D1 state and RNG traces using the unchanged recording
2. Compare frame-zero and earliest differing state, orientation and RNG events;
   locate Spreadfire pickup/selection/fire and preceding collisions
3. Trace the responsible engine operation and test the suspected cause with a
   controlled change before choosing a permanent engine fix
4. Rebuild and replay native D1; compare D1-in-D2 separately and identify any
   additional compatibility divergence
5. Record evidence, limitations and exact remaining mismatches. Do not alter the
   recording, replace expected output, or add replay-specific compensation

## Findings on 2026-09-21

Follow-up implementation: trace native/imported player velocity and the full
orientation matrix before frame 1519, identify the first different operation,
place the behavior correction in the D1 compatibility owner, and verify the
unchanged level-14 recording plus the existing native comparison tests. Compare
against the native replay separately from its known recording/homing mismatch

Native D1's failure is caused by replay-only homing acquisition. D1-in-D2 also
has an earlier, independent drift. Spreadfire selection is not the first
divergent event in either trace. The initial investigation made no permanent engine changes; the subsequent
compatibility repairs and their validation are recorded below

### Native D1: controlled reproduction

All runs used the unchanged 5,696-frame recording, its embedded checkpoint,
the Brazilian anniversary-edition D1 assets, default rendering, hidden robot
labels, accelerated playback and the same current host source except for the
one explicitly identified experimental removal

| Run | Homing acquisition | Result |
| --- | --- | --- |
| Fast, no rendering | Existing replay-only complete object scan | Fail |
| Visual control | Existing replay-only complete object scan | Fail; state trace byte-identical to fast run |
| Visual experiment | Temporarily remove the replay-only branch; use ordinary rendered-list acquisition | Pass |

The passing experiment matches every recorded `state` field on every frame,
including fixed-point ship position and forward direction. Live-object,
robot, weapon, allocator and runtime hashes also match throughout. This is
stronger than the final summary check, but is not a claim of complete internal
state equality: the recording lacks the populated player-bump diagnostics
emitted by today's replay, and AI danger-laser bookkeeping differs for exactly
one frame (1287), then converges without changing the live-object trajectory

Timeline below uses zero-based frame indices and cumulative recorded frame time

| Frame | Elapsed | Observation in failing native replay |
| --- | --- | --- |
| 2735 | 1:49.56 | Spreadfire selected; ship and live-object state still match |
| 4212 | 2:48.68 | First live-object/weapon hash mismatch, in object slots 160-191 |
| 4223 | 2:49.12 | New homing missile slot 71, signature 1622: recording targets robot 33, replay targets robot 155 |
| 4227 | 2:49.28 | Traced missile direction and velocity differ |
| 4275 | 2:51.20 | First simulation RNG call-site/order difference (`do_ai_frame` versus `create_awareness_event`) |
| 4742 | 3:09.90 | First summary-state mismatch: homing ammo/powerup pickup |
| 5000 | 3:20.23 | First ship-position mismatch; shields also differ |
| 5306 | 3:32.48 | First ship-forward-direction mismatch |

The relevant branch is `d1/main/laser.c::find_homing_object`, introduced in
commit `acb39c5ff`. Recording uses the retained rendered-object list in reverse
order, quick vector normalization, and no complete-scan distance cap. Replay
instead uses `find_homing_object_complete`: ascending object slots, exact
normalization and a 250-unit range cap. These are different gameplay
algorithms, even with rendering enabled. The controlled visual A/B test proves
this substitution causes the native level-14 failure; it does not yet isolate
which individual search difference changes the initial winning target

This is also the issue described by existing audit finding BR-0386 in
`android/ai tool plans/code management/branch_adversarial_review_ledger.md`.
Both ordinary D2 and `d1_in_d2_acquire_homing_target` contain the analogous
replay-only substitution

Per-frame recorded RNG states are restored by the replay system, so equality
of the frame `rng` field is not independent evidence of RNG-consumption parity.
The native baseline RNG sidecars were compared by simulation event, ignoring
source path/line labels; their first differing call site is the frame above

### D1-in-D2: separate earlier divergence

The imported fast run used the same embedded checkpoint and byte-identical D1
HOG/PIG data, with D2 assets available in the combined data directory

| Frame | Elapsed | First observed difference |
| --- | --- | --- |
| 0 | 0:00.04 | Object/AI/link diagnostics already differ; imported checkpoint equivalence is not established |
| 474 | 0:19.00 approximately | Traced laser position differs while ship position still matches |
| 1414 | 0:56.7 approximately | A flare has expired in imported playback while native still has it |
| 1519 | 1:00.89 | Player object/physics hash first differs, before the position summary changes |
| 1521 | 1:00.97 | Ship X differs by one fixed-point unit: -32113597 versus -32113598 |
| 2097 | 1:24.03 | Vulcan ammo is 271 versus 369 |
| 3496 | 2:20.03 | Shields are 17 versus 18 |
| 3653 | 2:26.31 | Ship is in segment 582 natively, still in 615 when imported |
| 3654 | 2:26.35 | First forward-direction mismatch, while moving into segment 582 |

Thus the imported angle mismatch follows an earlier positional drift, and
occurs before the native homing divergence. Segment-dependent auto-leveling is
a concrete next hypothesis for the angle change, since the two runs enter the
segment on different frames; it has not been proven by a controlled experiment.
Do not attribute this imported failure solely to the native homing bug or
claim that Spreadfire is responsible based on the visible timing

### Repair direction and remaining work

1. Remove the recording/replay split in homing acquisition while preserving
   D1's intended targeting rules. Simply deleting the branch works for this
   rendered replay but leaves no-render playback without a current candidate
   list. A complete fix must supply equivalent targeting inputs without drawing,
   or explicitly capture the presentation-dependent inputs for replay. Do not
   silently replace ordinary D1 targeting with D2 targeting or the existing
   full-scan variant merely to make replay deterministic
2. Exercise normal play, recording, visual replay and no-render replay with
   competing targets, equal alignments, range boundaries and view changes.
   Keep D1-in-D2 policy in its weapon owner, with minimal original-file hooks
3. Establish imported checkpoint equivalence and trace its first different
   weapon movement/expiration and ship-velocity update before adjusting its
   later angle. Check the ammo discrepancy independently
4. Repeat the unchanged level-14 recording and the other four D1 regression
   demos. The native rendered experiment establishes a useful reference; it
   does not certify imported fidelity or fix headless replay

### Evidence and workspace state

- Baseline native: `temp/level14-native-state.jsonl`,
  `temp/level14-native-rng.jsonl`, `temp/level14-native-replay.log`
- Unmodified visual control: `temp/level14-visual-control-state.jsonl.gz`,
  `temp/level14-visual-control-replay.log`
- Restored ordinary-acquisition experiment: `temp/level14-rendered-state.jsonl.gz`,
  `temp/level14-rendered-replay.log`
- Imported baseline: `temp/level14-imported-state.jsonl.gz`,
  `temp/level14-imported-replay.log`
- Baseline native and visual-control state trace SHA-256:
  `09d1181a3fbb2027ce4918e66255ff4d4d65b2c0698c6efca2659a0cf699ef75`
- Temporary `d1/main/laser.c` edit reverted and executable rebuilt. The source
  matches its pre-investigation backup byte for byte (SHA-256
  `a73c782161a21d0dc0e2570976b18a3d62018f8f3605ca158ee52ec6aa0cda88`)

Runner arguments common to native runs:

```powershell
.\android\tests\run_input_demo_replay.ps1 `
    -DemoPath android/regression_demos/d1_descent_level14_20260920_184354.dximdemo `
    -Game d1 -Runner visual -Mode accelerated -RenderProfile default `
    -ReplayRobotLabels hide -StateLogPath temp/level14-visual-control-state.jsonl `
    -KeepSandbox
```

Use `-Runner fast` for the native no-render control. For imported playback,
replace `-Game d1` with `-D1InD2` and use a separate state-log path


## Compatibility repairs on 2026-09-21

The follow-up isolates four ordinary gameplay differences and corrects them in
`d2/main/d1_in_d2/`, with small dispatches at existing engine operation boundaries

1. **Foreign projectile collision traversal.** D1 considers projectiles from
   different shooters unrelated; D2 excludes most such pairs from FVI. At
   processing frame 473, native laser 24 hits another projectile and continues
   through a second fixed-point movement step. The D2 shortcut skips that step,
   changing its rounded endpoint. A later missile explosion at frame 1518 then
   pushes the player by a slightly different amount, producing the first velocity
   mismatch at state frame 1519 and the eventual visible angle drift. Restoring
   D1's relationship rules fixes the original drift without changing physics math
2. **Released stuck flares.** Native door cleanup gives a released flare a quarter
   second, versus D2's eighth second. D1 also retires obsolete registry entries
   without shortening the object's life. Preserve both native rules in the
   semantics owner; the earlier flare expiration/allocation discrepancy disappears
3. **Vulcan pickup quantity.** A duplicate D1 Vulcan grants one ammo box, not all
   ammo remaining inside the weapon pickup. First acquisition retains D1's minimum
   allocation and competitive LowVulcan rule. This removes the frame-2097 ammo gap
4. **Factory robot startup.** D1 retains the mode set while constructing a spawned
   robot's exit path, except that toasters run away. D2 resets the mode from the
   robot's behavior. Robot 33 (signature 2182) consequently chased rather than
   followed its exit path in the imported run. Upon leaving the factory, processing
   frame 5281 skipped one path-following turn: its firing dot product was 57326
   instead of native 57395, straddling the 57344 threshold. The resulting AI/motion
   difference eventually affected the player's shields and orientation. Preserve
   the native startup mode in the AI owner

### Verification and limits

- `temp/level14-fixed-state.jsonl` matches `temp/level14-native-state.jsonl` for
  every canonical frame `state` object across all 5,696 frames, including all
  player fields, position, forward vector, time and level summary
- The robot, weapon and fireball state hashes, object signature seed, live object
  count and runtime state hash also match on every frame. This is stronger evidence
  than final-result equality, but is not a complete comparison of every internal
  struct, all RNG events, full orientation matrices or presentation output
- Added real projectile/FVI, factory creation, stuck-flare cleanup and loaded-asset
  Vulcan pickup coverage; native D1 and ordinary D2 are exercised alongside the
  imported profile
- The unchanged recording's expected result still fails in both engines because
  of the separately demonstrated replay-only homing acquisition bug. These repairs
  establish imported/native replay parity, not original-recording acceptance
- No recording, expected result, RNG seed or replay correction was changed. All
  temporary engine probes were removed. Small targeted evidence is retained in
  `temp/level14-physics-probe/` and the sandbox `late-ai.txt` files


Final checks:

- Host builds: D1, D2 and D2 headless executables plus both compatibility test targets
- CTest: 51/51 D1 and 59/59 D2 passed
- Robot-frame comparison: 1,260 scenarios / 5,040 native/imported frames passed
- Loaded-asset gameplay comparison passed, including all six added Vulcan pickup
  cases and ordinary D2 rule assertions
- Existing imported regression runner: levels 5, 15, 16 and 18 passed unchanged
  (`temp/level14-imported-corpus.log`). These are existing runner verdicts, not new
  strict per-frame or presentation-fidelity claims
- Level-14 comparison report: `temp/level14-native-imported-comparison.json` checks
  equal trace lengths, sequential frame indexes, exact canonical states and all six
  selected diagnostics. Both traces contain exactly 5,696 frames
- Scoped code-quality invocation and `git diff --check` passed. The quality script
  excludes these C/C++ paths from formatting; original-file style was retained
