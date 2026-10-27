# Wall action scheduling and stateful flags

The later [trigger flag tranche](trigger-flags.md) supersedes this tranche's
32/64 restriction. Its reference/native and Android acceptance are recorded there;
the original scheduling evidence below retains its bounded historical scope.

## Scope

The shared OpenGL D2 runtime now schedules the previously supported wall actions:
classic actions 0..13, teleports 14, and master/enable/disable actions 31..33.
Permanent (8), alternate (16) and autoplay (128) flags are implemented alongside
the classic low three bits. Detection and resource checks remain independent of
mission names and hashes

Other action types, orientation (32) and silent (64) flags, effect-object targets,
object-attached triggers and multiplayer remain blocked. Source files must still
meet all other layout, capacity, environment and resource requirements. This
chunk does not qualify a new original mission or campaign

## Reference and live semantics

Reference revision 2576a98b5de310e3ac7c9777df2677d1627b8bae:

- main/trigger.cpp SHA-256
  74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852
- include/timer.h SHA-256
  57b2e6853def5ae0ea062056c64b88f608933402babc16b8c31498f7bd18a06b
- include/maths.h SHA-256
  43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd

Absolute time 100..900000 denotes a delay in milliseconds. Positive values are
exact; negative values choose h+h*(random%10), where h is the truncated magnitude
divided by ten. Redux uses its simulation RNG stream with that modulo policy;
matching the reference libc random sequence is not claimed

The first pending activation retains its actor, shot state, start time and
selected delay. Repeated pending activation does not restart it. Frame iteration
uses serialized order. A disabled action retains its deadline and can execute
overdue work after re-enabling. A permanent delayed action clears its operated
state for rearming; a nonpermanent delayed action clears time and disables itself
after countdown processing, even without the one-shot flag

Alternation and one-shot changes happen after action dispatch. Immediate autoplay
runs once and disables itself; immediate permanent autoplay is suppressed.
Delayed permanent autoplay can rearm once top-level debounce permits another
activation. Eligible nonplayer actors retain their original identity when a
delayed master later dispatches its children

Classic player wall entry retains recording and route-confirmation gates before
the scheduler callback. Stock missions use the original dispatch and timer paths

## Persistence

XL saves append a bounded DXXLTR01 block after native runtime state and before
launcher/mod metadata. The block stores elapsed simulation ticks, selected delay,
actor, shot and operated state for each trigger, with an authored-trigger
fingerprint and payload checksum. Parsing checks bounds and semantics before
publishing any scheduler rows. Stock saves receive no additional bytes

The same I/O path accepts Android rewind memory streams. Disk save restoration,
memory checkpoints and source transitions require separate runtime evidence

Elapsed state is bounded by the native save epoch limit and rebased to the
restored simulation clock, including an earlier or zero epoch. It is not rejected
merely because the restore clock is smaller than the saved elapsed duration

Two pinned-reference edge cases are corrected deliberately: its save loader
retains operated time but loses the selected delay, and its positive-timestamp
countdown test can strand an activation at game time zero. Redux preserves the
selected delay and represents operated state explicitly. Live timing comparisons
are scoped separately from these persistence and zero-epoch corrections

## Native evidence

The extracted oracle compiles the original arming block, IsDelayed, Delay and
Countdown with storage adapters. It matches 130 exported runtime arming/timing
samples and independently checks ten INT_MIN controls outside the delay interval.
The comparison covers deadline arithmetic and selected random delays; it is not
an end-to-end reference-engine campaign comparison

Native controls exercise repeated pending activation, exact deadlines, permanent
rearming, disabled overdue work, random selection and simulation RNG call counts,
positive/random pending source save restoration, stock transition, source identity
and corruption rejection, classic and enable/disable alternation, autoplay
variants, serialized frame ordering and zero-epoch activation

Dead-player teleport controls verify that motion is skipped while the ordinary
post-action one-shot flags still apply. Native SIM diagnostics reset their call
count on full restore; the checks preserve that policy, verify the restored seed
and prove that the scheduling reader consumes no new random value

Fresh source normalization checks now cover 72 version/value/flag combinations,
including the reference's removal of permanent from positive modern autoplay
masters. Browser controls distinguish supported scheduling from unsupported
actions and flags on otherwise similar records

The final focused regression selection passed 8/8, including these edge controls,
immediate chains/teleports, modern water/properties, original Neptune and
Observatory descriptor loading. The reference samples exercise all ten random
selection residues. Native compiler output introduced no warnings

Original geometry HOG SHA-256:
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303

Native derived HOG SHA-256:
100dde0285ddeec3c8919e22dfce251b142feecc7b173241efe71d92bf041ddc

Android derived HOG SHA-256, with longer timers to observe persistence:
4d7a893899f28ffca4e8342e551075ab007399720256c2a1c60c75087755a473

Derived descriptor SHA-256:
5f4cfed456b34db7c78b9db0d743f4f72c54b37e5b133db32f7b7756ee8710db

## Device evidence and outstanding work

The initial Android run passed 62/62 steps, run ID
246f1d87a1d84df3a8f57f45577aca84. It covers pending disk restoration, stock-to-XL
source restoration, saved alternate type, random autoplay, disabled overdue work
and re-enabling. The runner restores pre-test app-private file hashes

The expanded 70-step integration adds production memory-checkpoint captures and
three-round rewind restoration for positive and randomized pending timers. The
final APK passed 70/70, run ID 3bad9be437c045b5b1817308d14b582b, in 221033 ms.
Both capture-frame reference comparisons and all six rewind restores passed.
Pre-test and restored app-private file hashes match exactly. Only emulator-5554
was used

Final tested APK SHA-256:
252ce332f27f0b14e97eb35c3dc9736aa485604f151e8c4b436ab1b684239430

Logs live under temp/xl-trigger-scheduler-20261026 and
temp/xl-wall-trigger-scheduler-android. The Android x86_64 build and scoped
formatting/lint passed

The trusted original-fixture CI runner requires xl_wall_trigger_scheduler using
its pinned Neptune package. Its receipt now requires 42 established native tests.
Both automation catalog checks and the 12 runner-policy tests passed locally

Remaining gameplay work includes object event activation, boosts and spawn state,
other segment/wall/generator semantics, additional object layouts and custom
gameplay data, followed by combined original-mission qualification
