# D1 fan transparency fault demos

Three ordinary-start, input-only demos use the pinned Descent 1 v1.4 shareware
assets. Each starts level 1 on trainee difficulty and runs 324 frames at 16 Hz.
No saves, warps, pose/world edits, inventory grants or cheats are included.
These are fixed-timestep captures through native input consumers, not full SDL
event-loop recordings. Each input and each separate oracle is below 100000 bytes.

| Demo                  | Input bytes | Expected imported result                                          |
| --------------------- | ----------: | ----------------------------------------------------------------- |
| `d1-fan-transparency` |       56604 | Flare hits fan; strict carried-RNG abort before input 301         |
| `d1-fan-no-fire`      |       56570 | No flare; completes 324 frames                                    |
| `d1-fan-delay`        |       56604 | One-frame-later flare stops in both engines; completes 324 frames |

This separate diagnostic corpus leaves the existing passing public and retail
suites unchanged. `public_replay_admission: false` is deliberate. Reproducing a
known fault does not qualify full imported-D1 parity or turn an aborted replay
into a successful terminal result.

## First meaningful gameplay divergence

The route reaches fan segment 135 at frame 219. All flight inputs are released
at the common arm frame 299; the positive demo requests one flare then.
During SIM 300, flare slot 77/signature 179 crosses side 4, wall 5, in native D1
and enters child segment 142. Imported D1 stops that same flare in segment 135:

- Native position `[8216206, -2905300, 30439592]`, velocity `[129480, 4201080, 6646920]`
- Imported position `[8208474, -3156161, 30042682]`, velocity `[0, 0, 0]`

At the exact imported impact point, native fixed-point UV is `[-17056, 35286]`
and the sampled texel is `(47, 34)`. Native effect 3 selects fan frame 0/bitmap
823, whose merged pixel is 255 and whose native transparency test permits passage.
Imported D1 selects frame 1/bitmap 824, yielding opaque pixel 59. The wall uses
base texture 83 and overlay 329; the flare radius is 26709. Read-only native
lookups of both phases at this exact point establish the phase-sensitive mask
change independently of the delayed shot's geometry.

Native `d1/main/effects.c::init_special_effects` initializes animation timers to
one frame duration. The D1 asset import retains the serialized zero timer;
imported animation consequently advances one frame ahead. Animation runs before
object physics, and the point-transparency collision path consumes the current
merged bitmap. The initial timer difference is already visible at frame 0
(native 3276 versus imported 0); this demo shows its collision consequence.

The imported wall collision consumes awareness RNG call 346 during SIM 300,
changing state 2301841099 to 2296157864. The unchanged carried-RNG guard stops
before input 301. The native run consumes this draw later. All 301 shared
summary gameplay/frame-RNG entries still match, so a summary-only prefix check
would miss the physical difference. The detailed gate requires the actual
collision, exact phase/trajectory, extra draw, and strict abort together.

### Boundary alignment and raw differences

Trace entry `fN` is after ordered input N and clock advancement, before SIM N's
animation/physics. Native entry 301 follows neutral input 301; imported aborted
boundary 301 is before input 301. Their flare/effect states describe the end of
SIM 300, but their clocks deliberately differ. No clock or RNG state is restored.

Raw parity fails from startup, including player signature 1 versus 0, animation
phase and engine metadata. Each weapon's parent signature is checked against its
actual live player. Only that bound player-context signature difference is
permitted in the separately named SIM RNG value-prefix comparison; raw RNG
context and full diagnostic comparisons remain failed and retain every mismatch
in their declared scopes. No general identity normalization is used.

## Causal controls and checker negatives

Decoded input comparison proves that no-fire only omits flare pulse 299, and
delay only moves that pulse from 299 to 300. Both actual imported controls finish
all 324 frames with matching gameplay/RNG values and expected terminal states.
Both delayed flares stop at `[8207005, -3163843, 30042452]` during SIM 301.

Delay is an input-timing intervention, not a frozen-geometry phase-only test:
ordinary ship momentum changes the muzzle and samples texel `(46, 34)` instead.
Both native frame 1 and imported frame 2 are opaque there. The same-frame positive
pair and exact-impact-point native mask observations supply the phase evidence.

