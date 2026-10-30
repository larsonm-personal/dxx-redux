# Owned XL seismic gameplay

Wall and supported attached type-36 actions now start XL-owned mine disturbances.
Earthshaker impacts retain their positions for distance-scaled rocking. Player
and companion rotation, tremor magnitude, rumble ownership and saved continuation
use the XL path; stock Redux keeps its existing alternate implementation.

The compatibility viewer counts wall and attached actions in a dedicated row.
Authored `time` is whole seconds, not scheduler milliseconds. Durations above
32767 seconds reject before loading or mutation with a seismic-specific error.
The row remains partial because owner, resource, settings and original-campaign
qualification are separate requirements. No mission-name exception is used.

## Reference semantics and explicit boundaries

Reference revision is `arbruijn/d2x-xl`
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The existing named-audio source owner
extracts and executes the actual shake action, disturbance, Earthshaker, rocking
and sound-cadence functions. Source inputs are hash-checked:

| File                  | SHA-256                                                            |
| --------------------- | ------------------------------------------------------------------ |
| `weapons/seismic.cpp` | `2c0e2c45cfef16b7713d63ff4a92368720733405613af929867ec433dd49bacc` |
| `main/trigger.cpp`    | `74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852` |
| `include/maths.h`     | `43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd` |

The 23,100 comparisons cover negative/default/zero/high strength and durations,
retriggering, disabling, four detonation slots, pressure/expiry, tick gating,
player and companion rotations, magnitude, sound requests and random-draw
consumption. Companion impulses are four times the player's. Distance inputs
include 0, 199, 200, 400 and 2000 units. The seismic source adapter supplies the
distance result; it does not independently execute vector magnitude. Production
uses the separately source-verified XL default geometry helper, whose magnitude
and normalization owner covers 20,344 cases. Runtime controls include an oblique
distance to distinguish that helper from Redux integer magnitude.

XL starts an eligible pending disturbance immediately, consumes the duration
once, and uses raw fixed-point distance from the interval midpoint before the
source 1..16 clamp. It does not use Redux's random starts or midpoint scaling.
Negative action strength clamps to zero; negative duration selects ten seconds.
A retrigger changes strength immediately and queues a duration for the next
eligible interval. Four Earthshaker slots last two seconds; effects at or within
200 units use full scale and more distant effects use 200/distance. Source
rounding, zero-timestamp sentinel behavior, and sound decisions based on the
previous frame's volume are retained.

The qualified profile is ordinary singleplayer, default floating geometry and
the source 40 Hz ceiling. The existing saved simulation-clock policy replaces
process-global wall-clock jitter: phase and fractional milliseconds accumulate
between frames, and a tick discards overshoot. Pause consumes no time or random
draws. Simulation rocking uses the SIM stream and audio cadence uses FX, following
the existing engine determinism policy rather than sharing XL's global RNG.
Wide engine time and explicit wrapping rotation addition avoid source integer
overflow. Alternate timing/math/nostalgia settings and multiplayer remain open.

## Runtime and persistence evidence

The labeled `xseismic` fixture derives canonical 23/40 and 27/40 levels from the
existing Neptune geometry. Eight wall and two attached actions exercise actual
player wall contact, master dispatch, one-shot/disabled/autoplay behavior and
powerup removal. A 1000-second action must execute immediately rather than
entering the millisecond delay scheduler. The exact two original Pumo action
parameter sets are represented without bypassing any mission admission gate.

Controlled running frames compare applied player and real robot-companion
rotation against the source-verified model with the same SIM draws. Coverage
includes high magnitude, distance scales, slot pressure, sub-tick continuation,
queued disturbances and expiry. Existing rumble controls now exercise the XL
physics path and compare actual mixed PCM, including stale-channel recovery,
retired channels and protection of unrelated voices. These checks drive engine
events; they do not attempt autonomous navigation.

Trailer schema 25 adds strength, pending duration, active interval, four
timestamp/scale pairs, magnitude, cadence phase and fractional remainder. Rumble
ownership and volume cadence remain in the shared named-audio state. Relative
times rebase against the 64-bit save epoch; a control restores beyond the native
32-bit time range. XL's legacy common seismic save record is inert because it
cannot represent the complete XL state. Stock serialization remains unchanged.

