# KCXF2 worker crash and Obsidian live route

Date: 2026-08-15
Status: implemented and validated; unrelated cold-corpus findings documented

## Requests

- Fix the KCXF2 level-metadata worker crash captured in the supplied native
  tombstone
- Make the live Guide-Bot route on Obsidian level 4 continue through the
  post-reactor blue and red keys before directing the player to the exit

## Plan

- [x] Symbolize and reproduce the KCXF2 `load_mission_ham` crash, then fix the
  underlying mission-analysis lifecycle or invalid state
- [x] Trace canonical-to-live route projection and cache invalidation on
  Obsidian level 4 to explain why live routing still uses the old exit goal
- [x] Add focused native and maintained integration regressions for both cases
- [x] Run scoped formatting, focused tests, a real KCXF2 analysis, a live
  Obsidian route test, and paired D1/D2 build validation
- [x] Reproduce why full regression regeneration leaves `Obsidian.json` with
  the old level-4 route and fix the responsible runner/analyzer/cache path
- [x] Regenerate all configured regression data and review the complete diff
  for intended route updates and unrelated regressions

## Results

- The tombstone's null dereference in `load_mission_ham` came from a reusable
  metadata request leaving `Robot_replacements_loaded` set after freeing
  `Current_mission`. Request cleanup now restores the base or mission HAM before
  freeing the mission.
- Exit planning now follows any reachable scripted opener path before acquiring
  a still-required exit key, then returns to the exit. It also recognizes an
  exit whose opener trigger has already fired, while retaining the key for a
  genuinely keyed exit. Both persistent route caches were also advanced from
  generation 5 to generation 6.
- Reusing one D2 metadata worker for Obsidian followed by KCXF2 passed on the
  emulator without a crash.
- Host analysis and live device introspection both report the 11-step Obsidian
  level 4 route: yellow key, reactor, three post-reactor trigger objectives,
  blue key, another trigger, red key, then exit. Device introspection reports a
  generation-6 cache hit with no live fallback.
- The maintained live Guide-Bot progression script reached and validated the
  on-device route. Its final objective-by-objective phase could not run in
  isolation because concurrent robot-preview automation repeatedly replaced the
  app during the test; those interruptions produced no crash or product
  assertion failure.
- Scoped code quality, D1/D2 route-cache tests, Kotlin cache/scheduling tests,
  Android assembly, and the paired Windows build passed.
- The canonical mission regeneration templates now clear both the native route
  cache and the complete level-metadata result cache before every analysis.
  A runner regression test enforces both clears and their ordering.
- A focused canonical Android regeneration rewrote `Obsidian.json`; level 4 now
  contains the reviewed 11-step post-reactor route. The 1,561-route reviewed
  corpus baseline and travel/status checks pass with the regenerated result.
- A cold host sweep analyzed all 115 configured archives: 112 passed, one had
  no descriptor, and the two Vertigo-derived archives failed because their
  packages reference the absent `d2xlvl10.rl2`. The sweep also exposed older
  partial-route results for KCXF2 level 1 and Phobos level 4; those unrelated
  results and host-only Ulterior removals were not accepted into the reviewed
  mission data.

## Constraints

- Do not hard-code either mission or level
- Preserve the user's existing outstanding-bugs and extraction-regression edits
- Keep shared behavior in Android shared code and retain D1/D2 compatibility
