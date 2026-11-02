# Original Pumo gameplay playtest

The latest verified bundle is [pumo-selector-playtest-20261102.zip](../../temp/pumo-selector-playtest-20261102.zip).
Its [README](../../temp/pumo-selector-playtest-20261102/README.md) and
[manifest](../../temp/pumo-selector-playtest-20261102/manifest.json) record installation,
exact hashes and validation. Original mission content is unchanged.

Source: `8b80ef220dbda2d9481c21611084ef21f5413808`, stacked through
[PR308](https://github.com/larsonm-personal/dxx-redux/pull/308). All six D1/D2
libraries across ARM64, ARMv7 and x86_64 match this commit and version code 28810,
which updates 28800 normally. APK SHA-256:
`89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069`.
ZIP SHA-256:
`e4b1b6d698131956664a9d96fe3fffbdaeaf4a6e7cdb91a36b6447dd18c1094b`.

This build repairs [repeatable PMC2 selectors](evidence/pmc2-selectors.md).
Previously, a first laser hit destroyed a permanent control panel. Changing a
setting and trying to select the earlier choice again could therefore leave
its route locked. The panel and its light now remain intact, as in the pinned
XL source, while normal impact and trigger activation still occur. Ordinary
destructible panels retain their behavior.

It retains [difficulty-dependent boss weak spots](evidence/pmc-boss-combat.md),
[final-HXM boss binding](evidence/hxm-gameplay.md) and the
[reactor/objective correction](evidence/reactor-objectives.md). The original PMC1
reactor remains alongside its six countdown bosses; destroying it does not start
the final countdown while bosses remain. XL's enabled authored exit has no
universal boss-clear lock. No objectives, weapons, keys or health are granted.

## Install and start playing

Use your existing retail Descent 2 data. Launch the app and create a pilot once,
then enable USB debugging and select the device serial shown by `adb devices`.
From the repository root:

```powershell
pwsh -NoProfile -File android/helpers/install_pumo_playtest.ps1 `
  -Serial YOUR_DEVICE_SERIAL -Bundle temp/pumo-selector-playtest-20261102
```

From an extracted ZIP, run `pwsh -NoProfile -File ./install_pumo_playtest.ps1
-Serial YOUR_DEVICE_SERIAL -Bundle .`. The installer verifies hashes, updates
the APK and backs up mission resources. It preserves pilots, saves, settings and
control mappings. Do not uninstall to resolve a signing mismatch.

Choose **Descent 2 > New Game > XL: [-Pumo Mines-] > level 1**, then a difficulty.
Start a new game or use an unfired checkpoint: old saves retain already-broken
panels, hidden reactors or stale boss registries from their earlier build.
Preserve those files with their previous bundle and use a separate new save slot.
The Temporal Hub's PMC route reaches PMC Data Storage Center, PMC Transmission
Control Station and Cybernetic Menaces Research Lab. Existing touch/controller
bindings apply. Use the pause menu for Quick Save/Load or named save slots;
normal Load Game resumes after closing the app.

## Verified scope

| Scenario on this exact APK       | Result            | Scope                                                                                                                                                                            |
| -------------------------------- | ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Repeated selector replay         | 60/60, 43.599s    | All seven controls, including returning to earlier settings; the same fourth selection failed before the fix                                                                     |
| Fresh protected original mission | 175/175, 263.665s | Normal hub entry, local PMC1 switch/key/reactor actions, continuous local boost escape, PMC2 selector changes, blocked-field control, physical field crossing and warm save/load |
| Fresh-process PMC2 load          | 59/59, 43.410s    | Saved gate settings, further selector changes, locked-door control and normal flight through the subsequently unlocked door                                                      |

PIDs 19103 and 20708 differ. All 25 backed-up profile files match after
installation, and protected before/after hashes match. The cold endpoint is
alive in segment 189, z=588.4249, with 80.6474 shields, no cheats and no
invulnerability. The rendered endpoint was inspected. Receipts, traces and
scripts are included in the bundle and `temp/xl-original-pmc2-selectors`.

Native production-collision controls fail before the fix and pass afterward for
static/animated panels, destruction animations and all four orientations. They
preserve permanent-panel textures and lights across repeated impacts while
ordinary panels break. Existing texture collision and disk-through-stock
restoration pass on both admitted XL layouts. These are controlled native checks.

The retained f1e73 combat bundle separately passed boss frontal immunity/rear
laser damage and exact health persistence (99 warm / 17 cold). Additional f1e73
[inner gates](evidence/pmc-inner-gates.md) passed 88 / 23, and its
[normal PMC1 escape](evidence/pmc-escape.md) passed 105 / 24. Earlier c638 checks
covered opening gates (76 / 15) and PMC3 continuity (115 / 21, controlled exits).
Those results belong to their stated builds; they are not new exact-APK claims.

Additional tests on the same 28810 APK qualify the [normal PMC2 escape](evidence/pmc2-escape.md):
155 warm steps and 23 fresh-process steps, including its contact-operated door,
four-boost flight, normal advance to PMC3 and destination save/load. These receipts
are supplementary; the published bundle remains unchanged.

The same APK also passed the [PMC3 finale](evidence/pmc3-finale.md), with 215
warm steps and 22 fresh-process steps. Normal grate-switch fire, outward and
return portals, reactor combat, countdown save/load and continuous final flight
through the corridor door reach the authored ending and main menu. Local fixed
approaches leave earlier access gates and connected level traversal unqualified.

Supplementary [PMC3 opening checks](evidence/pmc3-opening.md) passed 226 warm
steps and 26 fresh-process steps on the same APK. They cover normal shots through
the two shaft grates, closed/open flight controls, the connecting switch and door
unlock, and persistence of all 36 grate faces. Later key-access routes and full
connected traversal remain unqualified.

The [rendered GuideBot pilot](evidence/rendered-guidebot.md) confirms a bounded
hub route with an assisted, physics-driven companion. Companion activation of
the XL exit is unsupported. Normal-player probes fill that specific gap without
relaxing actor rules or dispatching exits directly.

## Boundaries and feedback

- Fixed local approaches and bounded escape flights do not qualify full human
  navigation, surviving every boss encounter or earned campaign completion
- PMC3 opening and finale receipts supplement the bundle; its later key-access
  routes remain unqualified
- Physical ARM, touch ergonomics, controller acceptance and device performance
  need user feedback; device tests here use an owned x86_64 emulator with host GPU
- Large levels and save restores can take tens of seconds on the emulator
- One 28810 launch stalled during graphics initialization with an automation
  broadcast ANR; four later menu launches and three clean quit/relaunch cycles
  passed. The original cause remains unconfirmed
- Gameplay and hazard visuals remain active. Optional high-resolution replacements,
  custom soundtrack and graphics-only parity work are deferred
- Existing broken-panel saves are preserved rather than rewritten; older saves
  can also fail asset-identity checks, so keep their previous builds

Report the build ID, mission/level, last ordinary action and whether the problem
survives a normal save/load. Prioritize crashes or lost data, progression,
controls/combat, save/load, then performance that prevents play.
