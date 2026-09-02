# GuideBot route door flare aiming and speed

## Goal

Remove the Counterstrike level 1 keyed-door hesitation, aim route-confirmation
flares at the actual door surface, and raise the verification actor's movement
speed from 140% to 160% of normal.

## Phases

- [x] Trace the current frontier wait, door interaction, flare, and speed paths
- [x] Select visible upcoming path and keyed-frontier doors as explicit flare targets
- [x] Replan immediately when the frontier door begins opening, retaining collision interaction as backup
- [x] Increase the route-confirmation movement multiplier from 140% to 160%
- [x] Add focused regression coverage and verify Counterstrike level 1 in headed/headless-compatible engine code
- [x] Run scoped code quality, Windows build, and tests
