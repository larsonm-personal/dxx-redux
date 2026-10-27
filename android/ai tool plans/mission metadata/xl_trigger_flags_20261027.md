# XL trigger flag and audio fidelity

Implement on xl_more_13, stacked on boss-transport PR #155. No merges.

## Source findings and scope

Reference arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
TF_SET_ORIENT (32) is consumed only by the still-unsupported speed-boost path.
For currently admitted actions it is inert, not a request to change teleport
orientation. TF_SILENT (64) has two action-specific audio consumers: it suppresses
the forcefield-off cue, but enables the explicit illusion-off cue. The latter is
counterintuitive; preserve the pinned behavior rather than inventing a global
mute policy. Other implemented actions ignore this bit.

Keep shared geometry queries and forcefield mechanics, with an XL cue policy and
bounded XL illusion toggles. Redux retains its classic path when no XL level is
active. The XL toggle must handle missing back walls safely and use the connected
side, correcting the pinned segment method's same-side lookup. Record that
intentional correction; do not claim full wall/material or collision parity.

## Implementation and acceptance

- Use one shared flag mask for the inspector and both XL event runtimes. Admit
  bits 32/64 with their verified per-action semantics. Keep wider object flags,
  speed boosts, unsupported actions/owners and delayed object descendants gated
- Compile actual hash-pinned DoChangeWalls/DoIllusionOff functions against
  recorded engine/audio adapters. Compare cue decisions and retained wall/hum
  operations for all byte flag combinations, including the source's inverted
  illusion-off condition. State the geometry/mixer limits of this comparison
- Exercise actual native wall and object dispatch, one-shot/alternate/scheduled
  state and disk restoration with the new bits. Verify high bits still reject
  and ordinary Redux dispatch retains its existing behavior
- Use a derived source fixture for actual Android mixer attempts, forcefield
  hum removal, authored object events, checkpoint/disk continuation and stock
  isolation. Use controlled poses; no autonomous navigation
- Run focused regressions, both catalogs and scoped formatting, record evidence,
  then publish separately and require green latest-head CI

This does not qualify a new original mission, speed-boost orientation, every
wall/material behavior or full audio equivalence. The earlier proposed delayed
object-chain work remains open: source wall Countdown loses its attached
context, so it needs a separate explicit behavior/lifecycle decision rather than
blindly enabling the current guard.

## Completed controls

All 6,400 source leaf cases pass with MSVC and Linux GCC. The 21-test native
sweep passes, followed by the strengthened teleport/forcefield and multi-profile
inventory controls. Android passes all 66 steps with real mixer diagnostics,
two asynchronous checkpoint roundtrips, a different-difficulty stock transition,
disk restore and exact protected-file restoration. Both catalogs and all 12
private-policy checks pass. See
[trigger flag evidence](../../XL_compat/evidence/trigger-flags.md) for fixture,
APK and receipt hashes. Publish separately and require green latest-head CI
before reporting this tranche complete.
