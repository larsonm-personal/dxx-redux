# XL slow-motion core, stage one

Implement the pinned default-speed state machine independently of engine admission.
Retain separate world/player factors, cached active-query results, strict elapsed
25 ms update boundary, one-step ramp, toggle precedence, energy debit and ordered
audio/message requests. Do not replace FrameTime globally. Classic, multiplayer
and nostalgia paths remain inert. Reject unqualified state/input before mutation.

Compare every output with actual extracted pinned slowmotion.cpp functions and
separately cover classic isolation, invalid input, long and interrupted ramps,
energy thresholds, cheat flags, reactor transitions, sound failures and state
copy/replay. Register the public owner without changing private FAST membership.
Run public source/GPU/audio controls, scoped quality, Python owner negatives and
catalog validation. Do not admit powerups48/49 or claim rendered readiness.

Remaining stages: pickup and duplicate behavior, genuine addon sprite rendering,
per-consumer world/player timing integration, controls, actual audio lifecycle,
level reset, disk save/restore and unchanged rendered original qualification.
The asset worker located the genuine sprites in textures/D2; await verified asset
revision. This core only emits audio requests; it does not play or reconfigure audio.

## Qualified boundary and evidence

Default ship only: energy -1 through 200 fixed units, difficulty0..4, frame delta
0..65536, monotonic nonnegative timestamps, speed1..3 and direction-1..1. Invalid
inputs are rejected before state/effect mutation. Cached active flags retain the
reference query semantics. A synchronous nonthrowing effect sink returns actual
playback success; no sink records an unavailable channel. Core output is an audit
of delivered requests, not a second dispatch queue. Engine playback, clock callers
and disk persistence are deliberately not connected in this stage.

98,400 extracted-source comparisons pass, including 12 long trajectories, strict
25/26ms boundary, interrupted ramps, both controls, all equipment combinations,
energy exhaustion, cheat, reactor shutdown, failed audio, repeated state-copy
replay and classic/network/nostalgia isolation. ASan/UBSan comparison run passes.
Public owner55 controls and Python owner33 tests pass; private FAST remains113.
Catalog validation passes; catalog execution hits the existing unavailable Linux
/proc/self/task/.../children interface (exit125). No admission/cohort changes.

The asset worker has supplied draft assets PR4 at
50f02a520482a859a7812bb2bcdaacb07c745d94: overlay paths
xl-time-pickups/textures/d2/slowmotion#0.png and bullettime#0.png, each256x4096 RGBA
(sixteen256px frames). These bytes are for the later renderer stage; this core
performs no asset lookup and makes no new rendered-readiness claim.
