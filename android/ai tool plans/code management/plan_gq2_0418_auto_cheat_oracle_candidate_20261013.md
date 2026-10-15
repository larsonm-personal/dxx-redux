# Automatic cheat recording oracle candidate, 2026-10-13

Diagnosis-only candidate from GQ2-CHUNK-0418 current consumer reconciliation. No implementation or runtime acceptance. Canonical finding/remediation admission and final deduplication remain pending.

Problem: android/tests/test_auto_level_demos.ps1 L73-L80 searches each simulation frame's events array for command=cheat and requires legacy numeric IDs. Current keyboard handlers in d1/main/gamecntl.c L1225-L1232 and d2/main/gamecntl.c L1801-L1803 invoke weapon_cheat_execute_live. Its shared implementation stages enable_cheats/grant_all_weapons; input_demo_recorder_stage_direct_command_event L246-L253 stores typed commands in pending_input.operations. The format5 events path explicitly rejects direct commands. Successful current recording therefore cannot satisfy this legacy events-array assertion, which occurs before independent replay. This is a source witness for a false failure, not a freshly observed device failure.

Keep the correction in the Android test owner. Read ordered command operations in dispatch order from the exported format5 recording, including the terminal input tail where applicable. Assert the exact expected commands and named grants for the script and active gameplay profile. Preserve explicit D1 activation versus D2 implicit activation, repeated refill order, and D1 versus D2 grant behavior. Imported D1 uses D1 gameplay policy despite a D2 executable; determine its default grant from the selected edition/assets and scripted keys rather than treating the executable name as the profile. Do not add a compatibility reader for legacy command events or synthesize evidence from replay actuals.

Retain the two-recording/level-start/no-checkpoint assertions, Original-routing policy, carried inventory, next-level cheat-enabled/zero-score checks, immutable exported input and strict independent SIM-RNG/state replay. Existing public all-weapons fixtures remain a separate fidelity contract.

Implementation acceptance, after diagnosis-only restrictions are lifted:

- Inspect the real exported dispatch/tail command stream and correct only the current-format assertion
- Run the maintained automatic-level cheat scenario on supported native D1, native D2 and imported-D1 profiles, with relevant retail/shareware assets and optional state diagnostics
- Require the exact activation/refill order, intended gameplay grant and independent replay for both exported levels
- Demonstrate rejection of missing/reordered/wrong-profile operations using disposable metadata without weakening RNG or terminal comparisons; preserve originals
- Run scoped code quality and repository-required checks appropriate to the eventual implementation

Deduplication searches for automatic-level/ordered-input/format5/cheat-oracle descriptions found no matching canonical root in the two active ledgers. Existing BR0209 outcome, BR0233 publication, BR0293 history, BR0294 probe activation, BR0651 frame identity and GQR0173 producer containment retain their distinct acceptance. These searches are not a complete global-root audit; resolve candidate ownership before canonical publication.

Only a conditional correction plan is authored. No test/helper/product code changed, no test or device execution, no staging or commit.


Canonical admission0418: GQF0311OPEN/GQR0297TODO/PRIMARY47 admitted by publicationPASS3103;implementation and actual-entry runtime acceptance remain pending. Original candidate bytes are preserved in the immutable0418snapshot.
