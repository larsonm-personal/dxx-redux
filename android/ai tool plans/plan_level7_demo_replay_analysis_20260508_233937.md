# Level 7 replay analysis 2026-05-08 233937

## Goal
- determine why the new D2 level 7 demo desyncs
- check whether the failure closes out one of the existing replay theories covered by the current AI and impact instrumentation

## Steps
- [completed] locate the exact replay artifact directory and inspect result, state, and rng trace outputs
- [completed] identify the earliest meaningful drift from the recorded result and replay traces
- [completed] compare the first drift window against existing AI, visibility, awareness, agitation, and impact-related instrumentation
- [completed] decide whether one existing theory is confirmed, rejected, or still unresolved
- [completed] widen recorder weapon lifecycle coverage so near-player robot-owned non-homing projectiles no longer hide between robot_fire and player hit
- [completed] validate the D2 host build and a separate headless replay smoke test after the instrumentation change

## Notes
- user-observed symptom: replay appears to diverge around frame 2000 +/- 200 with an impact or explosion possibly perturbing player view angle so later shots miss
- cheap check: inspect the embedded and actual RNG traces plus any existing replay probe logs around frames 1800-2200 to see whether the first hidden drift matches an already instrumented branch
- first meaningful drift is at frame 2180, visible at the frame 2181 checkpoint boundary
- recording at gt 27088057 shows weapon 214 hitting a wall in segment 204, then three awareness-gate RNG uses and the expected twin player shot creation
- replay at the same gt logs player_blast_damage, player_weapon_hit, and shield_change from weapon 122 / explosion 134 / killer robot 110, with shields dropping from 187 to 186
- the replay-only RNG burst is not generic explosion creation; it is the three d_rand calls in object_create_explosion_sub() used for robot rotthrust flash stun in d2/main/fireball.c lines 246-248
- frame 2181 pose still matches the recording exactly while shields already differ, and pose / aim drift starts by frame 2182, so the later aiming problem is downstream fallout from the replay-only blast path
- the same robot 110 shot fired at gt 27064464 also exists in the recording as weapon 122 and later hits the player at gt 27130000, so the hidden divergence is in the projectile lifecycle between fire and first hit timing
- recorder coverage is now widened in d2/main/input_demo_hooks.c and d2/main/laser.c so weapon_create and probe_weapon_life events are emitted for robot-owned non-homing weapons when the projectile or parent robot is in the player segment or within i2f(40) of the player
- validation after the change: .\run-windows-build.ps1 -Target d2 completed successfully, and .\android\tests\run_input_demo_replay.ps1 -DemoPath .\android\regression_demos\d2_descent2_level3_20260506_223956.dximdemo -Runner headless-console -TraceState -TraceRng -HeadlessConsoleOutput 1 finished with RESULT: PASS
- current conclusion: the existing impact / explosion theory is confirmed for this demo, and the remaining unknown is now reduced to the projectile path drift before the replay-only early hit rather than the hit / blast branch itself