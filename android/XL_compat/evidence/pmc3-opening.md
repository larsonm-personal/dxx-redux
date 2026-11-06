# Original PMC3 opening access

This scenario closes an access gap left by the local finale checks. The original
Cybernetic Menaces Research Lab requires two shaft switches, a connecting-wall
switch and a door unlock before the next control area. All activation uses normal
laser fire and all tested crossings use flight input. Fixed local approaches are
explicit; no trigger dispatch, exit dispatch, key grant or mission edit is used.

## Authored sequence

- Masters 10 and 11 sit below the two shafts, at 288/3 and 550/3. Their normal
  laser hits dispatch children 95 and 93, opening nine grates each. Safe firing
  approaches are full-sized rooms 205 and 467, aimed down the shaft. The test first
  flies against each closed grate, then shoots its panel and crosses the opened
  wall. All 36 wall faces are checked after save/load.
- Switch 9 at 572/5 opens the connecting walls 197/198 at 310/4 and 306/5.
  A closed-wall flight control precedes activation. The subsequent normal shot
  at switch 8 at 310/5 comes through the opened connection from room 306.
- Switch 8 unlocks only wall 170 at 1587/0. Wall 171 belongs to the next door,
  at 1593/2, and remains locked. The reverse face of wall 170 is wall 169.
  The scenario first checks the locked door, then saves the unlocked approach,
  restores it and fires/flies through the correct side. The fresh-process replay
  also crosses both opened shaft grates and the connecting wall.

The engine-exported graph guided selection of approaches. Its abstract room
connectivity is not navigation evidence: tiny decorative segments can be connected
but cannot fit the player. An initial pose in the one-unit-thick grate segment
355 was invalid and drifted into the skybox; that failed probe is retained and is
not classified as a gameplay defect. The corrected approach uses room 205.

## Evidence and limits

Exact APK source 8b80ef220dbda2d9481c21611084ef21f5413808, version 28810,
SHA-256 89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069.
Individual corrected master probes passed 10/10 in 3.361 and 3.260 seconds.
The combined exploratory sequence passed 74/74 in 113.137 seconds, including
closed-wall controls and warm save/load. A nested scalar assertion in an earlier
probe was corrected to the supported direct equality form; the engine had already
produced the expected unlocked value.

The first protected run timed out at master 11 after 192/226 steps. Its profile
restoration still matched. The rendered view from the closed grate showed a bar
across the original firing line. An approach shifted two units in x within room
467 activated the panel normally in 3.243 seconds. The test uses that offset;
the failed trace is retained as a firing-line limitation, not an engine fix.

Use test_xl_pog_runtime.ps1 with -Pmc3Opening -Pmc2Escape -PmcEscape
-PmcReactorPlaytest -PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow
and the original resources. The protected owner verifies AVD identity, disables
automatic recovery and restores the starting profile. Receipts are supplementary
to the unchanged published selector bundle.

These checks do not qualify the subsequent key routes, full connected traversal,
human controls or physical devices. No runtime change was needed for this chain.

## Protected qualification

The corrected protected run passed 226/226 warm steps in 363.149 seconds and
26/26 fresh-process steps in 48.283 seconds. PIDs 12742 and 14786 differ.
Both rendered endpoints were inspected. All 36 grate faces, connecting walls,
master activations and the intended unlock persisted; normal flight repeated
through the door, both shafts and the connecting wall. Before/after profile and
resource hash lists match, SHA-256
9985ea381758a99808306501eb7e312511cdf735588016dd1cc2b9ef921c4c40.
Receipts: temp/xl-original-pmc3-opening. The saved starting profile was restored.

## Bounded firing-sweep follow-up

A later extended gold-key run again missed master 11 from the single offset
approach (192/372 steps), with the player alive and restoration hashes matching.
The opening probe now sweeps three valid upper-room firing approaches: x offsets
+2, -2 and 0 in room 467, aimed at the same original panel. Ordinary laser fire
continues across two 1.5-second bursts before the final activation wait. This
changes test positioning, not collision or trigger acceptance; it remains a
controlled local shot check, not human aiming or navigation evidence.

The sweep passed 18/18 in 52.060 seconds and a normal checkpoint reload repeat
passed 15/15 in 54.966 seconds, including a three-second timing variation. The
226/26 protected receipts above describe the earlier single-approach scenario.
Updated descendant qualification must retain its own step counts and exact head.

The next protected descendant attempt stopped at the other shaft (master 10,
178/376), before reaching the new gold checks. The player remained alive in room
205 with 91.216 shields and the profile restoration hash matched. Master 10 now
uses the same bounded +2/-2/0 x-offset firing sweep from room 205, aimed at panel
288/3. Its focused normal-activation check passed 18/18 in 46.230 seconds, ending
with 98 shields, actor 0, shot 1 and doorway 12 open. No trigger or collision
rules changed. The failed descendant receipt remains separate from qualification.

## Full-original warm opening coverage

The original-resource392-step [blue/gold-key route](pmc3-gold-key.md) now passes
the complete current opening sequence with its bounded normal shaft firing
approaches, connecting switches and physical gate crossings. Exact binary/run
identities and restoration are recorded there. Its16-step cold phase qualifies
the later earned-key checkpoint, not a separate cold replay of all opening gates.
No engine, trigger or original-content change was required for these interactions.
