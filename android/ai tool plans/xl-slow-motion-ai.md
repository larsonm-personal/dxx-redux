# XL slow-motion AI timing stage

Base #393, 6af375a47a38821cb9ec434cec73ba42727c8eb4.

Implement the pinned AI firing timer floor and decrement while preserving the
real-time scheduling counter and absent-secondary reset. Qualify actual AI
frames in a distant stationary fixture, including boundary and multiplayer cases.
Implement the world-clock dot and scale consumers in ordinary AI turning; retain
the baby-spider turn-impulse dispatch and current native normalization fidelity.
Compare new arithmetic to pinned source, preserve classic behavior, run relevant
native/public/classic regressions and publish only a separate draft. Pickup
admission remains disabled. Weapon/door/effect consumers, controls, rendering,
audio and persistence remain subsequent work.

## Implemented behavior

A single-player XL AI frame uses the world-clock factor for its firing decrement
and negative eight-second floor. A missing secondary weapon still resets its
counter to eight seconds. The scheduling counter still advances by raw frame
time until its 256-second bound. The original C block remains the classic and
multiplayer fallback. Ordinary AI turning scales the dot input and computed
turn increment by the world clock; baby spiders keep their separate physical
turn-impulse path, including console identity behavior already qualified in #393.

## Validation and limits

The mandatory water-actors source phase compares 1584 real do_ai_frame calls
across four clock profiles, multiplayer isolation, present/absent secondary,
11 firing-floor positions, three frame durations and three scheduler bounds.
A distant station behavior intentionally returns after timing so later combat
cannot obscure the counters. This qualifies the actual timer hook, not a new
complete slow-motion combat scenario.

576 native AI turning cases compare the extracted scaling arithmetic and verify
actual orientation integration using unchanged native normalization. Baby-spider
dispatch, zero-rate handling and no-seismic RNG isolation are checked too. Native
normalization/trigonometry parity and seismic turning are not newly qualified.

Reference revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The supported source setup retains exact raw-byte pins:

- ai/aiframe.cpp: 9fdc24b85cdfc7282830a0cc6e5a52fce9390151ecb4f157080e919c9c9046fd
- ai/aimove.cpp: 2aef988961c51464669d280d9073e2eceb98a8da45041a43d503b667005b5941

Source oracle ASan/UBSan passed with leak detection disabled. Fourteen
missing/duplicate/incorrect/truncated/trailing/unknown/invalid-mode variants
were rejected. Existing displacement, lifetime, rotation, impulse and contact
comparisons remain mandatory. Public source/GPU/audio owner: 55/55 passed.
Scoped quality and catalog validation passed; catalog execution remains blocked
by child-supervisor exit 125, unavailable /proc/self/task/<pid>/children.
Native engine sanitizers, Windows/Android and unchanged rendered Warp Shock
qualification were not run. No readiness improvement is claimed.

Original Neptune HOG remains SHA-256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
Trusted FAST membership, original missions, admission checks and deadlines are
unchanged. Genuine sprites at assets PR #4 head
d09f199c3b66bab71970364b1939b1658851ea81 are not consumed by this AI stage.

Native water/lava/projectile/cloak owners passed 4/4; classic upstream and
trigger owners passed 2/2; private-assets Python policy passed 33/33.
