# Original Pumo gameplay playtest

The new [phone fog candidate](../../temp/pumo-phone-fog-playtest-20261102/README.md)
includes ARM64, ARMv7 and x86_64, the preserved loading/save and metadata fixes,
and verified direct scene depth for fog, soft particles and lightning.
[Manifest](../../temp/pumo-phone-fog-playtest-20261102/manifest.json) and
[ZIP](../../temp/pumo-phone-fog-playtest-20261102.zip) retain exact identities and evidence.

**Physical Samsung SM-S931U / Adreno 830 remains untested.** The earlier phone
build rejected required fog depth transfer. The owned SwiftShader emulator
reproduced the broken transfer and now passes the new capture path. This ARM
candidate is ready for a short live session; no alternative phone level is
claimed verified. See [depth evidence](evidence/fog-direct-depth.md).

Source: `4abb2b47dab5915d898959089fed8b826d669372`, version code **29080**.
All six native library IDs match. APK SHA-256:
`5d535b74c71de7a416a0f7f0555fb40e51215f59b69bd94796fce33f149f7788`.
Runtime archive SHA-256:
`f54be3a7f2b6a6755a85b22d52bcb947379f5d54ecde8df8d329230217db4bbd`.
Stack: [PR318](https://github.com/larsonm-personal/dxx-redux/pull/318),
[PR320](https://github.com/larsonm-personal/dxx-redux/pull/320),
[PR321](https://github.com/larsonm-personal/dxx-redux/pull/321).
Later branch commits repair CI inventories only; the APK remains pinned above.

Why: depth blits can report success while returning unchanged depth. The new
path qualifies direct clears and raster writes numerically, captures normal
geometry depth, and preserves fog, particle fading and lightning occlusion.
Indexed POG palette-key flags now match original XL so opaque walls write depth.
No mission rewrite, GPU-name exception or admission bypass is used. Direct scene
capture is single sampled even with 2x/4x presentation; equivalent scene MSAA is
not claimed.

Exact-APK qualification on owned Android 14 x86_64 SwiftShader:

| Check                                                                            | Result                                                   | Boundary                                                              |
| -------------------------------------------------------------------------------- | -------------------------------------------------------- | --------------------------------------------------------------------- |
| Original hub door and PMC exit, repeated after hub save/load                     | 36/36 PASS, 83.164s                                      | Normal laser/thrust; fixed local approaches                           |
| Fog geometry, save/rewind, stock transitions, MSAA presentation and context loss | 208/208 PASS, 160.083s                                   | Seven derived levels; controlled pixel/geometry checks                |
| Bundled installer                                                                | 14 protected files unchanged; installed APK hash matches | Owned emulator, not a physical phone                                  |
| Rendered GuideBot authored PMC exit                                              | Unsupported after 548 physics frames                     | Companion reached exit; XL rejected activation; no progression credit |

Supplementary [Pumo/stock save isolation](evidence/pumo-stock-save-isolation.md)
passes68 warm and39 fresh-process steps on that same APK. Both mission saves
restore distinct player state; all286 protected files match after the run.
The native load menu follows the last saved mission; it is not a combined list.

The previous [selector bundle](../../temp/pumo-selector-playtest-20261102/README.md)
and all older bundles remain historical baselines. This candidate retains
[repeatable selectors](evidence/pmc2-selectors.md),
[boss weak spots](evidence/pmc-boss-combat.md), [HXM binding](evidence/hxm-gameplay.md)
and [reactor/objective corrections](evidence/reactor-objectives.md).

## Install and start playing

Use your existing retail Descent 2 data. Launch the app and create a pilot once,
then enable USB debugging and select the device serial shown by `adb devices`.
From the repository root:

```powershell
pwsh -NoProfile -File android/helpers/install_pumo_playtest.ps1 `
  -Serial YOUR_DEVICE_SERIAL -Bundle temp/pumo-phone-fog-playtest-20261102
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

## Historical broader coverage (selector baseline 8b80ef22)

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
through the corridor door reach the authored ending and main menu. Those finale
checks use local fixed approaches; the opening checks below cover earlier gates.

Supplementary [PMC3 opening checks](evidence/pmc3-opening.md) passed 226 warm
steps and 26 fresh-process steps on the same APK. They cover normal shots through
the two shaft grates, closed/open flight controls, the connecting switch and door
unlock, and persistence of all 36 grate faces. Later key-access routes and full
connected traversal remain unqualified.

The [PMC3 blue-key chain](evidence/pmc3-blue-key.md) adds 325 warm and 14
fresh-process steps: two higher unlocks, continuous contact-trigger/remote-door
flight, the key-access switch, original key pickup and saved keyed-door flight.
The protected endpoint was alive with only 0.714 shields. This qualifies the
bounded activation and persistence checks, not robust combat or full traversal.

The [PMC3 gold-key extension](evidence/pmc3-gold-key.md) adds normal collection of
two original shield resources, the gold key and keyed-door flight, including
consumed-item persistence. It passed 380 warm and 16 fresh-process steps on the
same APK, ending with about 75 shields. This extended chain also covers the
corrected bounded firing sweeps at both opening shafts. Fixed local approaches
remain explicit; the published bundle is unchanged.

The [rendered GuideBot pilot](evidence/rendered-guidebot.md) confirms a bounded
hub route with an assisted, physics-driven companion. Companion activation of
the XL exit is unsupported. Normal-player probes fill that specific gap without
relaxing actor rules or dispatching exits directly.

A separate [normal guided-missile pilot](evidence/pmc3-guided-pilot.md) uses debug
candidate28820 to observe the active projectile. Original ammo, local combat and
flight through the first grate were exercised; switch3, clean release and the
red-key route remain unqualified. That historical diagnostic candidate is separate
from the current version29080 phone fog candidate described above.

## Boundaries and feedback

- Fixed local approaches and bounded escape flights do not qualify full human
  navigation, surviving every boss encounter or earned campaign completion
- PMC3 opening, blue/gold-key and finale receipts supplement the bundle; the
  red-key route remains unqualified
- Physical ARM, touch ergonomics, controller acceptance and device performance
  need user feedback; historical route tests used an owned x86_64 host-GPU emulator.
  The version29080 depth candidate uses an owned x86_64 SwiftShader emulator
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
