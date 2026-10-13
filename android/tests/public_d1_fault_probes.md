# Public D1 fault probes

These test-only recipes exercise native D1 and D1 imported into D2 with the pinned
Descent 1 v1.4 shareware assets. They start at the authored level spawn and use
ordinary player controls. They do not move objects directly, alter inventory,
install saves, or use cheats. Existing public and retail gates remain separate.

## Capture and evidence

For qualifying candidates, use `capture_public_d1_fault_probes.py` with the exact
clean source revision, an independently configured native D1 build directory,
pinned assets, and a new output directory. It reconfigures recorder metadata,
requires a successful fixture build, verifies source and executable identity,
and only then starts capture. Build failure must never fall through to an older
executable. A successful capture is still only a candidate for admission.

```sh
python3 android/tests/capture_public_d1_fault_probes.py \
  --repo "$PWD" --build "$PWD/temp/native-d1-build" \
  --assets /path/to/pinned/d1 --output "$PWD/temp/new-fault-capture" \
  --revision FULL_PUBLISHED_SOURCE_COMMIT --scenario effect-phase \
  --pack --screenshots
```

The helper retains a manifest, configure/build/capture logs, source and binary
hashes, and full raw evidence. `--no-missile` creates the independent causal
control for missile scenarios. Optional `--gzip-raw` creates lossless archival
copies only after successful packing and retains the originals. The separate
effect and mine checkers must still examine their detailed observations.

The native `test_upstream_compat` executable accepts:

- `--public-level-replay <d1-data> missile-self-splash`: fly toward a nearby solid
  wall, aim, and fire one of the ordinary starting concussions
- `--public-effect-phase <d1-data>`: fly to the early level-one monitor at segment
  11, hold position, and face side 0
- `--public-level-replay <d1-data> mine-blast`: start level three on the ordinary
  trainee difficulty, collect an authored proximity pickup, clear nearby threats
  with lasers, drop a mine, strafe, and fire a concussion at solid geometry

`DXX_PUBLIC_CAPTURE_NO_MISSILE=1` records the same missile scenario without its
concussion firing input. This is a live causal control with its own carried RNG,
not a positive recording with mismatching RNG fields edited afterward.
`DXX_PUBLIC_CAPTURE_SCREENSHOTS=1` saves uncropped renderer screenshots. A new
capture directory and isolated HOME are required on each run. Use Xvfb for the
headed capture path on a display-free Linux host.

Full live recordings, RNG traces, observation traces, capture reports and image
files are evidence. Preserve them even if the candidate fails. The ordinary
compact packer may remove approved telemetry and live state from the input file,
but its oracle must come from the independent native live capture. Every admitted
input and oracle file is limited to 100000 uncompressed bytes. Do not replace a
native oracle with the imported result to make a known fault pass.

Only a clean published source checkpoint and matching rebuilt executable qualify
for final admission. A separately compiled fixture linked against existing engine
objects is useful for exploration, but an inherited version string does not make
it a clean-source capture. Record source and executable hashes explicitly.

## Explicit imported-D1 presentation profile

Native D1 has no missile-camera preference and records that field as disabled.
Normal D2 defaults to enabled. A strict projection guard correctly rejects this
combination before gameplay. Do not remove the recorded field or disable the
guard.

The D2 fixture command `--public-d1-comparison-profile`, run in a new empty
directory, uses `new_player_config`, `write_player_file` and `read_player_file` to
create the explicit `d1match` pilot. It changes only `MissileViewEnabled` to zero,
checks native readback, requires the text profile to stay unchanged, and reports
the one changed binary byte. It also preserves the original default profile for
comparison. The helper does not change production defaults.

The independent imported monitor capture uses this same native-written and
native-read comparison pilot before starting the level. Its actual recording
header therefore declares the matched camera preference, rather than relying on
the camera being inactive during the route.

