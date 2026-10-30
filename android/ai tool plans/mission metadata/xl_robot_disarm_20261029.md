# Owned XL robot disarming

Continue the authorized run on `xl_more_57_disarm`, above the countdown
increment and the pixel-header relocation in PR #204. The original Pumo action is type 34, autoplay/one-shot/no-message,
targeting two robots in `mvlev5`. Support is detected from action/owner
requirements, never the mission name. Reprogramming remains separate.

1. Execute pinned `DoDisarmRobots`, attack-mode predicates and the melee gate.
   Audit the real ranged-fire and thief call sites. Preserve the source's
   distinction between suppressing attacks, stopping thief decisions, movement,
   existing projectiles and collision-time theft. A global robot pause is not
   an implementation of disarming.
2. Keep neutral state in an XL-owned table bound to object lifetimes. New/reused
   slots start hostile; clear mission state on transition. Add minimal consumers
   at ranged fire, melee and thief decisions while retaining Redux's alternate.
   Preserve inert positive-side links and resolve negative-side links against
   live runtime slots, including generated robots. Missing slots and nonrobot
   targets are inert, as in the source; do not restrict links to authored objects.
3. Dispatch wall/object actions through existing flags, master chains and
   scheduling. Persist earned neutral state through ordinary disk/rewind, with
   atomic validation and native owner/signature checks. Preserve prior launched
   weapons and unrelated actors. Keep reprogramming and other AI profiles gated.
4. Verify real native attack consumers in both layouts and controlled Android
   events/persistence. Include armed controls, lifetime reuse, malformed saves
   and stock isolation. No autonomous navigation. Refresh the unchanged original
   inventory only after these controls pass.
5. Reuse source owners where practical, add bounded integration ownership,
   run affected regressions/catalogs/formatting, record exact support limits and
   publish the reviewable increment without merging. If implementation or
   acceptance remains incomplete at the run deadline, retain the admission gate
   and report that precisely.

The October 29 continuation resumes this unfinished increment at 17:03 UTC.
Finish and publish disarming first, then record the start of the additional
eleven-hour development block. Continue publishing reviewable stacked PRs,
without merging, and report the worktree, branch and publication state explicitly.

## Verified implementation checkpoint

At 17:55 UTC, the final Linux owner passes 106 required controls, sixteen
original-format outcomes and three modern source comparisons. Windows passes
both native layouts; final affected public contracts pass on both platforms.
Android passes all 67 steps with 26 protected files unchanged. The source owner
executes 1,120 comparisons. Both catalogs, formatting and PowerShell analysis
pass. See [disarming evidence](../../XL_compat/evidence/disarming.md) for receipts,
source quirks, lifetime semantics and the unchanged original Pumo requirements.

Publish this increment on `xl_more_57_disarm` with base
`xl_pixel_header_location`; keep the main `cmake` checkout untouched. The
additional eleven-hour block has not started at this checkpoint. Record its
start after publication and handle any CI failures before treating disarming
as complete.
