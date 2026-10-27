# Authored XL boss destination groups

Implement on xl_more_14, stacked on trigger flag PR #156. No merges.

## Source scope

Reference arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
CSegment::Read retains separate owner and group bytes. Segment m_group consumers
are CBossData::SetupSegments and multiplayer entropy ownership. Menu m_group
fields are unrelated. Boss destination traversal compares group equality; it
does not require team ownership or a wall at the group boundary.

Keep every group byte as a distinct identity, including the default 255/-1.
Admit groups with neutral owner -1; preserve all existing ownership, multiplayer,
layout, capacity, resource and feature gates. Existing XL destination state
already retains source groups and validates restored routes against them.
Do not introduce a Redux boss fallback or a mission-name exception.

## Acceptance

- Separate known group observations from unknown ownership in inspection and
  show group support/count in the compatibility browser. Check all 256 bytes
  across the three admitted format profiles, retaining ownership rejection
- Extend the pinned boss traversal comparison across byte identities and group
  boundaries, retaining actual SetupSegments as the reference
- Derive a two-boss source with open connected rooms and distinct destination
  groups. Prove physically reachable other-group rooms are excluded; moving a
  boss rebuilds only its list and group, including the default group. Check
  source/disk/rewind identity and reject cross-group cached routes atomically
- Exercise authored object transport, real subsequent AI destination selection,
  disk restoration through stock and Android checkpoint/disk controls without
  autonomous navigation
- Run focused regressions, catalogs and scoped formatting, record evidence,
  publish a separate PR and require green latest-head CI

This does not admit entropy/team ownership, arbitrary expanded capacities or a
new original mission. Pin source consumer census and document comparison limits.

## Completed controls

All 768 inventory cases and 512 new source-label comparisons pass; the previous
24 native source cases and 20 graph controls remain green. Linux GCC also passes.
The actual-loader group control proves open/real-size negative controls,
authored transport, selection across explicit/default groups, atomic rejection
of cross-group routes and disk restoration through stock. All 22 focused native
tests, all 12 private-policy checks and both catalogs pass. Android passes all
48 lifecycle steps and restores protected files exactly. See
[boss group evidence](../../XL_compat/evidence/boss-groups.md) for source pins,
fixture/APK hashes and acceptance scope. The original candidate reports are
refreshed without gameplay promotion. Publish separately and require green
latest-head CI before reporting this tranche complete.
