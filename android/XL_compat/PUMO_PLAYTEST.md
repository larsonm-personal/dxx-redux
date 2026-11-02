# Original Pumo opening playtest

The latest [PMC3 playtest bundle](../../temp/pumo-pmc3-playtest-20261101/README.md)
is `temp/pumo-pmc3-playtest-20261101.zip`. Its exact universal APK passed **64
opening, 117 PMC1/PMC2 and 86 PMC3 checks**, with protected hashes matching.
The original PMC route now reaches Cybernetic Menaces Research Lab and its
authored ending. Five camera monitors, 80 static robot restore states, fixed
lightning, ordinary combat, death/restart and background recovery were checked.
Start a new game and follow the PMC route from Temporal Hub. Fixed poses and
exit dispatch do not establish complete human traversal; see the bundle README
for install instructions, slow-load observations and remaining feedback needs.

Earlier bundles below remain available for existing checkpoints.

The verified [expanded PMC1/PMC2 bundle](../../temp/pumo-pmc2-playtest-20261101/README.md)
is ready for feedback: `temp/pumo-pmc2-playtest-20261101.zip`. Its exact universal
APK passed 64 opening checks and 117 PMC1/PMC2 checks, including ordinary controls,
combat, death/restart, disk save/load, camera monitors and background recovery.
Protected app-data hashes matched. Start a new game; PMC3 remains gated.

Static robot contact and omitted save payloads are now source-qualified. This
supersedes the provisional PMC1 bundle. Automated fixed poses and authored exit
dispatch do not establish full human traversal. Physical ARM devices and control
ergonomics still need feedback. See the expanded README for install instructions,
known issues and exact build/validation records.

The original opening-only bundle below remains available unchanged for existing
checkpoints. Its boundaries describe that older build.

## Build and install

Local bundle: `temp/pumo-playtest-20261101`. Runtime source:
`ea3e6f320fb513464dd45d1459401ee944b58583` (merged PR 277).
The universal debug APK contains ARM64, ARMv7 and x86_64 engines. Its SHA256 is
`e3a632f0e815cca20b4ddc4da202ee47edce5565d3a531645e60bcb21ec88341`.
The bundle manifest records the APK and runtime-resource archive hashes.

Use a device with USB debugging enabled and your existing retail Descent 2 data.
Launch Descent 2 and create a pilot once before running the installer. From the
repository root, substitute the serial shown by `adb devices`:

```powershell
pwsh -NoProfile -File android/helpers/install_pumo_playtest.ps1 `
  -Serial YOUR_DEVICE_SERIAL -Bundle temp/pumo-playtest-20261101
```

For the extracted ZIP, run `pwsh -File ./install_pumo_playtest.ps1 -Serial
YOUR_DEVICE_SERIAL -Bundle .` from the directory containing `manifest.json`.

The installer updates the debug APK, stages original mission resources, and keeps
a timestamped backup of any existing Pumo resources inside app-private files.
It does not replace pilots, saves, control mappings or settings. Do not uninstall
the app to resolve a signature mismatch: that would remove app data.

Open Descent 2, choose **New Game**, **XL: [-Pumo Mines-]**, starting level **1**
if prompted, then your preferred difficulty. Choose the **Mineral Valley** route
from **Temporal Hub**. It enters **Crauphel Scilab**. Existing touch/controller
bindings apply. Use the pause menu's Quick Save/Quick Load actions or named
Save Game/Load Game slots. After Abort Game, Load Game can resume the checkpoint.

## Verified scope

The 64-step Android run `63841fd3124044538b1e191493b754e7` passed in 170021 ms:

- Normal New Game and mission selection, ordinary starting equipment, cheats off
- Turn/thrust input, primary laser fire and energy use
- Damage, death, restart with one fewer life, and pre-death checkpoint recovery
- Quick Save, Abort Game, and Load Game through ordinary menu actions
- Original hub exit dispatch into Crauphel Scilab, without changing its trigger
- Original shield pickup through physics contact: shields rose from 75 to 102
- Ordinary aim/fire input killed original robot 36, with an earned kill and score
- Protected app-data hashes matched before and after the test

The pickup and combat checks start at fixed original-level positions. The exit
check dispatches the authored trigger directly. Neither establishes that a human
has traversed all doors, objectives, secrets or exits. The test ran on x86_64;
ARM variants compile but still need physical-device feedback.

Earlier protected checks cover authored transitions through Mineral Valley's
ending interlude in level 10, including the level-14 detour. Those checks are
controlled progression evidence, not a full combat or traversal qualification.

## Boundaries and known issues

- The hub's branch to level 11, **PMC Data Storage Center**, remains blocked by
  unsupported camera gameplay. Levels 12 and 13 also remain blocked. Keep a save
  before trying other hub branches; do not treat all 14 parsed entries as playable
- First load and save restoration can take tens of seconds on the emulator
- This local gameplay bundle includes original HOG/descriptor/sounds and required
  original effect atlases. Optional high-resolution models/textures and the custom
  soundtrack are not bundled. Existing gameplay/hazard rendering remains active
- Opening delivery PR 278 passed CI
- Full campaign, all weapons, all doors, secret routes, touch ergonomics, physical
  controllers and Android suspend/resume still need broader playtesting

For feedback, record level name, what you did, expected and actual behavior,
device/control type, and the nearest save. Attach exported logs from the launcher's
Advanced tab when reporting a crash, blocked progression or save/load problem.
