# Original Pumo gameplay playtest

The latest verified bundle is [pumo-boss-playtest-20261102.zip](../../temp/pumo-boss-playtest-20261102.zip).
Its [README](../../temp/pumo-boss-playtest-20261102/README.md) and
[manifest](../../temp/pumo-boss-playtest-20261102/manifest.json) record installation,
exact hashes and validation. Original mission content is unchanged.

Source: `c63885a409055938330680fc63bf6bdabea1cbdd`, stacked through
[PR 301](https://github.com/larsonm-personal/dxx-redux/pull/301).
All six D1/D2 libraries across three ABIs match this commit and version code
28790, which updates the previous 28780 bundle normally. APK SHA256:
`27b34b03b4bbb2c98661945b9628788c5967a1de288704f1ea4c941cb5c6864b`.
The APK contains ARM64, ARMv7 and x86_64; device validation uses x86_64.

This build [registers bosses after final mission robot definitions](evidence/hxm-gameplay.md).
Custom bosses now receive their lifecycle, while PMC3 no longer retains eleven
stale base-game boss entries. Native combat/death/save controls and original PMC3
warm/fresh-process checks cover this change. It also
[restores the authored reactor beside XL bosses](evidence/reactor-objectives.md)
and defers final countdown until all required bosses and reactors finish. The
older 28770 build hid PMC1's reactor and could start countdown after one boss death.
Normal starter-laser combat now destroys that reactor while six bosses remain
required, without starting countdown. The bounded test also covers ordinary hub
entry, switches, key pickup, gated flight, and warm/fresh-process persistence.
Fixed poses select interactions; no objectives, weapons or keys are granted.

## Install and start playing

Use your existing retail Descent 2 data. Launch the app and create a pilot once,
then enable USB debugging and select the device serial shown by `adb devices`.
From the repository root:

```powershell
pwsh -NoProfile -File android/helpers/install_pumo_playtest.ps1 `
  -Serial YOUR_DEVICE_SERIAL -Bundle temp/pumo-boss-playtest-20261102
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

| Scenario on this exact APK     | Result  | Scope                                                                                                                           |
| ------------------------------ | ------- | ------------------------------------------------------------------------------------------------------------------------------- |
| Normal hub and PMC1 objectives | 81/81   | Laser/thrust hub entry, locked-door negative control, switch shot, red-key collision, keyed gate, reactor combat and warm saves |
| Fresh-process PMC1 load        | 15/15   | Saved switch/key/reactor state, six bosses still required, no countdown, normal flight through opened gate                      |
| Original PMC3 continuity       | 115/115 | Cameras/static actors, fixed lightning, combat, restart/save, controlled authored ending, stock switch and return               |
| Fresh-process PMC3 load        | 21/21   | Matching player/85-actor checkpoint, empty boss registry, one reactor, monitors/lightning and ordinary fire                     |

Protected pilot/save/settings hashes match after installation and every scenario.
Fresh-process game PIDs differ; the checkpoint comparison passes. Rotating camera
orientation is excluded only from that cross-process comparison because restore
can advance before pause. Paused disk-restore tests compare orientation exactly.

The PMC1 objective scenario uses ordinary input for activation and does not
dispatch exits. Its fixed poses do not qualify the connecting gates between local
checks; an inventory/runtime audit identifies additional locked barriers. The broader PMC3 continuity scenario still dispatches authored
exits directly. Neither proves human traversal, full boss combat or earned
campaign completion.
There is no remaining admission gate along the checked PMC route. Other routes
have not received this complete gameplay scenario. Earlier protected checks cover
controlled transitions through Mineral Valley's ending/interlude and level-14
detour; they are not full combat/traversal qualification.

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
- [PR301 checks](https://github.com/larsonm-personal/dxx-redux/pull/301/checks)
  refer to the exact APK source head; pending CI is not a pass

For feedback, record the level, actions, expected/actual behavior, device/control
type and nearest save. Export launcher Advanced logs for crashes or progression
blocks. Prioritize crashes/data loss, progression, controls/combat and save/load.

## Earlier bundles

Published ZIPs remain unchanged for existing checkpoints:

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
