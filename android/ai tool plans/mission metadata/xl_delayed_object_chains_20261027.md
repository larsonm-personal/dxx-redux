# Delayed object chains and the second extended batch

Start from merged cmake ec1333671 in isolated branch xl_more_16_events. Continue
the authorized eight-hour run with independently reviewable feature PRs, green
latest-head CI, and no merges. The previous batch's scope and original mission
qualification requirements remain in force

## First tranche

Implement delayed geometry actions reached from nonplayer object masters. Keep
XL state and behavior separate from the inactive Redux alternate

The pinned reference at arbruijn/d2x-xl
2576a98b5de310e3ac7c9777df2677d1627b8bae propagates object context in
DoMasterTrigger, then loses it through geometry Countdown(false). It also
computes object-table indices for geometry children and retains only a reusable
object index. Preserve its defined delay selection, first activation, ordering,
enable/disable, permanent, alternate and one-shot semantics. Explicitly correct
context loss and stale actor reuse rather than claiming those defects match

- Store attached-event context and the initiating object's signature with each
  scheduled wall action, including disk and rewind state
- Continue authored geometry/resource work after the event owner is removed
  (robot destruction or pickup). Actor motion requires the original live actor;
  it must never teleport a reused slot. Nested masters retain that distinction
- Preserve independently timed descendants, random delay RNG, disabled pending
  actions, first-activation identity and rearming; bound recursive chains
- Validate all restored state before publishing any part. Expose context and
  identity in introspection. Retain meaningful blockers for unrelated features
- Compare actual pinned scheduling/master functions where behavior is defined;
  separately label and test intentional lifecycle corrections
- Test real authored events, deletion/slot reuse, exact deadlines, nested
  actions, disk through stock, rewind, malformed state atomicity and stock
  isolation. Use controlled poses/actions, never autonomous level navigation
- Add a reusable Android integration runner, register catalogs/timeouts, run
  scoped formatting and focused native/source/Android checks. Publish evidence
  and update the support ledger only after checks pass

## Subsequent tranches

Prioritize unfinished gameplay semantics using the original mission surveys:
speed boosts/movement, ambient sound activation/cleanup, additional segment
properties and cross-feature controls. Select each next tranche from evidence
after finishing the current one. Inspection alone does not establish runtime
support; derived fixtures do not qualify original campaigns

At the end of the requested run, verify every published PR's latest SHA and CI,
record what remains incomplete, and leave all PRs unmerged

## First-tranche result

All 25 focused native/source tests, the Linux GCC timing oracle, 12 private-policy
tests, both catalogs and 45 Android steps pass. Protected app files restored
exactly. See the [evidence](../../XL_compat/evidence/object-chain-delays.md).
Latest-head CI remains required; no original mission has been promoted
