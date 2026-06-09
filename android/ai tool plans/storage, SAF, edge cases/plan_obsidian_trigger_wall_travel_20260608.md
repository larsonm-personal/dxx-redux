# Obsidian trigger wall travel analysis

## Goal
- Investigate why many Obsidian maps report failed travel metadata.
- Determine whether location-triggered disappearing walls are blocking the travel graph.
- Add a general passability rule if trigger-opened walls should be treated as route-passable.

## Plan
- [x] Reproduce Obsidian travel failures with the local `Obsidian.zip`.
- [x] Inspect trigger/wall data exposed to the shared metadata scanner.
- [x] Patch the scanner/adapter to treat trigger-opened disappearing walls as passable for metadata travel.
- [x] Re-run Obsidian and base-game verification, then run scoped formatting/build checks.

## Notes
- User observed normal progression that reveals large map sections by disappearing walls.
- The intended rule is data-driven, not Obsidian-specific: if a wall disappears because an open-wall trigger targets it, consider it passable for metadata travel.
- Reproduced via headless D2 dump staged from `Obsidian.zip`: 16 of 18 levels reported non-ok travel before the trigger-wall routing change.
- The base route scanner already treated non-key walls as passable, so the blocker was not ordinary closed wall geometry.
- The false failures came from keyed `WALL_CLOSED` targets. The secret-area opener lookup intentionally ignored keyed target walls, which is correct for secret labeling but too strict for metadata travel.
- The metadata scan now uses a separate opener count that includes keyed target walls; the secret-area scan keeps the older key-filtered behavior.
- After the keyed-trigger fix, Obsidian improved from 16 of 18 non-ok travel results to 3 of 18. The remaining three are `missing reactor` rather than pathing failures.
- D1 and D2 headless base dumps both built and reported zero non-ok travel levels. The baseline wrapper still exits nonzero because the checked-in fixture differs from this worktree's generated metadata.
- Scoped code quality passed. Android `:app:compileDebugKotlin :app:externalNativeBuildDebug` passed.
