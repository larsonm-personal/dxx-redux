# Original PMC3 shield resources and gold-key access

The blue-key qualification survived with a narrow shield margin. This extended
scenario collects two original shield pickups through normal flight, then checks
the gold-key alcove and keyed door. Fixed local approaches remain explicit; no
health grant, invulnerability, equipment grant, trigger dispatch or content edit
is used. It is not a complete human traversal or combat qualification.

## Authored resources and route

- Original shield object 27 is in room 1566, near the level entrance. The player
  flies from adjoining room 1565 into it. The protected extended run collects it
  before the blue-key section so that the added work does not depend on surviving
  that section with less than one shield point.
- Original shield object 15 is in room 720, accessible after the blue-key route.
  The player flies from adjoining room 718 into it. Both pickups must be present
  before collection and absent afterward. Their consumed state is checked again
  after warm and fresh-process loads. The fixed local setup pose is established
  before unpausing each supply approach; travel between the approaches is not
  claimed as normal navigation.
- The no-gold control fires and flies toward wall 110 at 1190/4, remaining before
  the door plane at z=46. The gold key is not initially in the inventory.
- The historical exterior approach in room 761 at (-20, -392.3207, 302), facing west,
  flies into the original alcove and collects gold-key object 33. The player is
  not placed at the key or inside its chamber. The thin nearby segments were
  audited for clearance before choosing this approach.
- The player saves at the gold-door approach in room 1190, restores both keys
  and consumed pickups, then uses normal fire/flight to pass the door beyond
  z=55. The red key remains absent and the reactor remains alive.

An early exploratory selection confused energy ID 1 with shield ID 2. Objects
10/14 were collected normally but increased energy, not shields; that trace is
retained separately and excluded from this scenario. The corrected selection
uses the engine's powerup.h definitions and original objects 27/15.

## Historical substituted-resource exact-build evidence

APK source 8b80ef220dbda2d9481c21611084ef21f5413808, version 28810, SHA-256
89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069.

| Probe                                     | Result            | Boundary                                                                   |
| ----------------------------------------- | ----------------- | -------------------------------------------------------------------------- |
| Original shield collection                | 19/19, 44.029s    | Normal pickup collision; shields 35.370 to 79.971 with damage active       |
| No-gold door control and gold pickup      | 15/15, 8.043s     | Fixed exterior approach, then flight; alive with 75.925 shields            |
| Warm key/resource restore and door flight | 17/17, 46.544s    | Both keys persist, both shield items remain consumed                       |
| Protected extended chain                  | 380/380, 500.105s | Includes corrected opening sweeps; alive with 74.643 shields               |
| Fresh-process gold restore                | 16/16, 44.977s    | Both keys and consumed pickups persist; normal door flight, 75.630 shields |

The protected run used game PIDs 27449 and 30186. Both endpoint frames were
inspected. Before/after profile and resource hash lists match, SHA-256
540570408b017dfe61d079cd48019843665bfb4ceb4993d5ea59242e5ed571dc.
Receipts: temp/xl-overnight-20261104/prior-pmc3-gold. The starting gold profile was restored.

Two preparation failures, before the new gold steps, exposed intermittent
single-position shots at the opening shafts. Their receipts remain retained.
The corrected ancestor uses bounded normal firing sweeps from valid upper rooms;
both passed focused reload checks and this complete extended chain. Historical
opening and blue step counts describe their earlier preparation scenarios.

The earned gold checkpoint archive contains 26 profile files, SHA-256
38da8b0914c1b205953dbc95a41437bd8fe4d83df6719883032220c1652daec3.
Earlier opening and blue checkpoints, original assets and all published bundles
remain preserved.

Use test_xl_pog_runtime.ps1 with -Pmc3Gold -Pmc3Blue -Pmc3Opening -Pmc2Escape
-PmcEscape -PmcReactorPlaytest -PmcSwitchPlaytest -PmcPlaytest -OriginalHub
-MissionFlow and the original resources. The owner preserves the profile,
verifies device identity and disables automatic emulator recovery.

The red-key route, remaining control-room branches, complete navigation, human
controls and physical ARM devices remain outside this scenario. No runtime fix
is required for the actions covered here.

## Full-original combined key qualification, November 4 2026

The protected original route now passes392/392warm steps in290.422seconds,
run `211b4852115b4c8b9da8c0381e2ee297`, and16/16fresh-process steps in19.164seconds,
run `3df400f5e47242faa17c35a80286028f`. PIDs24726/26464 differ. This includes
ordinary hub/PMC1/PMC2 exits, PMC3 opening switches, original shield pickups27/15,
blue/gold key acquisition and normal keyed-door flight. Warm saves/loads exercise
both keys; the cold check restores both keys and consumed pickups and repeats
gold-door flight. The reactor remains alive; no red key is acquired.

Two original-resource preparation attempts remain separate failures: switch7
missed the old centered aim (288/392), then the old gold approach missed its
pickup (371/392). In the latter the player remained alive at66.44shields and
slid above the alcove, ending in2200. For the4.6-radius full ship, the old
room761 pose(-20,-392.3207,302) has only3.6units clearance to the sloped wall.
The revised exterior pose(-20,-391.3207,300),still room761 and facing west, has
5.6units clearance. Normal thrust collects original key33 in825ms. The player is
not placed in key room754 or at the item. See pmc3-gold-approach-audit.json and
preserved pmc3-keys-panel7-timeout/pmc3-keys-gold-miss under temp/xl-overnight-20261104.
No health/key grants, invulnerability, collider or mission changes were used.

Exact retained/installed APK29471, com.dxxredux.app, source
`ec7715ffa5cd34865b701aa2152ee42f2a51b018`, SHA256
`da070ebaacf95ea9be2f95fbd630ef5db1ccea0e24cf9108bbb6822c96a0661d`;
all six native IDs `dxx-build:ec7715ffa5cd34865b701aa2152ee42f2a51b018:29471`.
Full original HAM/HXM/VHAM/ASE/OOF and camera/effects resources as in the
[original PMC1 escape](pmc-escape.md), no Vertigo substitution. No APK build/install.
Owned Android14 software AVD DxxPhoneFog20261102, protected legacy staging.
Protected before/after manifests both SHA256
`160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`.
Both failed preparations also restored exactly.

Final cold position:segment2300,(-80,-587.4680,96.9420),alive,54.7166shields,
59.5758energy,both keys,one objective,no countdown. Endpoint frame inspected.
Receipts: `temp/xl-original-pmc3-gold`. Invoke the owner with Pmc3Gold/Pmc3Blue/
Pmc3Opening/Pmc2Escape/PmcEscape/PmcReactorPlaytest/PmcInnerGates/PmcPlaytest/
OriginalHub/MissionFlow and original resource arguments. This does not qualify
connected human navigation, red-key/remaining branches, reactor/ending, all14
mission levels, current-head native changes, separate launcher UI workflow,
physical phones or full visual/audio fidelity.
