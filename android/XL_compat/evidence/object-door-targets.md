# Object open-door targets without walls

Original Pumo `tulev2.rl2` has an object-attached open-door action on owner 51
with links to 909/4, 910/5, 907/4 and 911/4. At least one geometrically valid
target has no wall. Admission previously rejected the entire level for this.
Its particle requirements remain a separate gate.

At pinned D2X-XL revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`main/trigger.cpp`'s `CTrigger::DoLink` calls `CSegment::ToggleWall` for every
link. `main/segment.cpp`'s `ToggleWall` returns when `Wall(nSide)` is null.
It therefore skips that target and continues through later links. The trigger
file is already hash-pinned by the object reference target:
`74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852`.
The segment file SHA256 is
`eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d`.

Redux's `xl_object_classic_action` already dispatches open-door actions through
`do_link` and `wall_toggle`, which returns for wall index -1. Only XL admission
changes: action 0 may target a valid segment/side with wall -1. Other negative
or out-of-range indices, invalid coordinates and unaudited action types remain
rejected. No runtime dispatch or core engine code changes.

The existing native object-event owner adds a separate derived `edoor`
fixture with empty/real/empty links. It checks the later paired door opens,
one-shot owner removal and stock-to-save restoration. The existing `events`
fixture and Android owner's inputs retain their previous bytes. Portable
inventory controls cover the admission boundary across four existing layouts.

Portable inventory contracts and the full native object-event owner pass,
including the existing source reference comparisons. The Android x86_64 APK
build passes. The unchanged `events.hog` hash is
`80a7d98f01c138716f3650f388216ffca0e45ab829b263726bb77d75f2014cd6`.
No new device gameplay claim is made from this admission-only change.

The first native attempt used a nine-character derived mission filename, which
the native save header truncates to eight characters. Renaming that test input
to `edoor` fixed the restore failure without a production change.

Receipts: `temp/xl-skybox-20261031/admission-test.log`,
`empty-door-native.log`, and `android-empty-door-build.log`. Detailed native and
source receipts are under
`temp/xl-particles-20261030/contracts-windows64/object-events`.
