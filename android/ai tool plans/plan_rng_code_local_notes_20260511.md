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
- [completed] continue the audit through the next AI and spawn tranche in `fuelcen.c`, `aipath.c`, `ai.c` and `ai2.c`, plus the single-player portions of `escort.c`
- [completed] continue the audit through the helper tranche in `game.c`, `gameseg.c`, `physics.c`, and the remaining SIM-owned pathing and awareness rolls in `d2/main/ai.c`
- [completed] continue the audit from the next unresolved single-player RNG file after the current helper and `d2/main/ai.c` tranche
- [in-progress] continue from the remaining multiplayer-only and test-helper RNG sites after the single-player frontier closed

## Notes
- user requested a very short justification note at each concrete RNG call location
- keep comments minimal and avoid changing behavior
- newly confirmed this tranche: `d1/main/cntrlcen.c` and `d2/main/cntrlcen.c` reactor follow-up shot rolls stay on SIM because they decide whether extra live shots are fired
- newly confirmed this tranche: D2 `collide.c` buddy hint delay and hint-text rolls move to `d_rand_fx()` because they only affect guidebot message timing and wording after invulnerable boss hits
- newly confirmed this tranche: D2 `collide.c` player-death smart-mine arm roll stays on SIM because it can spawn a real armed smart mine in single player
- newly confirmed this tranche: D2 `fireball.c` spawned-shield and shield/energy suppression rolls stay on SIM because they decide whether real pickups exist
- newly confirmed this tranche: the audited `d1/main/laser.c` and `d2/main/laser.c` speed, lifetime, spread, target-selection, and recoil-rotation rolls all stay on SIM because they change live projectile or ship behavior
- newly confirmed this tranche: the audited `fuelcen.c`, `aipath.c`, `ai.c` and `ai2.c`, plus the single-player `escort.c` rolls, all stay on SIM because they change live spawn timing, AI routing, combat aim, boss spawn and teleport behavior, or real player inventory
- newly confirmed this tranche: the audited `game.c`, `gameseg.c`, and `physics.c` helper rolls stay on SIM because they change live damage, object lifetime, placement, or simulation timing
- newly confirmed this tranche: the remaining audited `d2/main/ai.c` rolls stay on SIM because they change live agitation pathing, flare cadence, wake/search transitions, and awareness/agitation state
- newly confirmed this tranche: the remaining D1 AI mirror sites, the lingering D2 `ai2.c` green-guy movement gate, and the long `fireball.c`/`laser.c` block tail calls all stay on SIM because they continue to change live AI movement, search transitions, debris behavior, and projectile spread
- current frontier after the closing scan: remaining unlabeled hits are multiplayer-only RNG, explicit path-test helper code in `aipath.c`, or `d_rand` text appearing inside comments
