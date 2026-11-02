# Original PMC3 blue-key access

This scenario extends the opening checks through the original blue-key access
chain. It uses normal laser shots, door contact and item collision. Fixed local
approaches isolate the gates; no equipment grant, trigger dispatch, exit dispatch
or mission edit is used. The remote-door corridor is one continuous flight.

## Authored sequence

- A no-key control fires and flies against blue door 72 at 819/5 and remains on
  its near side. The key is not initially present in the player's inventory.
- Switch 6 at 1317/0 is shot from adjoining full-sized room 1308. It unlocks
  walls 172/171 between 1588 and 1593. A locked-door control precedes activation,
  followed by normal fire and flight through the unlocked door.
- Switch 7 at 955/3 is shot from full-sized room 956. It unlocks walls 174/173
  between 1589 and 1594, again with a locked control and subsequent flight.
  The thin cap segment 994 is not used as a player placement.
- Contact trigger 0 is on ordinary door 83 at 821/2. After a locked control at
  remote door 75, continuous forward flight starts in room 821, opens the first
  door, crosses its contact trigger and passes remote door 75 without another
  pose. The trace records player actor 0 and shot 0. Trigger flag 1 suppresses
  its message; it is not the one-shot flag.
- Switch 1 at 884/5 unlocks wall 76 at 850/4. After its locked control, normal
  flight passes the unlocked door. A local approach from room 880 enters room
  879 and collects original blue-key object 31 by collision.
- The key and unlock state are saved at the blue-door approach in room 819.
  Warm and fresh-process loads check the persisted key, switch actors and unlocks,
  then normal fire and flight cross the blue door. Door animation bits may differ
  while unlocked, so the saved unlock checks admit flags 16 through 23.

## Evidence and limits

Exact APK source 8b80ef220dbda2d9481c21611084ef21f5413808, version 28810,
SHA-256 89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069.
The exploratory chain passed 89/89 in 74.804 seconds. The ship collected the key
alive with 35.370 shields; normal robot damage remained active. A separate
15/15 warm save/load and keyed-door flight passed in 42.086 seconds. The rendered
key-pickup endpoint was inspected.

The earned blue checkpoint archive contains 26 profile files, SHA-256
a16c44c07dfe42b97b9c62e5786c8eed450cde09f5ec5835e8e2aff39aba2da5.
The original opening archive remains available separately. Published bundles and
original mission assets are unchanged.

Use test_xl_pog_runtime.ps1 with -Pmc3Blue -Pmc3Opening -Pmc2Escape -PmcEscape
-PmcReactorPlaytest -PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow
and the original resources. The owner preserves the profile and uses explicit
device identity with automatic recovery disabled.

Gold/red-key routes, full connected traversal, human controls and physical ARM
devices remain outside this scenario. These checks require no runtime fix.

## Protected qualification

The exact APK passed 325/325 warm steps in 429.243 seconds and 14/14
fresh-process steps in 46.895 seconds. PIDs 16488 and 18861 differ. Both
rendered endpoints were inspected. The ship remained alive with 0.714 shields
in both runs, a narrow survival margin that does not qualify robust combat or
the next encounter. Before/after profile and resource hash lists match, SHA-256
671dd638ec9ed2de34f0949bdc7d8307636a1dcd04bb63abebf6d31e9eec296c.
Receipts: temp/xl-original-pmc3-blue. The owner restored the starting profile.
