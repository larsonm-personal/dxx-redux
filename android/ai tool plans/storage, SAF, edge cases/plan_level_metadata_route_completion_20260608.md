# Level metadata route completion pass

## Goal
- Treat D1 and D2 base-game normal levels as completable.
- Investigate normal levels currently reported as partial/unreachable by the travel estimator.
- Improve routing around ordered keys and locked-door hubs if the estimator is too naive.

## Plan
- [x] List normal base-game levels with non-ok travel status.
- [x] Inspect one representative level's travel failure and likely key/door ordering.
- [x] Update the route estimator to assume completable level flow where reasonable.
- [x] Refresh headless metadata baseline and run focused verification.

## Findings
- The initial travel estimator reported many base-game normal levels as partial because key acquisition was not recursive. For example, routes that needed a red door could fail when the red key route itself crossed another key door.
- After recursive key acquisition, most D1 failures disappeared, but examples like D2 level 2 still failed because hostages sit behind non-key progression mechanics such as hidden or trigger-opened doors.
- The completion estimate now treats keyed doors as the only hard route constraint. Connected non-key geometry is assumed openable/flyable, matching the user's assumption that D1/D2 normal levels are completable.
- Two D1 normal levels still had key pickups unreachable in the abstract graph. For those, the estimator now assumes the required key can be acquired when the key exists, then continues the completion route instead of marking the mine broken.
- Probe result after the route changes: zero non-ok normal levels for both D1 and D2 base games.
