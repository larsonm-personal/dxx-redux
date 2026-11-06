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

## Historical substituted-resource qualification

Build `8b80ef220dbda2d9481c21611084ef21f5413808`, version 28810, has six matching
D1/D2 library markers. APK SHA-256:
`89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069`.
The installed APK matches, and all 25 backed-up pilot/save/settings files match
after installation.

The adjoining-room normal selector replay passes 60/60 steps in 43.599 seconds;
the same sequence failed at its fourth selection before the fix. The fresh
protected scenario passes 175/175 warm steps in 263.665 seconds and 59/59 cold
steps in 43.410 seconds, with PIDs 19103 and 20708. The warm checkpoint retains
all selected states in segment 2071. After cold loading, further normal shots
change the settings again, and the player flies through the newly unlocked
door to segment 189, z=588.4249, alive with 80.6474 shields. No cheats or
invulnerability are active. The rendered endpoint was inspected.

Both protected hash lists have SHA-256
`a469457aa471b05f7919c6ade5a3f8f00fc037b56ad7dd6b31024ba573e4bf10`.
Historical receipts are in `temp/xl-overnight-20261104/prior-pmc2-selectors` and the immutable selector
bundle, including traces and exact scripts. The ZIP SHA-256 is
`e4b1b6d698131956664a9d96fe3fffbdaeaf4a6e7cdb91a36b6447dd18c1094b`.

Previously serialized broken panels remain broken in old saves; start a new game or use an
unfired checkpoint on the new build. Existing saves and published bundles are
preserved rather than rewritten.

## Full-original Android qualification, November 3 2026

The existing protected owner also passes with full original Pumo HAM/HXM/VHAM/
ASE/OOF resources and no Vertigo substitution. Invoke `-Pmc2Selectors -PmcEscape
-PmcReactorPlaytest -PmcInnerGates -PmcPlaytest -OriginalHub -MissionFlow` with the
original resource arguments. The earned original inner-gate/reactor/boost route
enters PMC2 normally before the same selector and physical-gate controls run.
Only ordinary save/load and pause requests are used by the automation hooks.

Retained/installed APK29471, source `ec7715ffa5cd34865b701aa2152ee42f2a51b018`,
package com.dxxredux.app, SHA256
`da070ebaacf95ea9be2f95fbd630ef5db1ccea0e24cf9108bbb6822c96a0661d`:
187/187warm steps in146.054seconds,run `fbf85c966103464bb4044b4cbaece215`;
59/59fresh-process steps in24.164seconds,run `5bc0497d89354bf79c8690c47b741d8f`.
Game PIDs15541/16565 differ. Selector states survive save/load, normal shots change
them afterward, and ordinary door flight ends at segment189,
(53.2336,-921.2298,589.4221),alive,70.3824player shields/85.5453energy,
zero objectives,no bosses. The paused endpoint frame was inspected.

Protected manifests both SHA256
`160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`.
Receipts: `temp/xl-original-pmc2-selectors`. Original resource identities are the
same as [the earned original PMC1 escape](pmc-escape.md). No APK build/install
and no physical-phone writes. This is the owned Android14 software emulator
DxxPhoneFog20261102, protected legacy staging and local fixed approaches, not
connected human navigation, separate launcher UI qualification, original PMC2
escape, later PMC3 progression or complete visual/audio acceptance.
