# Boss object-teleport destinations

Implement on xl_more_12, stacked on effect-inspection PR #154. No merges.
Use the separate XL boss runtime; stock Redux keeps its existing path.

## Reference and scope

Reference arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae:
main/trigger.cpp DoTeleportBot, objects/boss.cpp Setup/SetupSegments and boss save
state, ai/aiboss.cpp BossFitsInSeg. An object teleport to a different segment
rebuilds that owner's lists from its new group and current passability. A
same-segment action does not rebuild. Clocks/death state survive Setup. Ordinary
wall teleports have different semantics and do not perform this rebuild.

## Implementation

- Match the pinned destination-list traversal, including ordering, home revisits,
  group boundaries, size filtering and bounded capacity. Remove the stock-derived
  one-wall fallback from the XL setup path if the source comparison confirms it
- Rebuild the moved boss's list after direct and immediate-master object actions;
  preserve all owner-specific clocks and other bosses
- Capture the earned list and its home explicitly in checked disk/rewind state.
  Do not reconstruct it from current walls at restore time, since walls can have
  changed after setup. Validate the complete list before publishing any state
- Remove the boss-object-teleport admission restriction only after native/source,
  authored activation, continuation, persistence and stock controls pass. Retain
  all unrelated payload, resource, delayed-chain and multiplayer gates
- Expose owned destination state for controlled diagnostics and Android assertions

## Acceptance

1. Compile actual hash-pinned SetupSegments/Setup/DoTeleportBot functions against
   engine/storage adapters. Compare source ordering, group/size/passability
   boundaries, same-segment behavior and owner selection; state adapter limits
2. Use derived source geometry with two bosses in isolated destination components;
   check differing groups separately in the extracted source contract.
   Verify real damage-trigger dispatch, immediate-master context, independent
   clocks, new-group AI teleport selection and no same-segment rebuild
3. Save after rebuilding, change walls, restore through stock, and continue using
   the captured list. Exercise malformed list rejection before any event/state
   publication, plus actual authoritative checkpoint roundtrips on Android
4. Run focused regressions, both test catalogs and scoped mixed formatting;
   publish a separate PR and require green latest-head CI

No autonomous navigation, original-mission promotion or complete boss/AI parity
claim follows from this derived fixture.

## Acceptance record

The 19 focused native checks pass, including 24 native/source cases, 20 direct
traversal comparisons and 16 malformed-state guards. The normal APK passes 48
steps and three exact checkpoint roundtrips, preserving protected device files.
Both catalogs, the 12 private-policy tests and a fresh pinned-source bootstrap /
CTest-registration check pass. See [the evidence](../../XL_compat/evidence/boss-transport.md).

The initial test fixture exposed the stock save format's eight-character mission
basename and illusion walls that were not barriers. The corrected bosstran
fixture uses two solid wall pairs; it does not relax production admission.
Publish separately on xl_more_11 and require green latest-head CI.
