# Original Pumo gameplay playtest

The latest verified bundle is [pumo-final-playtest-20261101.zip](../../temp/pumo-final-playtest-20261101.zip).
Its [README](../../temp/pumo-final-playtest-20261101/README.md) and
[manifest](../../temp/pumo-final-playtest-20261101/manifest.json) record installation,
exact hashes and validation. Original mission content is unchanged.

Source: `888ded31795e2074c46566c688ac813902a60cce`, stacked through
[PR 292](https://github.com/larsonm-personal/dxx-redux/pull/292).
All three native ABI build IDs match this commit. APK SHA256:
`eca76d97e4e886bbdf251a78690f1c371314ef14c208b5ba4899f89bff6f771a`.
The APK contains ARM64, ARMv7 and x86_64; device validation uses x86_64.

The same APK also passed the [normal hub activation and restore probe](evidence/hub-activation.md):
ordinary laser/thrust through the PMC route, repeated after restoring a hub save.
This uses a fixed approach pose, with no exit dispatch or objective grants.
It does not establish human traversal or physical-device validation.

## Install and start playing

Use your existing retail Descent 2 data. Launch the app and create a pilot once,
then enable USB debugging and select the device serial shown by `adb devices`.
From the repository root:

```powershell
pwsh -NoProfile -File android/helpers/install_pumo_playtest.ps1 `
  -Serial YOUR_DEVICE_SERIAL -Bundle temp/pumo-final-playtest-20261101
```

From an extracted ZIP, run `pwsh -NoProfile -File ./install_pumo_playtest.ps1
-Serial YOUR_DEVICE_SERIAL -Bundle .` in the extracted directory. The installer
checks hashes, updates the APK and backs up mission resources. It preserves
pilots, saves, settings and control mappings. Do not uninstall to resolve a
signature mismatch; uninstalling removes app data.

Choose **Descent 2 > New Game > XL: [-Pumo Mines-] > level 1**, then a difficulty.
Start a new game on this build. From **Temporal Hub**, the **PMC route** reaches
**PMC Data Storage Center**, **PMC Transmission Control Station**, then
**Cybernetic Menaces Research Lab** and the authored ending. The **Mineral Valley**
route leads to **Crauphel Scilab**. Existing touch/controller bindings apply.
Use the pause menu for Quick Save/Load or named Save Game/Load Game slots.
Normal Load Game can resume a checkpoint after closing the app.

## Verified scope

| Scenario on this exact APK | Result  | Scope                                                                                                                                                       |
| -------------------------- | ------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Opening                    | 64/64   | Ordinary movement/fire, damage, death/restart, quick save, menu load, original pickup and robot combat, authored exit                                       |
| Original PMC1/PMC2         | 117/117 | Camera resources/monitors, ordinary controls/combat, restart/save and context recovery                                                                      |
| Original PMC3 continuity   | 115/115 | Five cameras, 80 static robots, fixed lightning, combat, death/restart, save/load, authored ending, stock-mission switch and return, quick-save preparation |
| Fresh-process PMC3 load    | 21/21   | Normal Load Game in a new process, matching player/85-actor checkpoint, monitors/lightning and ordinary fire after restore                                  |

Protected pilot/save/settings hashes match after installation and every scenario.
Fresh-process game PIDs differ; the checkpoint comparison passes. Rotating camera
orientation is excluded only from that cross-process comparison because restore
can advance before pause. Paused disk-restore tests compare orientation exactly.

Automation uses fixed firing positions and dispatches authored exits directly.
It does not prove human traversal of every door, objective, secret or route.
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
- CI status is recorded in the [handoff](../../temp/xl-playtest-20261101/final_handoff.md);
  pending CI is not a pass

For feedback, record the level, actions, expected/actual behavior, device/control
type and nearest save. Export launcher Advanced logs for crashes or progression
blocks. Prioritize crashes/data loss, progression, controls/combat and save/load.

## Earlier bundles

Published ZIPs remain unchanged for existing checkpoints:

- [PMC3](../../temp/pumo-pmc3-playtest-20261101/README.md): 64 opening, 117 PMC1/PMC2
  and 86 PMC3 checks; predates the stock-switch/fresh-process additions
- [PMC1/PMC2](../../temp/pumo-pmc2-playtest-20261101/README.md): PMC3 still gated
- [Opening](../../temp/pumo-playtest-20261101/README.md): original starting slice;
  PMC camera levels still gated

The older PMC3 APK was built from the later source checkpoint while its embedded
build label still named its parent. Its APK hash identifies that published
artifact. The latest bundle above has matching source and embedded commit IDs.