On Unix, put that named `.plr` and `.plx` in the inherited HOME's `.d2x-redux`
directory and pass `-Pilot d1match` to `run_input_demo_replay.ps1`. That wrapper
copies only the selected pilot into its fresh child HOME. Pilot loading requires
the full headed runtime; the native console backend deliberately does not load
pilots. If an Xvfb wrapper itself establishes HOME, set HOME after that wrapper.

## Known fault: animation startup phase

The real monitor uses texture 349 / eclip 24 / `misc066.abm`; the adjacent monitor
uses texture 352 / eclip 26 / `misc068.abm`. Native D1 initializes the eclip timers
from each clip's frame time. The imported level publishes a freshly zero-initialized
D1 asset generation after game initialization. `reset_special_effects` clears
effect flags and restores bitmaps, but does not reset the timer or frame count.
The first update therefore advances imported animation one frame early.

Relevant paths are `d1/main/effects.c`, `d2/main/effects.c`,
`d2/main/d1_in_d2/d1_in_d2.c`, `d2/main/d1_in_d2/d1_in_d2_assets.c`, and
`android/app/src/main/cpp/shared/effect_runtime_shared.c`. The first meaningful
difference is the raw startup timer (native eclip 24: 4369, imported: 0), before
any player movement. The next simulated state selects native bitmap 881 versus
imported 882. This is a real authored texture, not one of the intentionally
skipped shareware animation placeholders.

The capture report preserves every timer, frame index, bitmap identity and camera
state. The phase checker compares these values exactly. Its explicit known-fault
matcher verifies the particular startup-zero / subsequent-one-frame-offset
pattern; a missing fault, wrong offset, camera difference, malformed trace or
runner failure is not equivalent evidence. The ordinary comparison remains a
failure even when that known pattern is matched.

Existing replay world traces expose only a declared subset of this state:
timer, frame count, flags and selected wall bitmap. The trace checker reports that
scope and the unobserved fields rather than claiming complete asset parity. Raw
object signatures, references and all other differences remain in the retained
full evidence. Native console and headed runs can pass the phase scope while the
imported headed run fails it.

The phase change is visible on the monitor during the stable view. Collision and
transparency consequences are unproven. No engine repair is included here.

## Known fault: concussion force moves a live proximity mine

The ordinary level-three trainee route collects proximity ammunition, drops a
mine, strafes away, and fires a concussion into solid geometry. Native D1 excludes
weapon objects from radial explosion force (`d1/main/fireball.c:95-105`). D2
explicitly includes proximity mines and applies force without an imported-D1
guard (`d2/main/fireball.c:125-148`).

The exploratory matched-profile pair has 424 frames. The pickup occurs at 377,
the mine is created at 399, and the concussion is fired at 407. Mine slot 130 /
signature 323 agrees physically through raw pre-frame boundary 409. At boundary
410, after the blast during simulation frame 409, its velocity is
`[79237, -44531, 89760]` in D1 but `[-353071, 99215, -21970]` in imported D1. Both
mines remain alive. Raw positions diverge at the same boundary.

All 424 summary gameplay/RNG frames and terminal results still match. This is a
deliberate regression against a false-positive summary-only parity result: the
live object trajectory must be checked separately. A separately captured
no-concussion input retains the ordinary route and mine drop. Its paired mine
positions and velocities agree across all 25 raw samples. Both inputs remain
under 100000 bytes and carry their actual RNG histories.

The player signature and mine parent signature remain raw 1 in native D1 and 0
in imported D1. Preserve those differences and verify each parent reference binds
to the actual player in its own engine. A physically matching causal control is
not full raw identity parity. Final admission requires repeated clean-source
captures and explicit mine lifetime, identity and trajectory checks.

The mine pair also retains strict SIM event-context failures: the player actor's
`ctx_sig` is 1 in native D1 and 0 in imported D1. Algorithm values, event order,
object slots and frame-boundary carried RNG match in the observed pair. The
causal gate accepts only that explicitly bound context-signature pattern and
continues to report raw identity and strict SIM-context parity as failed. It does
not claim complete SIM event parity from matching frame-boundary RNG.
