# Sphere red-key campaign fixtures, 2026-11-20

Three registered stages extend the earned Gauss checkpoint through resupply (17 phases), the carrier fight and drop (15 phases), and recovery of the red key (25 phases). Each stage consumes the previous fixture's native save. Selecting the recovery test with CTest schedules eight tests starting at the first level. Each new stage retains the 1800-second deadline and each native phase retains the existing timeout.

The fixed route checks the pilot's segment, score, kills, gold and red key flags, three lives and an inactive reactor. It tracks carrier 314, signature 315, before and during the fight. It checks the authored drop in the world and the red key in the player's inventory after collection. The route uses portal steering and visible-target aiming. Every phase rejects position assists and fixed-view aiming.

Linux Debug passed all 57 new phases from the previously earned Gauss checkpoint. The pilot finished in segment 2500 with score 46850. The [phase receipts](../XL_compat/evidence/sphere-red-key-campaign-20261120.json) bind every incoming save, outgoing save, executed plan, engine result, detailed state and trace. Windows full-chain CTest was in progress at publication. Frozen sampled load/tick readiness remains 4/8.
