## Robot total runtime spawn fix

### Goal
- Prevent the optional robots killed/total stat from showing killed greater than total when robots are created during gameplay.

### Plan
- [x] Find the HUD or overlay code that exposes robots killed/total.
- [x] Trace how robot totals and kills are initialized and updated in d1 and d2.
- [x] Update runtime robot creation paths so the total increases for matcen spawns, contained robot drops, boss actions, and similar robot creation.
- [x] Add or adjust focused validation where practical.
- [x] Run targeted checks and update this plan with results.

### Notes
- The HUD and Android coop overlay both use `Players[pnum].num_robots_level` as the live denominator.
- Matcen, boss gate, and respawn robot paths already increment `num_robots_level` and `num_robots_total`.
- `drop_powerup()` can create `OBJ_ROBOT` children from destroyed robots and previously did not update the counters, allowing kills to exceed the denominator.
- Added counter increments after successful robot child creation in both `d1/main/fireball.c` and `d2/main/fireball.c`.
- Validation: `./run-windows-build.ps1` completed for D1 and D2.
- Validation: explicit CTest runs found no registered tests in `buildd1` or `buildd2`.
- Validation: all 10 standalone `buildd1/maths/test_*.exe` and all 10 standalone `buildd2/maths/test_*.exe` passed.
