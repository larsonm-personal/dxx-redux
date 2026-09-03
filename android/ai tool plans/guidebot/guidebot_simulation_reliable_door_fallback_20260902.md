# GuideBot simulation reliable door fallback

## Plan

- [x] Define deterministic flare-attempt and close-range fallback state
- [x] Remove unconditional direct hits on upcoming path doors
- [x] Require a real flare attempt, flight-time allowance, and close proximity before fallback
- [x] Preserve authoritative wall key, lock, and blastability checks
- [x] Add focused regression coverage for fallback eligibility
- [x] Build and run focused and integration verification

## Results

- A successful `Laser_create_new_easy` call arms recovery for that exact wall
- Recovery waits for calculated projectile travel time plus 0.25 seconds
- Recovery additionally requires the wall to remain closed, the actor to be within the door face radius plus actor radius and two units, and a clear engine trace to the same side
- Opening or changing objectives clears the armed wall, so an auto-closed door requires another real flare
- Counterstrike levels 1 and 9 produced byte-identical results across two runs each
- Level 1 remained `ok`; level 9 retained its later, unrelated fly-through-trigger timeout
- Level 9 exercised the recovery once after two real flare shots, confirming the guaranteed path without proactive remote opening
- The source compiled for all D2 targets; a separate headless-route binary linked successfully while the checked build executable was occupied by an ongoing corpus run
- All 45 D2 tests, scoped code quality, and `git diff --check` passed
