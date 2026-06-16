# Player Spew No Expire - 2026-06-16

## Context
- Add a multiplayer QoL option so player-dropped spew persists.
- Normal robot drops and other non-player powerups should keep their current timeout behavior.

## Plan
- [x] Trace current multiplayer QoL option flow from launcher to engine
- [x] Add launcher/default/resume/lobby plumbing for the new option
- [x] Add engine-side netgame option in D1 and D2
- [x] Apply no-expire behavior only to player-dropped powerups
- [x] Run scoped validation
