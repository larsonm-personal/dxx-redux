# Next XL readiness window

The Windows census recovered 57 advertised ordinals. Its 179 remaining failures fall into the following non-overlapping groups. A case belongs to the first operation that stopped the ordinary 120-frame run. The pinned Linux survey independently reported the same 95 pinned failures. [Case ledger](../XL_compat/evidence/hog-readiness-20261117.json)

| First blocker                            | Cases | Work that can change the result                                                                                                                            |
| ---------------------------------------- | ----: | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Texture, lighting and model resources    |    71 | Separate animated replacements (23), light indices (16), decoded texture budget (12), companion colors (9), POG directories (4), OOF (4) and ASE (3).      |
| Distribution and mission identity        |    39 | Identify genuine missing inputs (34), trace descriptor selection (4), and verify the advertised versus resolved filename (1).                              |
| Geometry and gameplay rules              |    39 | Start with segment functions (12) and reactor triggers (5). Group the rest by the exact field and version recorded in the ledger.                          |
| Level format and descriptor combinations |    25 | Compare the 14 unsupported layouts by level and game header. Exercise the 11 classic ordinals in XL descriptors with their actual mission-owned resources. |
| Simulation and watchdog                  |     5 | Trace the four stopped simulations from the last complete frame. Profile the timed-out case with the original watchdog and publish its last phase.         |

## First implementation window

Start with classic ordinals in XL descriptors. Six belong to Chaotic Theory, two to its XL variant, two to Vignette XL and one to Belial System XL. Belial's secret 2, `cargoca.rl2`, has a passing classic-descriptor counterpart. Compare geometry and payload hashes before using a counterpart as a control. Record the source mission's palette, model, robot, texture and sound owners before entry, inside the level, after exit and after a failed transition.

1. Freeze the 11 case IDs and the current 120-frame results. In the pinned XL source, trace how descriptor mode and the level header each affect resource loading. Determine which resource checks still apply to a classic payload. Keep every unsupported dependency visible.
2. Implement the smallest supported combination in the existing level and resource owners. Test ordinary entry, return to an XL level, transition to stock, disk/rewind across both directions and failed incoming validation. Include same-size resource banks with different contents.
3. Measure all 11, both fixed cohorts and the full census on the same candidate. Run a real rendered capture and fresh replay on the classic-in-XL original and an independent control. Check Android ownership and restoration of the previous APK and player state. Publish recovered ordinals, unchanged failures, new first blockers and any regressions.

One focused window can establish the reference behavior, test Belial secret 2 and measure the 11. Their actual source dependencies determine how many pass. A forecast for full readiness also depends on recovering genuine missing distributions.

## Follow-on work

For distribution identity, the previous full archive scan found no advertised payload in 32 of the 34 missing cases. It includes 27 FFYL ordinals. Keep those releases measured. Establish provenance for any alternate release before admitting it to the private fixture repository. For the two possible sibling-HOG matches, verify the reference engine's mission ownership and lookup order. Verify main/secret addressing on the filename mismatch. [Distribution audit and prior plan](xl_full_next_window_20261117.md)

For the 71 visual failures, take one clocked POG and one light-index case first. Compare source time, frame sequence, texture orientation and the specific light span or index. Verify pixels and timing in a rendered game and on Android across pause, restore and mission change. Profile Pumo's live decoded textures before designing an eviction policy. Use a real constrained-memory run to validate memory ownership.

Treat replay continuation as a separate workstream. Both new recordings match their cold ordinary renderer and full RNG trace. Same-process Belial replay diverges at frame one, and Klassics retains tick and metadata differences after 120 frames. Compare first differing RNG call sites and cache state with the previous KAK-XL evidence. Preserve the capture and failed receipts. A warmed process and a fresh one must each match on their own evidence.

For Android, repeat the standalone Sphere transition through level 3 in a rendered Debug game. It met the live-state assertion before the mine-connectivity dialog paused the game. Capture the segment and side reported by the native connection check, compare them with the original level and reference reader, and keep the dialog visible in the control. Exercise the next ordinal both in sequence and from a fresh session. A passing short desktop run cannot identify this dialog because its headless renderer has no gameplay dialog UI.

For the level-pack Sphere secret, address its known prerequisite first. New Game rejects the authored level 1 robot file `sp-01.HXM` before it can request secret 1. Compare that exact file against the source robot codec and independently check which definitions need normalization, then repeat an ordinary level 1 start and a separate secret transition. For D.I: Monolevel, preserve the first self-termination trace alongside the successful fresh start and capture the Android lifecycle call site on a repeat. The emulator reported about 1.91 GiB of RAM. The measurements do not establish memory pressure as the cause.
