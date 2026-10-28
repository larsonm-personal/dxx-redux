# Separate XL doorway semantics

Continue the authorized run on xl_more_24_doorways, stacked on PR #168.
Keep the stack unmerged and require latest-head green CI

## Implementation

Use pinned CWall::IsPassable for admitted wall types 0 through 7, with Redux as
an alternate. In particular closing doors remain transparent, only doors honor
the door-opened bit, and cloaked walls retain their cloak draw flag. Keep actor
keys, blocked/boost constraints, no-clip and observer dispatch. Colored walls,
multiplayer and new layouts remain gated. No new saved state is required

## Acceptance

1. Extend the extracted-source oracle across types, flags, states, cloak and
   actual texture flags; retain the actor/key matrix from the preceding tranche
2. Verify actual opening/closing and reversed animation, collision versus
   visibility, disk restoration, stock isolation and malformed indices
3. Register Android controlled wall/door crossings and animation, disk/rewind
   and stock controls, with protected files restored; no navigation
4. Run focused native regressions, both catalogs, private policy and scoped
   quality. Record precise evidence without original-mission promotion

## Completed acceptance

8,340 extracted-source doorway comparisons and the corrected 55,296 key samples
pass on MSVC and GCC -Wall -Werror. Native authored triggers, animation,
physical/visibility rays, player-hit reversal and disk through stock pass.
All 16 focused regressions and all 85 Android controls pass. Protected files
were restored, both catalogs and all 12 private-policy checks pass, and scoped
quality is green. The evidence records corrected fixture/test assumptions and
the signed cloak-byte adapter fix; no original mission is newly qualified
