# RNG Discipline and FrameTime Math Determinism 2026-05-10

## Goal
- reduce replay nondeterminism from shared RNG consumption, FrameTime-sensitive random gates, and platform-dependent simulation math
- start with small D1 and D2 changes that improve determinism without changing broad game behavior

## Steps
- [completed] audit RNG streams, d_rand callsites, FrameTime random gates, and float simulation math in D1 and D2
- [completed] replace the AI path velocity smoothing float expression with a fixed-point helper in both games
- [completed] add a focused host test for the fixed-point smoothing helper so cross-build drift is caught without launching the game
- [completed] move random custom music track selection to the FX RNG stream in D1 and D2
- [completed] run focused builds, tests, and scoped code quality checks

## Notes
- D1 and D2 both have the same AI path smoothing expression in `aipath.c`
- Added `deterministic_math.h` with `dxx_ai_path_smoothing_delta()` and switched both AI path and player path smoothing callsites to it
- Added `test_deterministic_math` to both D1 and D2 maths host builds
- Moved random custom music track selection in `songs.c` and `jukebox.c` from `d_rand()` to `d_rand_fx()` so presentation music selection no longer consumes simulation RNG
- Moved D2 seismic sound-only rescheduling in `weapon.c` to `d_rand_fx()` while leaving seismic start and shake physics randomness on simulation RNG
- Random robot sound timers and collision sound jitter still use simulation RNG; robot timers are stored or gameplay-adjacent, so they should move only with targeted state-trace checks

## Validation
- `cmake --build buildd2 --target test_deterministic_math; .\buildd2\maths\test_deterministic_math.exe`: PASS
- `cmake --build buildd1 --target test_deterministic_math; .\buildd1\maths\test_deterministic_math.exe`: PASS
- `cmake --build buildd2 --target dxx-redux-d2; cmake --build buildd1 --target dxx-redux-d1`: PASS with existing warning noise
- `cmake --build buildd2 --target dxx-redux-d2`: PASS after the seismic sound RNG split
- D1 and D2 `test_rng_seed_resume`: PASS
- D1 and D2 `test_input_demo_recorder`: PASS
- `android\tests\test_input_demo_state_trace_compare.ps1`: PASS
- `android\tests\test_input_demo_rng_trace_compare.ps1`: PASS
- `android\run-code-quality.ps1 -Fix -Paths ...`: PASS