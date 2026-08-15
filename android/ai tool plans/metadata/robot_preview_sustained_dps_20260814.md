# Robot Preview Sustained DPS

## Objective

Calculate and display each previewed robot's infinite-horizon damage per second using loaded robot and weapon data, assuming every damaging shot lands.

## Plan

- [x] Trace D1 and D2 robot firing cooldown, rapid-fire, multi-gun, secondary-weapon, and random-spread behavior.
- [x] Define sustained DPS rules for ranged, melee, mine-dropper, and non-firing roles, including random-distribution averages.
- [x] Implement the calculation next to the native robot preview attack summary so HAM/HXM data remains the source of truth.
- [x] Show total DPS and a compact breakdown in the upper-left attack text.
- [x] Expose DPS inputs and results through preview introspection.
- [x] Extend the emulator integration test with independent DPS consistency checks for D1, D2, non-firing, mine-dropper, and dual-weapon robots.
- [x] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator tests.

## Findings

- Direct projectile damage is the difficulty-scaled weapon strength stored in the loaded weapon table.
- D1 and D2 primary weapons fire one projectile per attack event. Multiple gun points rotate the origin and do not multiply the damage of one event.
- A rapid-fire cycle contains `max(1, rapidfire_count)` attacks, one full firing wait, and a short wait of `min(0.125 seconds, firing_wait / 2)` between the other attacks.
- D2 sniper behavior advances its rapid-fire counter with probability 0.5. Its infinite-horizon average therefore takes twice as many attacks to complete the same rapid-fire cycle.
- D2 secondary weapons have an independent `firing_wait2` channel, so sustained primary and secondary DPS add together.
- Melee contact deals `difficulty + 1` shield damage and uses the primary firing timer. Mine droppers use their engine mine-drop interval and the loaded mine strength. Non-firing roles have zero DPS.
- The calculation excludes misses, approach and visibility delays, splash-radius damage, smart children, and energy drain. It is labeled as direct damage so those assumptions are visible.

## Validation

- Scoped C/C++ and PowerShell formatting and lint checks passed.
- Android `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21.
- Windows D1 and D2 builds passed.
- Windows CTest passed: D1 33/33 and D2 40/40.
- Emulator robot preview tests passed for D1 robot 0, D2 robot 0, non-firing D2 robot 7, mine-dropping D2 robot 10, and sniper/dual-weapon D2 robot 36.
- D2 robot 36 reported the expected randomized primary timing average plus an independent secondary channel, with both channels included in total DPS.
