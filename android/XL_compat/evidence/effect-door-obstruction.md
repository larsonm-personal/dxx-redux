# XL effect objects do not obstruct doors

The final audio/seismic audit found that XL's door owner still called Redux's
obstruction filter. That filter excludes weapons and fireballs, but treats XL
static explosions (19) and sound-effect objects (20) as physical obstructions.
Both types are already admitted. A positive-size effect across a portal could
therefore prevent authored closing, keep an automatic door waiting, or reopen a
closing door.

Adding those types to the existing native actor matrix reproduced the defect:
the unchanged production implementation failed with `Two-sided actor obstruction
differs` and exit code 1. The corrected implementation passes that same assertion.
The before/after logs are under `temp/xl-seismic-20261030`, named
`effect-door-negative-control.log` and `effect-door-corrected-control.log`.

## Source and correction

Pinned reference remains `arbruijn/d2x-xl` revision
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The existing source adapter now also
extracts and executes `CSegment::DoorIsBlocked`. Consumed hashes are:

- `main/segment.cpp`: `eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d`
- `include/object.h`: `ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127`

Object constants come from the pinned header. Eighty-four source cases cover
types 0-20, overlap/no overlap and the ignore-marker switch. Geometry overlap is
an explicit source-adapter input; native and Android controls exercise actual
portal geometry separately. Existing 3,072 transition/allocation and 660 animation
comparisons remain unchanged.

The correction is entirely inside `android/XL_compat/runtime/doors.cpp`. XL skips
weapons, fireballs, static explosions and effects. Players, robots, pickups and
markers still obstruct. It preserves the existing intentional two-sided geometry
correction: the pinned source only walks the front segment's object list.
Redux's function and all D1/D2 core files remain unchanged by this increment.
Ignore-marker flags and unsupported object shapes remain outside admission.

## Runtime controls and retained failures

Shared native/Android controls exercise eight actor types on both sides during
authored close, automatic waiting and closing/reopening: 48 cases. They retain
ordinary-actor positive controls and restore the fixture's wall, side, player,
active-door and frame state. The existing full door-lifecycle owner retains its
90/91 allocation boundary, reuse, compaction, disk/rewind and stock isolation.

The Android debug build caught an invalid test weapon created with `CT_NONE`.
The fixture now supplies `CT_WEAPON`; production assertions are unchanged.
The failed process's protected files were restored exactly before rerunning.

The next Android run passed the 48 obstruction cases, then exposed a separate
fixture assumption: simulation-only overlay pause does not freeze independent
audio callbacks. The paired synchronous/detached checkpoint comparison differed
in voice positions. Preparation now holds XL audio until a normal running frame
resumes it, retaining the exact byte comparison. This establishes controlled
capture consistency, not the intended sound-tail policy of the pause overlay.

The door-lifecycle fixture pin is refreshed to
`e7e8a12cd6c4495250b4617c9900af1bb44e6cb4dc16b2d5240435a444db801d`.
The old pin is exactly reproduced by reversing six inherited bytes: two wall-key
bytes and a swap of two equipment-generator masks. Door geometry and the added
91-door table are unchanged. Descriptor SHA-256 remains
`e32d601e2f8c4a6f8a34a3206b760a7bea2a78c9007fae87e35ddcdd58a3bfde`.

The older `test_xl_close_door_runtime` owner was also tried. It fails before the
new controls because its motion-audio assertions read Redux playback counters
while XL now owns those voices. Its pinned generated fixture is also stale.
That runner remains an explicit follow-up; this increment does not remove its
assertions or count its failed execution as a pass. The current door-lifecycle
owner is the Android regression owner for this correction.

Windows and focused Linux door controls pass, including the final valid weapon
construction. The final Linux door test takes 22.02 seconds and retains all
transition, animation, persistence and source-filter comparisons. The complete
Linux selection passes 105 of 106 tests; the audio owner's existing scope child
times out during concurrent local workloads. A retry in that interrupted workspace detects four
leftover injected `arnold_1.wav` files and correctly rejects the nonempty classic
baseline. A fresh test-owned directory is used for final audio acceptance; neither
failure is counted as a pass. The complete unchanged audio owner then passes in
the fresh directory, including scopes, banks, static effects and both seismic
layouts. These are a 105-pass full selection plus a successful isolated audio
rerun, not a claim that the initial full-run receipt was green.

Android passes all 51 door-lifecycle steps in 116.737 seconds, run
`268ed305e7fa410787d1e490ed623e08`, on APK SHA-256
`4c9427c9d43867d47f9d70532532bb8a23fcd13c03f27d10d1f4cede730738b8`.
The 48 obstruction cases, paused audio capture, exact detached/synchronous
checkpoint bytes, normal audio resume, disk restoration, full door table, reuse
and stock transitions all pass. Protected files restore unchanged. The receipt
and full execution log are `effect-door-android-receipt.json` and
`effect-door-lifecycle-android-capture.log` under the artifact directory above.
Windows control binary SHA-256 is
`2f74b5920b196be5a188e245280a29749873e671836a733338f6f9224180a476`.

Both automation catalogs, scoped formatting and configured PowerShell lint pass.
No timeout or assertion has been relaxed. Final published-head CI remains a
separate gate. This correction does not qualify an original campaign or change
mission admission.
