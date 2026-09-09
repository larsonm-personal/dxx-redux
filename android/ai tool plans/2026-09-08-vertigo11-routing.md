# Vertigo 11 routing

Reproduce Vertigo 11's stalled switch approach, diagnose why the movement path ends before the requested target, retain a general fix in shared source where practical, and verify deterministic completion plus the eight-mission corpus. Keep TEW 9 and Counterstrike secret -5 deferred and do not edit outstanding_bugs.md.

## Diagnosis and changes

- Vertigo 11, Urgian Kiln, collected blue and gold keys but stalled before switch 8. The final recovery path ended at segment 296 instead of target 373
- Temporary path diagnostics showed the original path from 348 to 373 was complete. Contact with the gold-key door at 348:4 reversed the bot's motion, carrying it backward through 355, 354, 353 and 37 into 47. A later recovery path could no longer reach the switch
- The existing door-contact detector only accepted nearly head-on reversals. At the missed contact, actual/commanded velocity dot product was -48226, facing/goal was 55720, and velocity/goal was -53558 (fixed-point unit 65536). The old 15/16 thresholds excluded this oblique impact
- Changed the existing D2 Guide-Bot steering checks to recognize opposing motion while facing the goal. It still requires an openable door on the full-radius ray before braking, does not open doors itself, and resumes when the approach clears. This is a minimal change to existing steering code, with no mission constants or header implementation
- The first corpus run exposed an Obsidian 5 timing dependency: a reached partial recovery path ended at a now-closed keyed door, but the simulator still waited for the distant blue-key segment. Shared simulation logic now treats a physically reached partial endpoint as a frontier and refreshes the same semantic objective through the existing bounded door-interaction mechanism
- Obsidian 5 now refreshes navigation from 122 to frontier 49, opens the keyed door, extends back to 122 and completes. The route and collision checks remain in force
- All temporary diagnostics were removed. The user's concurrent edit to outstanding_bugs.md was left alone

## Validation

- New `test_vertigo_level11_door_contact.ps1`: repeated identical confirmed results, all seven objectives, 5432 frames
- New `test_obsidian_level5_partial_frontier.ps1`: repeated completion and explicit closed-door frontier extension checks; focused raw runs match exactly at 4552 frames
- Full eight-mission corpus `android/temp/core_eight_vertigo11_verified`: 179/203 pass versus baseline 178/203, with no previously passing level lost. Vertigo improves from 17/24 to 18/24; Obsidian remains 18/18
- Refreshed changed normalized simulation results for all eight missions. Route metadata does not change because these corrections affect movement and frontier execution
- All 49 native tests pass; `temp/vertigo11_native_tests.log`
- Scoped formatting/lint passes; `temp/vertigo11_final_quality.log` (upstream D2 files follow their existing style and are excluded by the repository clang-format filter)
- Build logs: `temp/vertigo11_fix_build.log`, `temp/vertigo11_frontier_build.log`; final verification build is `temp/vertigo11_final_build.log`
- Final D1/D2 builds pass, all 49 native tests pass again, and both new integration scripts pass after formatting: `temp/vertigo11_final_native_tests.log`, `temp/vertigo11_final_integration.log`, `temp/obsidian5_final_integration.log`

Remaining Vertigo failures: 4, 6, 14, 16, secret -2 and secret -3. TEW 9 and Counterstrike secret -5 remain deferred.
