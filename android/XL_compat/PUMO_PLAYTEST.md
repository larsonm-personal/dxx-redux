# Original Pumo gameplay playtest

The latest verified bundle is [pumo-combat-playtest-20261102.zip](../../temp/pumo-combat-playtest-20261102.zip).
Its [README](../../temp/pumo-combat-playtest-20261102/README.md) and
[manifest](../../temp/pumo-combat-playtest-20261102/manifest.json) record installation,
exact hashes and validation. Original mission content is unchanged.

Source: `f1e73f5fd38a836515bf98ea369dbe1c2a788d7d`, stacked through
[PR304](https://github.com/larsonm-personal/dxx-redux/pull/304).
All six D1/D2 libraries across ARM64, ARMv7 and x86_64 match this commit and
version code 28800, which updates 28790 normally. APK SHA256:
`762670c549ded1050e107701d4b938f32d9b6a288b07790790d0ab4ad2c65118`.
Device validation uses x86_64; physical ARM remains untested.

This build corrects [boss weak-spot angles by difficulty](evidence/pmc-boss-combat.md).
Pinned XL widens the accepted hit area on Trainee/Rookie and narrows it on
Ace/Insane; this runtime had kept the Hotshot threshold at every difficulty.
The fix applies on XL level startup and save restoration, while stock behavior
remains unchanged. The protected original-mission test also proves frontal
immunity, rear starter-laser damage and exact saved boss-health persistence.

It retains the [final-definition boss binding fix](evidence/hxm-gameplay.md) and
[reactor/objective correction](evidence/reactor-objectives.md). Original PMC1's
reactor remains present alongside its six required bosses; destroying it does
not start the final countdown while bosses remain. PMC3 uses the final HXM
robot roles instead of retaining stale base-game boss entries. Fixed poses
select local interactions; no objectives, weapons, keys or health are granted.

## Install and start playing

Use your existing retail Descent 2 data. Launch the app and create a pilot once,
then enable USB debugging and select the device serial shown by `adb devices`.
From the repository root:

```powershell
pwsh -NoProfile -File android/helpers/install_pumo_playtest.ps1 `
  -Serial YOUR_DEVICE_SERIAL -Bundle temp/pumo-combat-playtest-20261102
```

From an extracted ZIP, run `pwsh -NoProfile -File ./install_pumo_playtest.ps1
-Serial YOUR_DEVICE_SERIAL -Bundle .` in the extracted directory. The installer
checks hashes, updates the APK and backs up mission resources. It preserves
pilots, saves, settings and control mappings. Do not uninstall to resolve a
signature mismatch; uninstalling removes app data.

Choose **Descent 2 > New Game > XL: [-Pumo Mines-] > level 1**, then a difficulty.
Start a new game on this build: old saves retain their serialized world, including
any previously hidden reactor or stale boss registry. Old development saves may
fail validation; preserve them with their earlier bundle and use a separate new
save slot. From **Temporal Hub**, the **PMC route** reaches
**PMC Data Storage Center**, **PMC Transmission Control Station**, then
**Cybernetic Menaces Research Lab** and the authored ending. The **Mineral Valley**
route leads to **Crauphel Scilab**. Existing touch/controller bindings apply.
Use the pause menu for Quick Save/Load or named Save Game/Load Game slots.
Normal Load Game can resume a checkpoint after closing the app.

## Verified scope

| Scenario on this exact APK       | Result          | Scope                                                                                                                                                           |
| -------------------------------- | --------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Normal hub and PMC1 local combat | 99/99, 214.396s | Hub laser/thrust entry, locked-door control, switch shot, red-key collision, keyed flight, reactor combat, boss frontal immunity/rear damage and warm save/load |
| Fresh-process PMC1 load          | 17/17, 32.429s  | Saved switch/key/reactor state, exact boss health, six objectives still required, no countdown and normal flight through the opened gate                        |

Boss 95 retains exactly 1112.387451171875 shields across the cold load. Game PIDs
9264 and 10597 differ. All 24 backed-up pilot/save/settings files match after
installation; the protected test's before/after hash lists also match. The cold
rendered endpoint was inspected. Receipts are in the bundle and in
`temp/xl-original-pmc-boss-combat`.

Native controls reproduce the pre-fix collision failure, then pass ten side-hit
decisions across five difficulties, disk restoration and stock isolation.
Existing boss lifecycle, death-roll persistence, mixed reactor/boss ordering,
save guards and the pinned source oracle also pass. These are controlled native
checks, not human combat.

The previous c638 APK passed PMC3 continuity (115 warm / 21 cold steps) and
[opening switches 0/1/4](evidence/pmc-opening-gates.md) with three local gate
crossings (76 / 15). Those unchanged scenarios were not repeated on this APK;
their results belong to c638 and its retained bundle. PMC3 continuity uses
controlled exits and does not prove earned progression.

The current normal-input scenario does not dispatch exits. Fixed poses do not
qualify connecting gates or full traversal. A longer exploratory normal-fire
probe killed one boss and reduced objectives from six to five, but the player
died. Full surviving boss combat and earned campaign completion remain
unqualified. Other long attacks lost aim and timed out; those failures are
retained rather than counted as passes.

The [rendered GuideBot pilot](evidence/rendered-guidebot.md) confirms a bounded
hub route with an assisted, physics-driven companion. Its authored PMC exit
activation is unsupported. It does not establish normal-player combat or
progression. Other original routes and physical-device input still need playtest
feedback.

## Known limits and feedback

- Physical ARM devices, touch ergonomics and controllers need user feedback
- Large-level loading/restoration took roughly 30-90 seconds on the emulator
- An earlier intermediate build had a launcher ANR; later clean scenarios passed
  recovery. The retained delayed trace does not establish its original cause
- Gameplay/hazard visuals remain active. Optional high-resolution replacements,
  the custom soundtrack and cosmetic parity work are deferred
- Older development saves are preserved but can fail asset-identity checks; keep
  the corresponding older bundle when resuming those saves
- Multiple-reactor firing and negative non-ending boss flags remain unqualified
- [PR304 checks](https://github.com/larsonm-personal/dxx-redux/pull/304/checks)
  refer to the exact APK source head; pending CI is not a pass

For feedback, record the level, actions, expected/actual behavior, device/control
type and nearest save. Export launcher Advanced logs for crashes or progression
blocks. Prioritize crashes/data loss, progression, controls/combat and save/load.

## Earlier bundles

Published ZIPs remain unchanged for existing checkpoints:

- [Boss-definition bundle](../../temp/pumo-boss-playtest-20261102/README.md), source
  `c63885a409055938330680fc63bf6bdabea1cbdd`, version 28790: 81/15 PMC1 and
  115/21 PMC3 checks, plus later 76/15 opening-gate checks. It predates the
  difficulty-dependent vulnerable-angle fix

- [Reactor bundle](../../temp/pumo-reactor-playtest-20261102/README.md), source
  `c94341f823c0c498ae3dc824cf4663f8fddf4d8d`, version 28780: the preceding
  81/15 PMC1 and 115/21 PMC3 checks; predates final-definition boss binding

- [Previous final bundle](../../temp/pumo-final-playtest-20261101/README.md), source
  `888ded31795e2074c46566c688ac813902a60cce`: 64 opening, 117 PMC1/PMC2,
  115 PMC3 and 21 fresh-process checks. It also passed the
  [hub restore](evidence/hub-activation.md) and [switch/key probes](evidence/pmc-switches.md).
  These earlier results belong to that APK; it retains the reactor/boss defect
- [PMC3](../../temp/pumo-pmc3-playtest-20261101/README.md): 64 opening, 117 PMC1/PMC2
  and 86 PMC3 checks; predates the stock-switch/fresh-process additions
- [PMC1/PMC2](../../temp/pumo-pmc2-playtest-20261101/README.md): PMC3 still gated
- [Opening](../../temp/pumo-playtest-20261101/README.md): original starting slice;
  PMC camera levels still gated

The older PMC3 APK was built from the later source checkpoint while its embedded
build label still named its parent. Its APK hash identifies that published
artifact. The latest bundle above has matching source and embedded commit IDs.
