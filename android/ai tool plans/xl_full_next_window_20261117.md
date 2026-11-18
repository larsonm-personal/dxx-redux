# Next XL compatibility window

Use the per-case evidence in `../XL_compat/evidence/full-readiness-20261117.json` to choose cases by archive, descriptor and ordinal. A first blocker is the first operation that stopped the ordinary engine. Clearing it can expose a second dependency. The current short test starts the player at Hotshot, uses seed 1, and requires 120 unassisted frames at 60 Hz on the advertised level.

## 1. Open original HOGs with duplicate member names

This is the first implementation priority. Five distributions account for 58 failed starts. Belial System contributes 17 originals in each of two releases, Pluton contributes ten in each of two releases, and Klassics contributes four. A read-only run of the pinned XL archive reader found the advertised member in every case. The returned bytes contained 42 classic levels and 16 XL version-27 levels. All 16 passed static level inspection. Actual resource preparation and simulation still have to run.

Use the original `CLevelHeader::Read`, `CHogFile::QuickSort` and `CHogFile::BinSearch` from reference revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`. The sorted index selects a particular duplicate by its position after the source sort. In the 40-entry Belial HOG, the selected `13final.hxm` starts at byte 2926798 and has length 27620. Another entry with the same name contains different robot definitions. The two Belial releases also select different text payloads. The execution ledger records the measured offsets for both releases.

1. Represent all bounded member spans and the source-selected entry for each lookup key. Use the same selection in the native inspector, PhysFS archive and Android import, stream and music readers. On non-seekable Android input, use bounded seekable storage or a second verified pass. Reject truncated headers, invalid lengths and reads outside the selected span.
2. Compare names, offsets, lengths and content hashes with the reference. Cover long names, case comparison, equal and unequal duplicate payloads, odd and even index sizes, and unique-name archives. Verify the actual HXM, text, MIDI and level reads from the consumer that uses each resource.
3. Rerun all 58 original cases, the frozen 8 and 64, and unrelated stock and XL archives on desktop and Android. Use source-selected original bytes throughout. Publish recovered cases and every newly exposed blocker.

Estimate one focused implementation window for native selection and the first original runs. Allow a second window if Android streaming or music requires separate ownership. Reserve credit for cases that complete the ordinary probe.

## 2. Resolve advertised level files and descriptor identity

The complete distribution scan found no matching level in 32 of the 34 missing-file cases. This includes 27 FFYL ordinals. Keep the advertised ordinal and original hash in the inventory. Recover a genuine release from an identified source before adding it to the private assets repo. Record the new distribution and retest both releases independently.

Two cases have possible alternatives inside the published archive. The `danwentz/dffusion.mn2` descriptor's filename occurs in a different author's sibling HOG. The Nocturnal D1 descriptor differs in spelling from a sibling HOG containing the requested level. Check the reference mission resolver and release metadata for the intended binding. Bay32 separately completes 120 frames from a loose level and reports a path-qualified filename. Test mission identity, actual source bytes and same-basename collisions before changing the proof format. Keep the historical result visible.

## 3. Qualify textures, lighting and models by their actual consumers

| Family                                      | First implementation and evidence required                                                                                                                                                                                                                                                  |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Animated POGs and scoped wall clocks        | Select one hash-pinned original per clock type. Trace frame count, source bitmap, interval, orientation and clock ownership through the existing effect or wall-clip owner. Compare pixels and frame sequence, including pause, restore, level exit and a second mission with equal counts. |
| Texture memory budget                       | The 12 Pumo Mines cases hit the aggregate decoded-pixel limit. Measure resident bytes by image and live dependency. Test bounded decoding or eviction with the actual renderer, ownership transfer and reload. Keep rejection viable on small Android heaps.                                |
| Light indices and CLR companions            | Compare the packed spans with the exact level/game version and record the first out-of-bounds entry. Qualify signedness, range and source-defined skipping per profile. For CLR, compare embedded and companion precedence, per-vertex color, save/restore and release to stock.            |
| OOF, ASE, POG directories, HXM and base HAM | Check the actual failing chunk or directory and the incoming resource bank. Compare offsets, counts, selected submodel, visibility and bitmap bindings. Prove independent incoming ownership, discard and outgoing continuation on failed validation.                                       |

Run all members of each repaired class. Use one rendered desktop and one constrained Android example per new visual owner. A decoded bitmap alone leaves timing and rendering unverified.

## 4. Expand supported layout and gameplay semantics

Group the remaining level versions by both level and game header. Compare actual records against the matching source revision. Qualify offset boundaries, side corner compaction, lights, factories, walls and trigger target counts before admitting a pair. Handle classic levels embedded in an XL descriptor separately, including mission-owned HAM, HXM and POG transitions.

For non-hexahedral geometry, first compare source side topology, normals, collision planes and portals in an original map. For station functions, fluid physics, reactor and object triggers, inspect the calling source and trace one native transition across save and restore. Treat independently invalid segment, wall, object and target references as retained rejections until the source behavior is known. For particles and lightning, compare their time, resource and visibility owners. Re-run every unchanged case that exhibited the same first diagnostic.

## 5. Close simulation and continuation defects

Wolf Pack stops after 82 frames and Pyrotechnica after 104. Both logs record lethal robot-weapon damage with neutral input. Reproduce their starts in a reference build before classifying a gameplay discrepancy. Keep the same difficulty, input, watchdog and criterion. Zugzwang reaches the loaded phase before its 45-second watchdog. Identify the first stalled simulation operation on an isolated run before changing a budget.

For same-process checkpoint replay, capture and restore the gameplay connected-distance cache, including unreachable entries, age, ring position and level identity. Test cold and warm KAK-XL, KKR-XL and D2-XL level 10, saves with no cache state, and a stock transition. KAK-XL has a traced miss-versus-hit divergence. D2-XL level 10 has a measured frame-1 SIM divergence whose cause still needs tracing. Compare object, world, native XL, SIM and FX evidence. Track the pre-existing Android starfield FX difference and the G2 corrupt-trailer energy change separately.

## Completion and estimates

Start with archive lookup. Packaging provenance can proceed independently of engine support. Implement animation and resource ownership next, then a source-proven format family. Within each window retain time for the exact binary, local and hosted fixed gates, affected-family receipts, Android state restoration and final-push CI. The frozen eight already pass. A finite unconditional estimate for the complete inventory would require the missing distributions and evidence about blockers that are currently hidden.
