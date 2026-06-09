# Level metadata route completion pass

## Goal
- Treat D1 and D2 base-game normal levels as completable.
- Investigate normal levels currently reported as partial/unreachable by the travel estimator.
- Improve routing around ordered keys and locked-door hubs if the estimator is too naive.

## Plan
- [x] List normal base-game levels with non-ok travel status.
- [x] Inspect one representative level's travel failure and likely key/door ordering.
- [x] Update the route estimator to handle recursive key acquisition and non-key progression openings.
- [x] Re-check remaining D1 key failures without assuming unreachable keys are acquired.
- [x] Refresh headless metadata baseline and run focused verification.

## Findings
- The initial travel estimator reported many base-game normal levels as partial because key acquisition was not recursive. For example, routes that needed a red door could fail when the red key route itself crossed another key door.
- After recursive key acquisition, most D1 failures disappeared, but examples like D2 level 2 still failed because hostages sit behind non-key progression mechanics such as hidden or trigger-opened doors.
- The completion estimate now treats keyed doors as the only hard route constraint. Connected non-key geometry is assumed openable/flyable, matching the user's assumption that D1/D2 normal levels are completable.
- Two D1 normal levels still had key pickups unreachable in the abstract graph after the recursive key and non-key progression fixes. The optimistic fallback that assumed those keys were acquired was rolled back so the metadata does not hide uncertain route logic.
- After rolling back the broad fallback, D2 normal levels reported OK and D1 had two partial normal levels: level 14 (`europa co2 mine`, gold key unreachable, reached 6/8 targets) and level 25 (`pluto outpost`, blue key unreachable, reached 5/7 targets).
- A same-key detour fallback briefly cleared those reports, but it was rejected as another special case and removed.
- A focused D1 level 14 trace showed the remaining failure was stale route input. The headless/launcher metadata path called `load_level()` directly, before `Player_init[]` was populated by normal gameplay startup, so the scanner began from segment 0 instead of the loaded player start object.
- Reading the player start from loaded objects gives D1 level 14 start segment 187. The ordinary route now resolves as no-key start to blue key, then blue key through the blue door to yellow/gold key, without level-specific or key-specific special casing.