The final real control executions are rejected by the positive fault gate.
Additional clearly labelled in-memory mutations of the verified decoded positive
trace reject an absent physical fault, wrong phase and wrong collision magnitude.
They do not replace the actual no-fire/delay captures or alter raw evidence.

Full native recordings, RNG, compact inputs and summary oracles are byte-identical
with screenshots and both pixel observers jointly enabled versus jointly disabled.
The switches were tested jointly, not independently. The sparse capture-observation
log contains combat events and a 600-frame cap, not a complete 324-frame trace.

## Reproduce and check

Build the required native fixture/game/console targets from retained source
`03f9c85239dbbaeae668693bb82186452179bbd8` on branch
`capture-public-d1-fan-transparency`. The capture helper checks clean source,
successful build exit, compiled source identity, assets and executable hashes
before invoking the fixture. From the repository root, with a working display
or Xvfb:

```sh
python3 android/tests/capture_public_d1_fault_probes.py \
  --repo "$PWD" --build "$D1_BUILD" --assets "$D1_DATA" \
  --output "$OUTPUT/new-positive" \
  --revision 03f9c85239dbbaeae668693bb82186452179bbd8 \
  --scenario fan-transparency --pack --screenshots
```

Use fresh output directories. Add `--no-flare` or `--flare-delay 1` for controls.
Do not generate native expectations from imported playback. Source-capture
provenance and full raw input hashes are recorded under `provenance/`.

For native playback, use `run_input_demo_replay.ps1` with `-Game d1`, the matching
D1 executable, `-DataEdition shareware`, `-DataDir "$D1_DATA"`, and either
`-Runner headless-console` or `-Runner visual`. Save state to `.jsonl.gz`, RNG to
`.jsonl`, and the result separately. The recorded nine command lines and expected
process exits are in `provenance/replay-manifest.json`.

Imported playback requires the explicit native-written `d1match` comparison
pilot. The D2 fixture's `--public-d1-comparison-profile` writes/reads it natively
and verifies only missile camera preference 1 -> 0 changes at creation. The
verified generated profile files are included in `profile/`. Copy `d1match.plr`
and `.plx` to a fresh `$PROFILE_HOME/.d2x-redux/`, then use `-Game d2 -D1InD2
-Pilot d1match -Runner visual` with inherited `HOME="$PROFILE_HOME"`. Place the
HOME assignment after an Xvfb wrapper that resets HOME. Production defaults and
strict projection checks remain unchanged. The console runtime does not load
pilots, so imported headless playback is not qualified here.

Use `check_public_fan_transparency.py compare` for the positive and
`compare-control` for controls. Both require the unchanged compact input,
native-only detail oracle, native/imported state and RNG logs, native result,
actual imported run log and exit code. Positive additionally requires
`--imported-probe` from `-ReplayDebugLog`; it must have exit 1, exact carried-RNG
abort 301 and no terminal result. Controls require exit 0 and `--imported-result`.
Use the `causal` subcommand to compare all three decoded input sequences.
`--help` lists exact paths/options. A generic runner failure is never a fault pass.

`test_public_d1_fault_tools.ps1` runs offline adversarial checker and immutable
corpus admission tests. It does not execute the games. Nine real executions cover
native true console, native headed and imported headed. Native console/headed
state, RNG and result bytes are identical; all six detailed pair comparisons
match their explicit fault/control expectations. The independent audit passed
133 checks. Existing retail/public cases and earlier fault probes remain intact.

## Evidence

- [Native frame 299](screenshots/d1-fan-flare-frame299.png) and
  [native frame 307](screenshots/d1-fan-flare-frame307.png): uncropped 640x480 Xvfb
  screenshots. These are native route/capture images, not paired imported images
- `evidence/`: complete bounded pair reports, native live observations, causal
  comparison, rejection evidence, strict abort log and exact collision-row excerpt
- `manifest.json`: byte hashes and sizes, including full decompressed hashes for
  lossless gzip evidence. Pair-report `oracle_sha256` hashes canonical logical
  JSON; oracle file-byte hashes are recorded separately
- Full raw recordings, world/RNG traces, collision log and remaining screenshots
  are retained unchanged at the capture workspace paths in the manifest. The
  excerpt is explicitly a subset; no full trace has been replaced by it

No engine repair is included. This branch documents a specific animation-phase
collision fault while keeping broader imported-D1 fidelity unqualified.
