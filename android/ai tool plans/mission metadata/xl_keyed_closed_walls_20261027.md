# Actor-specific XL closed walls

Continue the authorized run on xl_more_23_keyed_walls, stacked on PR #167.
Keep the stack unmerged and require latest-head green CI

## Source decisions

Pinned arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae CWall::IsPassable
permits a player through a closed wall with exactly KEY_BLUE or KEY_RED when
that player owns the matching key. Gold and combined key masks do not enable
this rule. Actorless queries and nonplayers retain ordinary closed-wall behavior

The source ignores door-opened flags on closed walls. Redux's common fallback
can treat that bit as passable regardless of wall type, so the XL closed-wall
path must compute the full source result rather than only add an exception

Blocked destination and boost-exit restrictions remain outer constraints. Keep
the no-clip/observer bypass and ordinary stock behavior. Use live wall and player
state so existing key, wall-trigger, disk and rewind state remain authoritative;
no new mutable gameplay state or save schema is required

## Acceptance

1. Extract the actual pinned CWall::IsPassable function; compare actor types,
   key masks/ownership, cloak/texture transparency and irrelevant door flags
2. Add XL-owned closed-wall dispatch for actorless doorway queries, actor-aware
   FVI and player containment. Preserve rendering and stock alternatives
3. Derive blue/red/gold closed-wall portals, including a blocked destination.
   Check real physics, visibility, projectile/pickup/robot rejection, keys,
   blocked precedence, live wall changes, disk through stock and malformed IDs
4. Register/run Android controlled key acquisition/removal, attempted physical
   crossings, visibility state and disk/rewind/stock controls. No navigation
5. Audit original closed-wall candidates, record source and evidence scope,
   run focused regressions/catalogs/private policy and formatting, publish PR

## Completed local acceptance

55,296 extracted-source samples pass on MSVC and GCC -Wall -Werror. Actual native
physics covers 120 crossings plus live keys/walls, blocked and boost precedence,
disk-through-stock and stock controls. All 15 focused regressions pass in 72.08s.
The Android runner passes all 90 checks in 67,854 ms with protected files restored.
Both catalogs, all 12 private-policy checks and scoped quality pass. The original
audit records 121 levels (93 decoded, 28 explicit rejections), with no mission
qualification; identical regeneration preserves bytes and modification time.
See the evidence record for source/fixture/APK hashes and coverage limits
