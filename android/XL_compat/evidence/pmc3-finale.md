# Original PMC3 portal access, reactor and ending

This scenario checks ordinary player activation in the unchanged original
Cybernetic Menaces Research Lab. Fixed local approaches isolate the switch,
reactor and portal approach; the final portal/corridor flight has no position
changes after its start. It is not a connected human traversal of the whole level.
No exit dispatch, trigger dispatch, equipment grant or content edit is used.

## Authored route and activation

- Master 17 is a one-shot panel at segment 4147 side 3. Its normal laser hit
  dispatches child 74, opening walls 229/230 in front of the portal chamber.
  The firing approach is above grate 227/228 in room 4144, aimed through a visible
  gap, not inside the panel alcove. A closed-wall flight control precedes the shot.
- Four portal faces (18, 20, 21 and 22) share destination 4177 side 4. Forward
  flight from room 4077 enters the opened chamber and appears in the distant
  corridor facing forward. Individual traces have recorded 18 and 22 on this
  approach; the scenario checks the player's destination and facing, retaining
  the exact trigger trace rather than assuming a single boundary face.
- Reverse flight from the destination crosses return portal 19 at 4177/5,
  returning to 4099/2 and coasting outside the opened gate. Normal save/load
  preserves the opened wall and completed return contact.
- Reactor 294 at segment 3100 remains the sole countdown objective. A fixed
  firing approach in room 3090 uses the starting laser, with no invulnerability.
  Destroying it changes one objective to zero and starts the 90-second Trainee
  countdown. The gate and countdown are saved together before the portal flight.
- From that saved approach, forward input passes the portal, traverses the
  corridor and opens ordinary door 233/234 by contact. It then crosses exit 38
  at 4238/4. Destination value 983040 selects slot 15, beyond this original
  fourteen-level mission, and invokes its authored ending.

The final ending uses explicit normal screen advances: completion, ending
briefing, final score screen, then main menu. Generic skip_briefing keeps
sending Escape at the main menu and can cleanly close the engine; that helper
misuse is not a gameplay crash.

An earlier continuous portal-to-exit flight also completed with the reactor
still alive. This agrees with pinned XL source
2576a98b5de310e3ac7c9777df2677d1627b8bae, main/trigger.cpp::CTrigger::DoExit:
there is no universal reactor-clear guard. Reactor objectives govern countdown
accounting; the test does not invent an extra exit prerequisite.

## Exact-build exploratory evidence

APK source 8b80ef220dbda2d9481c21611084ef21f5413808, version 28810, SHA-256
89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069:

| Probe | Result | Boundary |
| --- | --- | --- |
| Reactor laser combat | 14/14, 52.047s | Fixed firing approach; alive with 98.687 shields |
| Warm countdown load and exit contact | 11/11, 37.444s | Fixed exit approach |
| Cold countdown load and explicit ending | 18/18, 47.161s | Normal exit contact and menu return |
| Grate-gap switch shot | 8/8, 5.894s | Normal laser hit from above the grate |
| Outward teleport | 9/9, 2.437s | Normal opened-wall flight, portal 18 actor 0/shot 0 |
| Return teleport | 8/8, 4.170s | Reverse flight, portal 19 actor 0/shot 0 |
| Gate/portal save and load | 10/10, 41.363s | Open gate and return contact persisted |
| Reactor, countdown load, portal/corridor exit | 26/26, 99.367s | Continuous final flight through the ordinary door |

The first portal probe timed out only because it required face 18 while the
player used equivalent face 22 and reached the exit alive. A subsequent short
centered firing burst missed the grate opening; the corrected approach offsets
the aim through the visible gap and waits for normal activation. These are
probe constraints, not established gameplay defects. Their failure receipts
remain alongside the successful probes under temp/xl-traversal-20261102.

The earned initial PMC3 archive contains 26 pilot/save/settings files, SHA-256
3ce82d7aba05b8528296daddaa19728a4f2e9c51fbb4ef3c7514d191dfd4ae1b.
The opened-gate checkpoint archive has SHA-256
7858ab267525d798722a880d92f7afe5298d2f1688d9bb72cce41fcf9d76a406.
Only the owned disposable AVD was used. Original assets and all published bundles
remain unchanged.

## Repeatable protected owner

Use test_xl_pog_runtime.ps1 with -Pmc3Finale -Pmc2Escape -PmcEscape
-PmcReactorPlaytest -PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow
and all original resources. The owner preserves the profile, checks AVD identity
and disables automatic emulator recovery. It records the warm completion trace
and frame, then loads the preflight countdown checkpoint in a fresh process,
repeats the portal/corridor flight and checks the ending's main-menu return.

Full mission navigation, all three key-access routes, human control acceptance,
physical ARM devices and complete surviving boss encounters remain outside this
scenario. The rendered companion's unsupported XL exit activation remains a
planner boundary; these normal-player checks do not relax actor semantics.

## Protected qualification

The exact 28810 APK passed 215/215 warm steps in 403.278 seconds and 22/22
fresh-process steps in 51.892 seconds. Game PIDs 5025 and 7369 differ. The
completion and final main-menu frames were inspected. Before/after profile and
resource hash lists match, SHA-256
1e41ad8c1791a2978e26bea22293902736c40331439a8949a9cf2ec1e5403129.
Receipts are in temp/xl-original-pmc3-finale, supplementary to the immutable
selector bundle. The protected owner restored its starting profile.
