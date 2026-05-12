# RNG code-local notes 2026-05-11

## Goal
- add short code-local notes at concrete `d_rand()` and `d_rand_fx()` sites that have already been classified
- keep the note text brief and directly tied to why the call is SIM-owned or FX-owned
- continue the single-player RNG audit from the next unresolved fireball/weapon path

## Steps
- [completed] inspect which code-local RNG notes already landed in the current D1/D2 files
- [completed] patch missing short notes at the already-confirmed SIM and FX sites in D1 and D2
- [completed] run focused validation on the touched files after the first comment-only edits
- [completed] continue the audit through the D2 smart-mine death-arm roll and the adjacent D2 fireball pickup-existence gates
- [completed] continue the audit through the current single-player `laser.c` tranche and add SIM notes for projectile speed variance, flare lifetime, vulcan spread, smart-child target choice, and recoil rotation in D1/D2
- [in-progress] continue the audit from the next unresolved single-player RNG file after the current `laser.c` tranche

## Notes
- user requested a very short justification note at each concrete RNG call location
- keep comments minimal and avoid changing behavior
- newly confirmed this tranche: `d1/main/cntrlcen.c` and `d2/main/cntrlcen.c` reactor follow-up shot rolls stay on SIM because they decide whether extra live shots are fired
- newly confirmed this tranche: D2 `collide.c` buddy hint delay and hint-text rolls move to `d_rand_fx()` because they only affect guidebot message timing and wording after invulnerable boss hits
- newly confirmed this tranche: D2 `collide.c` player-death smart-mine arm roll stays on SIM because it can spawn a real armed smart mine in single player
- newly confirmed this tranche: D2 `fireball.c` spawned-shield and shield/energy suppression rolls stay on SIM because they decide whether real pickups exist
- newly confirmed this tranche: the audited `d1/main/laser.c` and `d2/main/laser.c` speed, lifetime, spread, target-selection, and recoil-rotation rolls all stay on SIM because they change live projectile or ship behavior
