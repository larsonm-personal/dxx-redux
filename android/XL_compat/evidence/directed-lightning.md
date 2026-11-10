# Directed lightning under the existing presentation profile

Reference: arbruijn/d2x-xl at 2576a98b5de310e3ac7c9777df2677d1627b8bae.

The unchanged Sphere level 2 `sp-02.rl2` is 1260610 bytes, SHA-256
`a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b`.
Owners 428/429 have reciprocal, unique IDs 2/1 and targets 1/2. Their original
50-byte payloads at 1133480/1133610 specify angle 4, style 1, three bolts,
30 nodes, five children, four interpolation frames, life 300/delay 30, amplitude 6,
offset 2, width 3, sound enabled and no glow. Their positions are
(22686019,6375183,43982521) and (22449866,7653026,43981970).

`effects/lightning.cpp` Setup/Init retain `m_vRefEnd` separately from the
randomized endpoint. Setup rejects sampled normalized directions until the dot
product with the normalized reference direction is at least
`65536 - angle * 65536 / 90`. For angle 4 this is 62624, not a literal four-degree
cone. The retained reference endpoint is unchanged by Move and reused by every
restart. The runtime now follows those semantics and rejects a coincident
reference endpoint before sampling. Negative angles, moving authored owners,
in-plane and waypoint profiles remain rejected.

The shared owner now follows `FindTargetPos`/`StaticFrame` when targets are absent
or repeated. Target zero skips lookup; otherwise the first live authored owner
with the matching signed ID wins, in ascending object order. Missing targets use
the emitter's current forward vector times the signed authored length. This is
still a fixed reference endpoint, including for a cone. Random angle-zero owners
retain their null endpoint. A zero-length fallback remains rejected before RNG.
Zero-bolt objects retain target identity and activation without allocating bolts,
sound owners or effects randomness. The existing native lifecycle check covers
duplicate IDs, removal before rebuilding, live orientation and anchor restore;
the existing pinned-vector comparison covers positive and negative lengths.

The pre-existing global style 0 and child-detail-off settings remain unchanged.
The reference STYLE macro caps authored style 1 at effective style 0; this does not
implement effective style 1 CreateJaggy/Scale or child bolts. Existing tail-node,
length/lifetime jitter, delay, update ordering and rendering paths are retained.

The extracted-source oracle covers cone thresholds 1/4/30/90/127, equality at 90,
zero/rejected random vectors, original Sphere positions, complete bolt restarts,
movement with retained reference endpoints, multi-bolt update ordering and
activation. Cone cases use a reproducible scrambled-counter random stream because
the legacy linear test stream correlates adjacent axes and cannot sample every
cone. Legacy cases retain their original stream. Malformed construction and
coincident restart controls require rejection before RNG consumption.

Native controls extend existing owners rather than changing trusted test cohort
membership: target validation, creation clock, disabled creation, rendering,
visible-segment lighting, scene visibility, simulation RNG isolation, trigger
activation, atomic saved identity, disk restore and rebuilding from live target
coordinates. Audio controls exercise actual generated-fixture PCM, range,
pause, dormant/restart, restore and slot reuse for directed owners. They do not
substitute generated audio into the original Sphere run.

The earlier original qualification reached a genuine downstream resource failure:
`Missing XL sound lightning.wav`. The mission archive and owned startup
installation do not supply it. The original waveform has documented SHA-256
`149382490dfec4517e2594921c8dcb71518c372846b73ee3c157a0f9ca3efd30`, but is not
available among the mounted private assets. Unassisted level load and 120 ticks
were therefore not established by that run. The November 10 implementation imports
the genuine waveform through the existing resource owner. Its new rendered
qualification is tracked separately; this source change alone grants no gameplay
or readiness claim.

Validation: all 42 standalone/reference tests passed, including 218880
complete bolt ticks and 11520 emitter ticks; final changed-source controls passed
again after malformed/restart guards. Both classic regressions passed. Scoped
quality and automation-catalog validation passed. The catalog invocation test
cannot complete on this executor: its Linux child supervisor cannot read
`/proc/self/task/<pid>/children` (exit 125), reproduced twice without changing tests.

Unoptimized Debug native suites exceeded their existing 45/80-second process
deadlines. A line-buffered audio reproduction reached existing save/restore
controls before either lightning audio profile. Native qualification is also
run with Debug assertions retained and `-O2 -ffp-contract=off -fno-fast-math`;
no test deadline or assertion is relaxed.

Final native results: the exact directed audio controls pass for 23/40 and 27/40
with Debug assertions enabled, including zero-delay exclusion of the forcefield
companion. The repository's normal RelWithDebInfo configuration
(`android/tests/run_xl_originals.py`) passes the full effect-inspection suite
in 18.08 seconds and full named-audio suite in 135.57 seconds, within unchanged
80/150-second test limits. All test requirements remain active in that profile.
The first CI-profile audio attempt found an `arnold_1.wav` replacement left in
the generated fixture directory by the interrupted Debug scope test. That
fixture directory was preserved separately; a complete regeneration passed
without code or assertion changes. The original Sphere input was never changed.
