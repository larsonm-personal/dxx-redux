# Repeatable PMC2 selector panels

Original PMC2's Firewall Level, Encryption Method and Coding Mode controls are
permanent shot switches. A player must be able to change a selection and then
choose an earlier setting again. The old runtime destroyed each panel on its
first hit, so a later shot could no longer activate it. This can strand a player
with a locked route after experimenting with the controls.

On the published f1e73 APK, the normal-shot sequence 13, 14, 15, 13 fails on the
last selection: door 122 stays locked. The first three shots dispatch normally.
The same actual collision path fails a native regression with
"Permanent switch impact destroyed its panel or lighting". Original mission
bytes are unchanged; this is not an unsupported planner step.

## Source semantics and correction

Pinned XL revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`main/segment.cpp::CSegment::BlowupTexture`, checks the wall trigger's
TF_PERMANENT flag before subtracting light, assigning a destroyed texture or
starting its destruction animation. It still accepts the hit and produces its
normal impact explosion/sound. The pinned source file SHA-256 is
`eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d`.

A shared XL query now identifies permanent wall triggers. The small D2 collision
hook keeps their panel texture and light while retaining normal hit acceptance.
The query is inactive for stock levels. Ordinary destructible panels still
break. No mission content, selector outcomes or actor acceptance rules change.
D1 has no active XL wall-trigger runtime and keeps its existing collision path.

Native production-collision controls cover static destroyed textures, animated
textures, destruction animations and all four overlay orientations. Two hits on
each permanent panel retain its texture, geometry light values and light-removal
bits; ordinary panels still acquire the destroyed texture. Both admitted 23/40
and 27/40 layouts pass, along with existing texture dispatch, collision and
save/restore-through-stock checks. Before/after logs are under
`temp/xl-traversal-20261102/permanent-*`.

## Original-mission scenario

The protected `Pmc2Selectors` mode extends `PmcEscape`: normal hub entry,
local PMC1 switch/key/reactor combat and the normal boost exit lead to PMC2.
Local fixed poses aim from adjoining rooms at its selector panels. The sequence
changes all seven controls and returns to earlier selections. It checks the
closed forcefield before opening it and physically crosses afterward. Warm and
fresh-process saves must retain the selected gates; further shots after loading
must still change settings. A locked-door control precedes normal fire/thrust
through the subsequently unlocked door.

| Selector    | Normal result                                                           |
| ----------- | ----------------------------------------------------------------------- |
| 13 at 207/4 | Unlocks door 122; selects firewall texture 142                          |
| 14 at 208/4 | Locks door 122; selects firewall texture 141                            |
| 15 at 209/4 | Locks door 122; selects firewall texture 140                            |
| 16 at 19/4  | Locks door 122, opens fields 120/121, selects encryption texture 146    |
| 17 at 21/4  | Unlocks door 122, closes fields 120/121, selects encryption texture 145 |
| 18 at 113/4 | Unlocks 124 and locks 126; selects coding texture 149                   |
| 19 at 115/4 | Locks 124 and unlocks 126; selects coding texture 148                   |

Texture values are the authored child actions, not yet an Android pixel claim.
Ambient object enable/disable child dispatch is separate from audible playback
acceptance. The scenario does not establish connected human navigation, full
boss combat, a PMC2 exit or physical-device controls.

Android qualification of the committed fixed APK is pending. Previously
serialized broken panels remain broken in old saves; start a new game or use an
unfired checkpoint on the new build. Existing saves and published bundles are
preserved rather than rewritten.
