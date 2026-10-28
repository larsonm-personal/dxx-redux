# Separate XL cloak lifecycle

Continue the thirteen-hour run after wide wall identities. Complete cloak
activation, reversal, timing, capacity and persistence using the pinned XL
method-0/default-speed profile. Redux retains its alternate implementation

## Source findings

Reference 2576a98b5de310e3ac7c9777df2677d1627b8bae:

- main/wall.cpp: allocation, owner lookup, StartCloak/StartDecloak and frame steps
- main/segment.cpp: caller, connected-side state, sound and saved UVL values
- include/cstack.h and main/gamedata.cpp: fixed 100-entry cloak stack, growth off
- include/palette.h: FADE_LEVELS is 31, unlike Redux GR_FADE_LEVELS 34
- render/renderlib.cpp WallAlpha: method-0 transition states draw as flat cloaked
  faces, even before the wall type changes; alpha uses the 31-level scale

Known source defects need explicit, tested corrections: the cloak deletion
return never becomes true and the caller can skip shifted entries; recording
after removal can use a different owner; decloak capacity failure writes a wall
type into its state; a large final step can miss the closed type. Missing
connected walls must not be dereferenced. Preserve source arithmetic and normal
ordering while correcting memory/lifecycle errors

## Design and acceptance

1. Keep the existing packed cloak records and base save format. Increase physical
   storage to 100 for XL-capable builds, retaining the stock ten-entry limit
2. Dispatch XL start/frame behavior to android/XL_compat/runtime. Keep stock
   functions as the alternate and process every shifted XL entry once per frame
3. Verify source transitions and alpha through extracted functions, including
   two-sided/one-sided walls, reversal, half/full boundaries, saturation and
   default-speed behavior. Optional lighting/slow-motion profiles remain separate
4. Use real authored triggers, collision/visibility rays, frame animation, audio,
   disk through stock and memory checkpoints. Exercise 100 simultaneous entries,
   the 101st request, slot reuse and completion without skipped entries
5. Validate saved count/identities/time before installing the cloak array. Check
   malformed counts and references plus successful recovery. Preserve stock
   save behavior and cap; keep newdemo missing-back sentinel handling explicit
6. Verify the actual flat draw alpha path, not only a helper's numeric result.
   Register/run an owned Android integration, format and run both catalogs, then
   publish a stacked PR with latest-head CI. No autonomous traversal

This batch does not claim full mission qualification, arbitrary reference
settings, multiplayer or complete demo fidelity

## Progress

Separate runtime, checked saves, optional-back demo decode/rewrite and actual
flat-draw instrumentation are implemented. The native integration passes with
3,200 source comparisons, 100 simultaneous entries, disk through stock, eight
malformed/truncated saves, back-side reversal and large-step light restoration.
MSVC and strict GCC source comparisons pass. Existing close-door/wide-wall tests
and the stock D2 compatibility suite also pass

All 81 Android integration steps now pass (run
5dadbc54a4204b95846396ee8d3301d8, 86134 ms on the final formatted APK), including authored transitions,
reversal, collision, completed audio, active disk/rewind recovery through stock
and the actual GL alpha. Protected hashes match. Early harness corrections fixed
pause/checkpoint ordering, wait syntax and assumptions about retained cloak
opacity after closure; only the complete successful run is acceptance

The scoped formatter, pinned PowerShell analyzer and both catalogs pass. Final
formatted native and Android checks pass. Next: publish the stacked branch and
monitor latest-head CI. See
[cloak evidence](../../XL_compat/evidence/cloak-lifecycle.md)

Preceding stack: xl_more_29_hxm (PR #174, all CI green) and xl_more_30_walls
(PR #175, latest head 8a1a93aeadd9a226a61a522231642ae79cf4bc2a). Six workflows
are green as of 2026-10-28 06:14 UTC; Linux replay CI is still running
