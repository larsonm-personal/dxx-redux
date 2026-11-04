# XL collision crossing-path repair

A native swept-hull trace could return a later segment while omitting the
intermediate portal segments. Player trigger dispatch consumes this list, so
missing cells can drop ordinary contact actions during a long movement frame.
XL now checks path endpoints/adjacency and, only for an invalid nonzero-radius
path, traces the center to the already accepted collision point. It replaces
crossing metadata only if that trace reaches the same segment through a valid
path. Zero-radius traces cannot recurse. Collision results and positions remain
unchanged. Ordinary non-XL levels return before the repair; D1 has no new hook.

## Evidence and limits

The original Crauphel warm78/cold22 passed on retainedAPK29471, then the unchanged
prefix in Majha run4e5cc7abcd55455da436efe100965b62 failed76/154 at level2/segment418
with boost17 and exit18 unactivated. Majha was not reached. The retained APK does
not contain this fix; this is a related reproducible source defect, not a claim
that this exact Android failure is resolved. No APK builds or phone tests.

Controlled isolated geometry, not earned progression proof:

- Original-derived corridor:7776 cases, baseline1479 badFVI/1647 badphysics,
  fixed0/0. At speed100,330 cases had a bad path; speed460 had1317
- Independently generated five-cell corridor:1944 cases, baseline216/216,
  fixed0/0. Includes18 speed100 failures before repair
- Frame intervals1/120,1/60,1/30,1/20,1/10,1/5,1/4,1/2,1second; offsets-5/0/5,
  ship radius301466raw. The timer currently permits the slow-frame intervals
- All accepted collision points/normals/objects/sides and final positions/velocity
  records are byte-identical baseline/fixed across both complete case sets
- CMake xl_speed_boosts gate PASS, including1944 generated crossing controls,
  4740 boost source samples, actor/save/phase/malformed-trailer and stock isolation
- Scoped quality and both catalogs PASS. The separately pinned external XL boost
  oracle was not run in this host configuration. No all-campaign or phone claim

Private diagnostics remain under
`temp/xl-overnight-20261104/boost-crossing-fixture`:
`slow-baseline-{original,synthetic}.log`, `slow-fixed-{original,synthetic}.log`,
`slow-comparison.json`, all corresponding collision/path JSONL receipts,
`cmake-gate-run.json`, `cmake-controls/controls.log`.
Precommit fixed host EXE SHA256:
`e66cfdb8444f6d0ae62e5c09237fca3d3de6f92a023e28fbcc344598b6ebaf1b`.
The explicit baseline is the same test source with only the repair hook omitted.
Original-derived geometry stays private; checked-in controls generate their own
boxes. No mission, model or retail file was changed or committed.

Collision-receipt SHA256, equal before/after:
originalf523f3ccddbab8968448182c6ec3da5b92d5b63d4ef13a16b76dbb6ccece1cc5;
synthetic7571745e0cf613d0d733f170dfa6b68d563cc68b1d98dfac6732d8027fe94456.
No direct trigger/exit dispatch, inventory grants or controlled ending is used by
these crossing controls. Trigger delivery and the entire original mission still
require ordinary gameplay qualification on a binary containing the fix.
