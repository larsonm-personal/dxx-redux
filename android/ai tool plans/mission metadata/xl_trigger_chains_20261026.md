# Immediate wall trigger chains

## Scope

Continue the singleplayer gameplay roadmap with master (31), enable (32) and
disable (33) wall actions. Preserve reference link ordering, nested dispatch,
master enable counters, post-dispatch one-shot state, actor restrictions,
top-level debounce and the recursion bound. Negative-side effect-object targets,
delays, extended flags, object triggers and unsupported child actions retain
explicit blockers. This does not qualify an original mission by itself

## Plan and acceptance

1. Audit the pinned reference Operate, DoMasterTrigger, DoEnableTrigger and
   DoDisableTrigger, including exit propagation and state save/load
2. Validate geometric wall targets for the supported chain actions and report
   supported actions separately from unsupported scheduling and flags
3. Implement XL-owned dispatch with minimal D2 hooks, preserving stock behavior
4. Test ordered enable/disable/activate chains, nesting/cycles, actor rules,
   one-shot reentrancy, counters, source save/restore and cross-mission reset
5. Compare counter/link behavior with extracted pinned reference functions
6. Run native and registered Android integration controls, scoped formatting and
   catalog checks, then record evidence and the next unfinished roadmap work

## Initial evidence

Reference revision 2576a98b5de310e3ac7c9777df2677d1627b8bae is available locally
in temp/d2x-xl-reference. Masters resolve links through segment sides and their
walls, in authored order, and pass shot=0 to children. Enable decrements a
positive master value by one raw integer unit, enabling it at <=0. Disable
increments a master value by one raw integer unit. Nonplayer actors may operate
masters and teleports but not enable/disable or stock leaf actions directly

The reference starts depth at -1, rejects entry when depth >15, and increments
before dispatch. One-shot disabling occurs after the action, so cycles must be
bounded independently. The 750 ms debounce applies only at depth zero, excluding
teleports and boosts. A successful exit aborts the remainder of a master chain

## Acceptance audit

1. Source Read, dispatch and counter rules are recorded in the
   [maintained evidence](../../XL_compat/evidence/trigger-chains.md). The source
   translation normalizes pre-39 and modern authored counters; saved live counts
   are restored afterward without another conversion
2. Inventory/browser controls cover all admitted layouts, target bounds,
   unsupported effect targets, delay thresholds and flags. Unsupported child
   requirements remain independent blockers
3. Minimal D2 entry/exit hooks select XL-owned dispatch only with a loaded source
4. Native tests cover ordered and recursive chains, debounce expiry, real wall
   entry and nonplayer physics, stock children, source save/restore and exit
   propagation. The focused native regression selection passed 7/7
5. Forty runtime counter samples match independently compiled pinned reference
   functions. Fresh-read normalization checks cover 18 version/value combinations
   in both the translator tests and extracted reference. These checks qualify
   the tested semantics, not an entire reference campaign
6. Android passed 62/62 steps, including live counter assertions, one-shot state,
   disturbance, restore and stock-to-XL recovery. The runner restored app-private
   file hashes exactly. Scoped formatting/lint and both catalog checks passed

The immediate wall-chain chunk is complete within this profile. The
[overall roadmap](xl_singleplayer_gameplay_roadmap_20261026.md) remains open;
delayed scheduling and stateful flags are next. No new original mission is
qualified by this chunk. Trusted CI wiring uses the already-pinned Neptune HOG
and descriptor, whose hashes match the local originals. All 12 private-fixture
runner policy tests pass after integration with current cmake, including
required-test receipt validation. The required native test count is now 41
