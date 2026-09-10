# Conservative reactor escape warnings

- Record the actual reactor countdown and completed post-reactor simulation travel time
- Warn only for completed routes: nonpositive countdown with at least five seconds of travel, or travel exceeding both three times the countdown and the countdown plus thirty seconds
- Describe this as a likely timing problem in the simulated route, not proof that no player can escape
- Keep route status unchanged; append a note to simulation metadata alongside existing diagnostics
- Exclude built-in final levels where the engine disables the countdown
- Verify Crossfire L15, a normal campaign level, policy boundaries, and metadata note preservation

## Completed

- Native results record actual countdown and elapsed travel after countdown activation for completed escapes
- Normalized simulation metadata preserves this evidence and adds only the conservative advisory note; existing texture notes and route statuses are retained
- Crossfire L15 has 8.0 seconds of simulated escape travel against a zero-second countdown; checked-in simulation metadata refreshed
- Crossfire integration passes with two identical 6174-frame completions and the warning
- Counterstrike L1 completes without warning (1.483322 seconds against 45 seconds); built-in final L24 is correctly excluded from countdown warnings
- D2 build, all 49 CTests, simulation schema/reporting tests, and scoped formatting/lint pass
- Existing unrefreshed mission files acquire the measured evidence/warnings on their next simulation regeneration; no full corpus sweep was performed for this annotation change
