# XL authored close-door behavior

Continue the authorized run on xl_more_25_close_doors, stacked on PR #169.
Keep the stack unmerged and require latest-head green CI

## Source decision

Pinned XL CTrigger::DoCloseDoor passes force=true. CSegment::CloseDoor and
CWall::CloseDoor therefore permit an authored close during an automatic door's
waiting state, reusing its active entry and rebasing animation time. Redux ignores
close actions while waiting. Implement an XL-owned authored close path, with
Redux retained as the alternate and ordinary automatic animation still using
existing earned wall/active-door state

Keep obstruction checks on both connected sides. The pinned DoorIsBlocked loop
only enumerates the front segment despite its comment describing both; retain
Redux's two-sided geometry check as an intentional correction and verify it with
actors on each side. Unsupported effect objects and ignore-marker flags remain
gated. Preserve opening reversal, sound, connected state and save/rewind

## Acceptance

1. Extract pinned CWall::CloseDoor and its caller into an independent oracle;
   compare states, earned timers, active-slot reuse/allocation and obstruction
2. Verify authored waiting close, actual animation/collision, player-hit reversal,
   actor blocking on both sides, exhaustion/malformed guards and Redux isolation
3. Register Android waiting-close, physical/cue and disk/rewind/stock controls
   using controlled poses and source trigger debounce; no navigation
4. Audit relevant originals without promotion; run focused regressions, both
   catalogs, private policy and scoped quality; publish the validated tranche

## Completed local acceptance

3,072 independently extracted source comparisons pass on MSVC and GCC. Native
authored transitions, twelve actor obstruction cases, actual animation/rays,
disk-through-stock and damaged-state controls pass. All 12 focused regressions
and all 80 Android waiting/obstruction/audio/persistence controls pass, with
protected files restored. Both catalogs, all 12 private-policy checks and scoped
quality pass. Seven originals were audited without promotion; repeat generation
preserves bytes and mtime. The evidence records hashes, the two-sided obstruction
correction and the bounded active-door capacity
