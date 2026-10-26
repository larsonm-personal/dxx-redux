# Wall trigger scheduling and stateful flags

## Goal and acceptance

Continue the singleplayer roadmap with delayed wall actions and the permanent,
alternate and autoplay flags on already-supported actions. Preserve detection,
the shared binary and rejection of unsupported action/flag/object requirements

1. Audit pinned Operate, Delay, Countdown, frame iteration, enable/disable and
   source/save initialization, including randomized delay selection and ordering
2. Route all supported wall actions through one XL scheduler and dispatch scope;
   retain the stock callback for classic leaf behavior and stock isolation
3. Persist actor, activation time, selected delay and repeat state in a bounded,
   validated XL-only extension that also works through Android rewind I/O
4. Exercise boundary timing, repeated activation, alternating children, autoplay,
   disabling/re-enabling pending work, frame ordering, recursion and actor rules
5. Verify pending source save/restore, stock transitions, malformed persistence,
   simulation RNG and independent reference timing/flag controls
6. Run native and registered Android integrations, scoped formatting/lint and
   catalog checks; update evidence and the roadmap before publishing the chunk

## Source findings

Pinned source: 2576a98b5de310e3ac7c9777df2677d1627b8bae, main/trigger.cpp
SHA-256 74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852

Delay applies at absolute time 100..900000 ms, excluding countdown/message/sound
actions. Positive delays are exact; negative delays choose h+h*(rand()%10),
where h is the truncated absolute time divided by ten. The first activation
retains its actor and shot state. Repeated pending activations do not restart it

Frame processing visits triggers in serialized order. Disabled triggers do not
count down. Once due, a permanent trigger clears operated state for rearming;
other delayed triggers clear time and become disabled. Disabling suspends a
pending deadline rather than moving it; re-enabling can therefore fire overdue
work on the next frame. Enable on a permanent enable action resets its own
operated time, as in the pinned source

Autoplay starts eligible unoperated triggers. Immediate permanent autoplay is
suppressed; delayed permanent autoplay rearms on a later frame. Alternation is
post-action, using the reference type-pair table. One-shot disabling is also
post-action, including when an action is reached through a chain

The reference save loader retains operated time but resets its selected delay
slot, which can lose a pending randomized delay. Redux's new state must preserve
the selected delay and remaining simulation time across restore; document this
save-format correction separately from reference live-dispatch comparisons

No autonomous 3D navigation or whole-mission completion claim is part of these
controlled feature tests. The overall gameplay roadmap remains open

## Completion audit

1. The source audit and independent oracle cover arming, randomized selection,
   delay boundaries and countdown completion. All 130 reference timing samples
   and ten separate INT_MIN controls pass, covering all ten random residues
2. Supported classic and extended wall actions share dispatch and scheduling.
   Classic player entry retains recording and route-confirmation gates; stock
   dispatch and countdown behavior remain selected without a loaded XL source
3. The bounded XL-only state block validates source identity, checksum, row/field
   ranges and selected delays before publishing state. Tests cover missing,
   truncated, oversized, corrupt, overflowing and wrong-source data, plus restore
   into an earlier epoch and zero-epoch activation
4. Controls cover ordered frame iteration, autoplay variants, alternation,
   one-shot/permanent interaction, first actor retention, a queued nonplayer
   master and dead-player post-action flags. The final native selection passes 8/8
5. Disk tests preserve remaining positive and random delays and simulation RNG
   selection. The Android memory test compares capture-frame reference bytes and
   performs three rewind rounds for each timer profile. Full native restore
   retains its existing SIM diagnostic-count reset policy
6. The final Android APK passes 70/70 steps, with exact app-private restoration.
   The x86_64 build, scoped formatting/lint, both automation catalog checks and
   all 12 runner-policy tests pass. Trusted CI now requires the scheduler CTest

See [maintained evidence](../../XL_compat/evidence/trigger-scheduler.md) for hashes,
run IDs and scope. This completes the wall scheduling/flag chunk; the overall
roadmap remains active. Object event activation is next, and no new original
mission or campaign is qualified by these derived controls
