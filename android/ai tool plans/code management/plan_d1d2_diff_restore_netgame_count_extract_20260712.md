# D1/D2 restored Netgame count extraction plan

## Goal

Move the final duplicated Android coop restore player-count normalization block from both upstream-original `state.c` files into the existing shared coop restore layer.

## Baseline and boundary

- Each state file has about 19 lines that counts live playing/waiting players, raises saved Netgame counts when necessary, sets connected count, and logs the result
- The existing `coop_restore_remap.h` and `coop_save.c` already own restore-time player remapping and coop metadata
- Game-specific saved-Netgame read logs remain local; only post-read live-count normalization moves

## Work

- [x] Add one shared restore-count function taking the D1/D2 game tag
- [x] Replace both duplicate count blocks with one call
- [x] Preserve connected-state predicates, max-only updates, connected assignment, and coop-only log fields
- [x] Run scoped code quality and diff checks
- [x] Build all Android ABIs and Windows D1/D2
- [x] Record the exact reduction and mark the remaining mechanical queue endpoint

## Risk controls

- Do not change save reads, Netgame field types, player disconnect policy, or remap ordering
- Call the helper at the exact existing point after Netgame fields are read and before score/flag refresh

## Completed result

- `d1/main/state.c`: 1,231 additions to 1,216 additions versus `upstream/main`
- `d2/main/state.c`: 1,737 additions to 1,722 additions versus `upstream/main`
- Exact inherited-file reduction: 30 additions
- The saved Netgame read log, score/flag refresh, disconnect policy, and remap ordering remain local and unchanged
- Scoped code quality, `git diff --check`, Android all-ABI builds, and Windows D1/D2 builds passed
