# Authored boss destination groups

The XL runtime now admits separate segment group labels with neutral owner -1.
Team/entropy ownership remains rejected. There is no mission-name exception and
no change to Redux boss globals or behavior. Original missions remain separately
qualified.

## Source semantics

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
CSegment::Read stores owner and group separately. The segment m_group consumers
in that source tree are boss SetupSegments and multiplayer entropy logic; menu
m_group fields are unrelated. Boss setup compares each candidate's label to its
home label independently of passability and size. All 256 labels retain distinct
identities. The default 255, decoded as -1, is a group rather than a wildcard.

The hash-pinned boss reference now also checks main/segment.cpp,
include/segment.h and gamemodes/entropy.cpp for this consumer audit. Their SHA256
values are respectively:

- eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d
- 4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667
- 91aa04d3afc77544918208a5ebee5bd5ac72bbc54ed71d634e9a850775535866

Inspection retains known nondefault groups as observations; nonneutral ownership
still produces an actionable unknown requirement. The browser exposes the count
of nondefault segment records as segment_groups, not a count of distinct labels.
The XL source snapshot owns these labels. Translation does not insert them into
Redux segment enums. Existing source identity, owner-only rebuilds and checked
schema-5 routes preserve them across disk/rewind restoration.

## Native and reference controls

The inventory contract checks every group byte across 15/33, 17/36 and 23/40:
768 preservation/admission/browser cases with ownership rejection retained.
Actual pinned boss SetupSegments matches the production traversal on 512 label
comparisons, in addition to the previous 20 graph/capacity and 24 native transport
cases. MSVC and warning-free Linux GCC pass. This compares traversal and recorded
engine queries, not a complete reference engine or independent geometry solver.

The bossgrp fixture derives from the existing enlarged two-boss Neptune fixture.
All existing walls are opened; segment 70 has group 1, segment 84 has group 2,
and other segments use default -1. Real native passability and boss-size checks
produce larger lists when only the group predicate is removed. Therefore a
one-room list cannot pass merely because a closed wall or size restriction
isolates the boss.

Authored damage and autoplay-master transport rebuild only the relevant boss's
list. Subsequent calls to the teleport selector used by AI stay in the home
group across 29 seeds per case. Further transport joins another explicit group
and then the default group, which remains bounded rather than becoming a
wildcard. A saved cross-group route, accompanied by a valid event-state mutation,
rejects with the entire serialized extension unchanged. Disk restoration through
stock retains source groups and earned routes; resumed selection remains bounded.

All 22 focused native regressions pass in 123.96 seconds. All 12 private-policy
checks pass. The private original receipt now requires xl_boss_groups.

Fixture HOG SHA256:
9a3e99c1d4a9ed79305889c27806e588e54ba28639aadd89ccd0b6296c8575ba

Descriptor SHA256:
e554ca52fa55d119ddd03bf27fa7b03ec8cc25da90a59797fd7461cff7904f17

## Android and original evidence

Android passes all 48 steps on emulator-5554: authored damage/master transport,
separate group labels and destination lists, three asynchronous checkpoint
roundtrips, a different-difficulty stock transition and ordinary disk restore.
Protected files match before and after; the app is force-stopped after restoring
them. Native selection controls establish the group boundary; Android's short
resumed-AI interval is not evidence that a natural AI teleport occurred.

Run ID: a44324176a874098ae884dde94e0af68, elapsed 147199 ms.
Acceptance APK SHA256:
251bf4f12335d456bf5d7df4c50b861a6bd8e3a19fd13697bac237c8525c6f06

Reproduce with
android/tests/test_xl_boss_spawning_runtime.ps1 -Groups and the pinned derived
HOG/descriptor on an isolated serial. The owned script checks destination_group
beside the home and cached segment list. The runner backs up and restores the
selected mission and protected player/save/configuration files. Controlled poses
are used; no autonomous navigation is attempted.

The seven spawn-bot and eleven equipment original-candidate reports are refreshed
with the current native inspector and pinned object-reader comparison. Supported
32/64 flags and neutral groups are no longer listed as unknown requirements.
Known group and reserved-data observations remain visible, with bounded counts
and examples. Every runtime_qualified field remains false. Layout, capacity,
effects, resources, other actions and ownership still need separate acceptance.
The report producer writes canonical LF JSON and preserves unchanged bytes/mtime.
Both automation catalogs pass: 395 top-level entries and 450 support scripts.

Logs are under temp/xl-boss-groups-20261027; Android receipts and protected-file
hashes are under temp/xl-boss-groups-android. These controls do not qualify entropy,
multiplayer, expanded capacities, full arbitrary AI or another original mission.