Fourteen malformed physics states, eight malformed full trailers and the existing
eight malformed shared-audio fields reject atomically. Native tests compare every
owned trailer byte after rejected full-trailer restores. Disk and Android rewind
controls earn active and pending disturbances, distance-scaled detonation, wide
magnitude and partial cadence before capture. They deliberately change state,
restore it exactly, then run through queued completion and expiry. Normal level
transitions clear old intervals and detonations; stock transitions leave XL
entry points inert and restore the earned XL state on return.

Final gameplay code at `fe1d3b34c` passes the Windows headless build and both
native seismic layouts, including oblique distance and malformed trailers.
The full Windows audio owner also passes, including ordinary replacements,
mission banks and static effects. An earlier full Linux acceptance run passed
all 106 required tests. The final GCC build, source/inventory checks and full
audio owner pass; the latter takes 135.19 seconds within its unchanged 150-second
limit. One intervening run timed out in the existing 45-second scope stage under
concurrent local workloads. A diagnostic run completed with all assertions,
and the final complete owner passes without increasing either bound.

Native audio fixtures now respect the low-level save function's ownership of
the caller's pause: the caller stops time and the save resumes it. Redundant
resumes and missing stops had produced negative pause depth in native controls.
The corrected tests assert that simulation is running after saves. Android's
production save wrappers are unchanged.

Android passes 64/64 seismic steps on APK
`bf28eaf7fab71764d3c3f0912d2b51757addc7598e65ba766afe5b8538949fa3`:
run `4e503af6594840b587d2aafe798c56f7`, 198.836 seconds, two authored-event
groups, 20 state checks, 14 actual PCM comparisons and 129,024 samples at 48 kHz.
All protected files are restored byte-for-byte. The broader named-audio Android
regression also passes 145/145 steps on the same APK: run
`1b27555ef3424d6588cc9449e374cc03`, 487.639 seconds, 145 output comparisons and
1,787,392 samples at 48 kHz, including six seismic comparisons alongside combat,
actor, afterburner, routing, ambient, linked and shared-channel controls. Its
protected files also restore unchanged.

Reproduction uses `xl_named_audio_source` and `xl_named_audio_runtime` under
CTest, native `-named-audio-test seismic -audio-level 1|2`, and
`android/tests/test_xl_named_audio.ps1 -Seismic` with the generated `xseismic`
HOG/descriptor. The default PowerShell mode runs the existing named-audio
regression and the seismic script, using sibling generated fixture files;
`-NamedOnly` and `-Seismic` select focused reruns. The combined-owner master
timeout is 900 seconds, while each script retains its existing bound. Both
scripts belong to the existing test owner; no duplicate CI lane is added.
The default combined owner passes 145 + 64 steps: named run
`516d053af5db42d39e8bdb4577b13b2a` in 459.173 seconds and seismic run
`e6fddd3abdf64246840ab53ee5e44fac` in 181.705 seconds. Each protected-state
restoration passes. Subsequent runner artifacts use separate `xsound` and
`xseismic` subdirectories so the second script cannot overwrite the first
receipt. Local acceptance receipts are under `temp/xl-seismic-20261030`.

## Original missions and unfinished work

[The original inventory](seismic-originals.json) records all fourteen Pumo levels
from unchanged HOG SHA-256
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`.
Only `tulev1.rl2` changes admission: records 63 and 66 are type 36, flags 3,
strength 196608 and duration 3, with no targets. Its runtime blocker count drops
from 17 to 15; particles and wider wall flags still block it. Thirteen of the
fourteen levels retain inventory blockers. `mvend.rl2` has none but still needs
resource/runtime qualification. No Pumo campaign playability claim follows.

Remaining work includes alternate source settings, durations outside the admitted
range, other object-owner lifecycle variants, and combined original-mission
qualification. Shared-audio admission and all independent effect, wall, segment
and custom-resource requirements remain in force.
