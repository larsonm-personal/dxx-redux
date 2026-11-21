# Original Pumo gameplay playtest

## November 21 phone playtest

For a first session, use the APK and save from the same kit. The Android 14
emulator reached both authored endings using the checkpoints below. Physical
phone play is still awaiting a report.

| Session | Kit | Start and first action |
| --- | --- | --- |
| Short Kartsal ending | 32030 | Load `kartsal-final-checkpoint`, fly straight through the checkpoint aboard the Black Hole, then advance the score screen. |
| Short Mineral Valley ending | 32080 | Load `mineral-ending`. The ship enters the exit as it resumes. Advance **Level 9 Complete** and read through the closing briefing. |
| Longer Mineral Valley flight | 32080 | Load `umbriel-blue` for the red-key grate and boss, or `mercury-arrival` for the full level. Make a manual save at a recognizable landmark, exit to the launcher, then use **Load Game** and continue. |
| Start from the hub | 32080 | Start Pumo level 1 with a separate test pilot. Record which episode entrance you take. |

Check the kit's `manifest.json` for the APK and checkpoint digests. Follow its
slot installer instructions if you use an earned save. Keep the supplied save
and your pre-issue save intact. Use a second slot for testing save and load.

Use this form for each report.

    Kit version and checkpoint (or fresh start):
    Phone model and Android version:
    Input method (touch layout or controller model) and difficulty:
    Episode, level, and room or landmark:
    Last successful action:
    First action that failed and steps to repeat:
    Expected result and observed result:
    Result after saving, closing the game and loading again:

A screenshot or short video can identify the room and what the controls did.
Keep saves and profile files on the build machine or your test phone until a
private transfer is arranged. The existing Windows runs used ordinary-input
steering through the connected episodes. Android automation and fixed-position
checks are recorded with the corresponding checkpoint.

For Kartsal level 13, use build 32030. It fixes a flight that could leave the ship trapped in the skybox. The ordinary universal APK includes ARM64, ARMv7 and x86_64. The Android 14 emulator imported the original release and cold-loaded the device-written checkpoints. Choose `kartsal-all-keys` to resume in room 3021 with the blue, gold and red keys, two lives and score 40,220. Choose `iquaxel-from-coldrom` to resume level 5 after the secret mission with score 83,440. The kit includes the APK, save files, a slot installer and a feedback checklist. [Build 32030 kit on the build machine](../../temp/xl-pumo-phone-32030/README.md)

### Play the Kartsal ending

Choose `kartsal-final-checkpoint` in the 32030 kit to load the pilot aboard the Black Hole ship with one life and score 48,270. Fly straight ahead through the checkpoint. Advance the score screen to see the authored ending. The ordinary Android build completed this flight from a cold load and showed the MD Vertex9 ending on `demoendb.pcx`. This completes the Kartsal Motivation route from the hub through levels 11, 12 and 13. The separate Mineral Valley route also reached its ending. [Rendered ending receipt](evidence/pumo-kartsal-ending-20261120.json)

### Continue Mineral Valley

For a short ending test, install build 32080 and choose `mineral-ending`. The checkpoint resumes with one life, score 344,540 and the ship already moving through Umbriel's final exit. Advance the **Level 9 Complete** screen, then read through the briefing. On ordinary Android the last page displays “Thanks for playing this first episode, Pumo Mines: Mineral Valley.” The kit contains the engine-written Android-format export that produced this screen. Report if the closing text, page advances or score screen behave differently on your phone. [Build 32080 kit](../../temp/xl-pumo-phone-32080/README.md)

For a longer final-level test, choose `umbriel-blue`. Android wrote and cold-loaded this save in room 997 with blue, two lives and score 315,790. Try to find the red-key switch behind the grate, defeat the teleporting boss and reach the escape tunnel. Report the first place where the route or controls stop making sense. [Umbriel checkpoint and ending evidence](evidence/pumo-mineral-valley-ending-20261121.json)

For Mineral Valley, install build 32080, which fixes extra lasers from some robot impacts. Start in the lava shafts with `lava-shafts-arrival` (level 6, two lives, score 100,000) or Mercury Geothermal Lab with `mercury-arrival` (level 7, three lives, score 162,740). Both are device-written saves verified after restarting the ordinary Android game. Record the first room where finding a key, firing through a grate, or controlling the ship becomes difficult. [Build 32080 kit and installation instructions](../../temp/xl-pumo-phone-32080/README.md)

Three more 32080 checkpoints are available. Use `mercury-three-keys` for the laboratory puzzles and prototype fight (room 424, three lives), `terradeep-arrival` for a fresh run of level 8 (room 1239, three lives), or `terradeep-blue` for the defended return after collecting blue (room 1049, one life). The moving ship can drift into a neighboring room after load. For the blue-key save, confirm the blue indicator and score 269,300. Android wrote each save and loaded it in a fresh process. [Run and checkpoint identities](evidence/pumo-mercury-phone-20261120.json)

In the earlier build 32030, choose `iquaxel-direct-gold` to resume in room 322 on the direct route with one life, 89 shields, the blue and gold keys, Vulcan and Plasma Cannons, and score 76,750. Try the guarded passage beyond the gold door. Record which robot or doorway stops you and whether the ship can retreat into cover. Choose `iquaxel-from-coldrom` for the alternate two-life entry from the secret level.

### Earlier checkpoints

Use the merged universal APK, version 32000, package `com.dxxredux.app`, SHA-256 `4ad8f0b32f34e0277dac002b3ab56ba314d60d7e4d0a81da22729e2b838f765b`. It includes ARM64, ARMv7 and x86_64 with XL developer controls off. An Android 14 x86_64 software emulator passed the original mission import, hub flight and cold save tests. Physical phone testing is open. [Playtest APK on the build machine](../../temp/xl-pumo-tranche-20261120/pumo-integrated-3c21ac2d4.apk)

For a shorter session, the local playtest kit includes the APK, five earned checkpoints and a helper that installs a checkpoint in a free manual slot. It includes the Kartsal level 12 entry and the Iquaxel entry reached by finishing Coldrom. The helper was exercised on Android with an occupied slot and a second slot claimed during installation. [Playtest kit instructions on the build machine](../../temp/xl-pumo-phone-32000/README.md)

1. Install the APK over a compatible test installation. If Android rejects the signature, preserve your existing pilots and saves before changing installations.
2. Open **Import files > Pick One or More Files**. Select your retail D2 data, the original `pmines_v11.7z` archive and the XL camera, effects and sound resources. Under **Mods/Levels > Show Files**, check that the mission and resources are enabled together. For the recorded Metillis route, leave Vertigo disabled. Pumo supplies its own robot and model definitions.
3. Select **Launch Descent2 > New game**, choose **XL: [-Pumo Mines-]**, and start at level 1. Use a separate pilot or save slot. At the hub select the Mineral Valley or Kartsal Motivation entrance. Record which one you chose.
4. On Mineral Valley, try reaching Metillis, collecting the keys, navigating back to the reactor and escaping. On Kartsal Motivation, try the opening switches and fire at the stationary robots. At a recognizable landmark, save, close the game and use **Load Game**. Check your room, keys, score and the state of the last door you opened.
5. For feedback, give the APK version, phone and Android version, input method, difficulty, episode, level, last successful action, and the shortest sequence that reproduces what you saw. Describe what you expected. A screenshot, short video or separate save before the problem can identify a switch or corridor more reliably than the room name alone.

The earned Metillis checkpoint reached room 948 with the blue and gold keys, three lives and score 35,360. The ordinary Android Load Game menu restored the desktop checkpoint and then reloaded an Android-written quicksave after a fresh launch. A separate developer recording from that Android save matched 25 frames on the ordinary Windows and Linux players. [Replay receipts](evidence/metillis-device-input-20261120.json)

At Metillis, collect all three keys, destroy the reactor, then choose an exit. The connected pilot reached Iquaxel through the main exit with four lives and score 51,150. It also reached Coldrom from the same reactor save via the secret exit, with four lives and score 53,740. On Android the Iquaxel save returned to the mine in room 260 after a cold launch. The ordinary device-written quicksave has SHA-256 `f206a6aad6b453638a7a53e4bfe24ea13c9abee0c54332281cedfb81f5c187c6`. [Earned exits and Android save identities](evidence/pumo-earned-exits-20261120.json)

For a shorter Coldrom session, start from the earned level 14 entry. The ship starts in room 228 with four lives, score 53,740 and no keys. Activate the first switch, find the master switch and collect the Fusion Cannon. Watch for the reactor doors to open. On version 32000 the ordinary Load Game menu restored this entry, and a new process loaded the device-written quicksave with the same pilot state. [Coldrom device receipts](evidence/pumo-coldrom-android-32000.json)

On Kartsal Motivation, activate the three opening switches, then shoot through the grate from room 236 and fly around to the far side of the timed door. Beyond it, stand in room 357 to shoot the two recessed switches. Try the master switch from an angle toward the side corridor. Record which switch responded and whether the forcefields dropped. [Reactor and grate route](evidence/metillis-earned-reactor-20261120.json)

The connected pilot completed Coldrom and returned to Iquaxel with score 83,440. On Kartsal it took the first exit into level 12 with six lives and score 17,580. Android cold-loaded both arrivals. In level 12, rolling the ship before firing helped hit the thin panel. Find the two access keys, use the control settings in the briefing, then take the escape route. The native pilot reached level 13 with five lives and score 28,820. The later rendered Kartsal result appears above. [Connected exits and Android receipts](evidence/pumo-connected-boundaries-20261120.json) [Metillis save evidence](evidence/metillis-portable-gold-save-20261120.md) / [Author's v1.1 release and episode list](https://descentbb.net/viewtopic.php?t=24808)

## Historical November 3 candidate

Use the [universal29470 APK and instructions](../../temp/pumo-mission-toggle-playtest-20261103-29470/README.md).
Source `7d2899bf816d16f5d59e724ab3d3f144a46211a1`, package
`com.dxxredux.app`, version29470. Six D1/D2 ARM64/ARMv7/x86_64 native IDs match.
APK SHA256 `5ba03d7df22b8b78c25dcc99cef0d7761100b248edbc706d9f43712ed41814a3`.
The [handoff](../../temp/pumo-mission-toggle-playtest-20261103-29470/final_handoff.md)
retains exact stack heads, validation, pending CI, preservation and continuation.

Tap the APK to update without uninstalling. In the launcher use **Select Game
Files or Archive to Import > Pick One or More Files** for your retail D2 data,
original `pmines_v11.7z` and your XL7z/raw camera, effects and sounds. Keep the
mission and required XL resources enabled in the same file set under
**Mods/Levels > Show Files**. Choose **Launch Descent2 > New game >
XL: [-Pumo Mines-] > level1**, then a difficulty. Use a separate save slot.
No proprietary resources or runtime archive are bundled. Original Pumo supplies
its own HAM/VHAM/ASE/OOF models; do not substitute Vertigo definitions.

29470 fixes re-enabling a valid7z mission after disabling it. Its exact owned
Android14 x86_64 emulator checks pass original UI import and production toggles,
68warm/39cold stock/Pumo save isolation, public damaged-archive rejection and
62D1 lifecycle steps. Protected hashes restore. Instrumentation supplies picker
URIs; actual system Files browsing is untested. Earlier29460 separately passes
76warm/15cold original PMC opening gates and catalog-owned checkpoint loading.
Approaches use fixed poses; neither run proves connected traversal or a campaign.

No physical phone verification is claimed for29470. Its separate reusable
disabled-owner gate now passes29seed/26rejection/9recovery steps, with stock save
bytes unchanged before rollback and full profile hashes restored. Ordinary
full-model robot/boss combat and later earned progression remain open.
See [full-original evidence](evidence/owned-model-android.md). The historical
Samsung hub render and older substituted-resource tests below are separate facts.

The retained29471 now separately passes88warm/23fresh-process full-original
[PMC1 inner gates](evidence/pmc-inner-gates.md): ordinary hub entry, switch/master
shots, blastable passage, red-key flight/pickup and save persistence. No new APK
was built. This corrects the old Vertigo-based seven-objective assumption: Pumo's
type76 is a gunless prop and PMC1 has one reactor objective. Fixed approaches,
legacy staging and an owned software emulator limit this claim; full traversal,
later campaign and phone verification remain open.

The same retained29471 now passes full-original [reactor combat](evidence/reactor-objectives.md)
(106warm/7cold) and [earned PMC1 escape](evidence/pmc-escape.md) (117warm/24cold).
Ordinary laser fire destroys the750-shield reactor and starts its90-second
countdown. Normal boost contacts/exit46 reach Level11 Complete, then ordinary
menu/briefing flow reaches PMC2 and saves/loads there with countdown inactive.
Protected profile hashes match exactly. These checks use legacy protected
staging and local fixed approaches; later PMC2/PMC3 progression and physical
phone execution remain open. No new APK was built or installed.

Full-original [PMC2 selectors](evidence/pmc2-selectors.md) now pass187warm/59cold,
including repeat selection after loading and normal gate flight. The separate
[PMC2 escape](evidence/pmc2-escape.md) passes167warm/23cold, reaches PMC3 by its
authored contact/boost exit and saves/loads normally there. Both use the same
retained29471, full original resources and exact protected restoration. The
original descriptor has14levels; these local main-route checks do not establish
all authored branches, connected human traversal or PMC3 completion.

The full-original [PMC3 blue/gold-key route](evidence/pmc3-gold-key.md) now
passes392warm/16cold on retained29471, including opening switches, normal shield
and key pickups, warm key saves, cold inventory/consumed-pickup restoration and
normal gold-door flight. Two local approaches were corrected for the full ship's
gun spacing and slope clearance; no trigger/collision/inventory rules changed.
Reactor/ending and remaining14-level branches are still open. No APK was built.

## Historical29330 delivery

The new [phone candidate](../../temp/pumo-user-addon-playtest-20261102/README.md)
includes ARM64, ARMv7 and x86_64, the preserved loading/save and metadata fixes,
and verified direct scene depth for fog, soft particles and lightning.
[Manifest](../../temp/pumo-user-addon-playtest-20261102/manifest.json) and
[handoff](../../temp/pumo-user-addon-playtest-20261102/final_handoff.md) retain exact identities and evidence.

**The Samsung hub worked on version 29180.** Its next transition reported a
missing camera addon; authorized read-only inventory confirmed it was absent.
Version 29330 retains installation-resource ownership and readable wrapped errors.
Tap the APK to install, then supply addons through launcher Import Files or the
grouped optional XL addon row. No proprietary addons are bundled. General import
also recognizes addon-only distributions. New phone transition/save acceptance is pending.
See [resource and dialog evidence](evidence/phone-readable-errors.md).

Version 29080 failed D16
numeric admission; version 29160 passed D24 admission but failed the first scene
because its 4x RGB8 source did not match RGBA8 capture. This candidate matches
the exact color format while retaining strict depth qualification. No alternative
phone level is verified. See [depth evidence](evidence/fog-direct-depth.md).

Source: `022a19202a996bbda75d33b0d366faec1388c4e4`, version code **29330**.
All six native library IDs match. APK SHA-256:
`94a10a84aad528af8d9b3d1a83afaeb7fba060c4dc0c4fe436be5cdc233651af`.
This delivery contains no runtime resource archive.
Stack: [PR318](https://github.com/larsonm-personal/dxx-redux/pull/318) ->
[PR320](https://github.com/larsonm-personal/dxx-redux/pull/320) ->
[PR321](https://github.com/larsonm-personal/dxx-redux/pull/321) ->
[PR322](https://github.com/larsonm-personal/dxx-redux/pull/322) ->
[draft PR323](https://github.com/larsonm-personal/dxx-redux/pull/323) ->
[PR324](https://github.com/larsonm-personal/dxx-redux/pull/324) ->
[PR326](https://github.com/larsonm-personal/dxx-redux/pull/326) ->
[PR327](https://github.com/larsonm-personal/dxx-redux/pull/327) ->
[PR328](https://github.com/larsonm-personal/dxx-redux/pull/328).
Earlier bundles remain unchanged historical baselines.

Why: depth blits can report success while returning unchanged depth. The new
path qualifies direct clears and raster writes numerically, captures normal
geometry depth, and preserves fog, particle fading and lightning occlusion.
Color capture matches the source MSAA format, including RGB8 without alpha.
Indexed POG palette-key flags now match original XL so opaque walls write depth.
No mission rewrite, GPU-name exception or admission bypass is used. Direct scene
capture is single sampled even with 2x/4x presentation; equivalent scene MSAA is
not claimed.

Exact 29330 qualification on owned Android 14 x86_64 SwiftShader:

- Original hub door/PMC exit, save/restore/repeat: 36/36 PASS, 77.287s, using
  the camera HOG from the user's supplied XL distribution and original Vertigo HAM.
- Original briefing: 18/18 PASS, 33.486s. Automatic overflow fits the physical
  canvas and printing audio stops at each page; next-page audio restarts.
- Addon raw/ZIP/seekable and streaming 7z import, tracker and progress toggle pass
  instrumentation. Fresh settings and both presets default progress bars on.
- 27 focused JVM tests, universal Android and Windows D2 builds, scoped quality
  and catalogs pass. All 13 protected install files and 297 restored profile files match.

Historical 29210 qualification on the same emulator:

- Complete original archive imported through launcher: 38/38 PASS, 110.484s.
  Normal hub door/PMC exit, save/restore and repeated transition reached level 11.
- Missing-camera rejection and full onscreen dialog: 22/22 PASS, 70.775s.
- Imported-only installer preserves 15 files; full test baseline restores 297.
  D1/D2 production resource ownership tests pass. Universal APK assembly passes;
  JVM suites pass 61/62, with an unresolved Enemy Within archive-import failure.

Prior 29180 qualification on the same owned emulator (not rerun unchanged):

| Check                                                                            | Result                                                   | Boundary                                                              |
| -------------------------------------------------------------------------------- | -------------------------------------------------------- | --------------------------------------------------------------------- |
| Original hub door and PMC exit, repeated after hub save/load                     | 36/36 PASS, 78.172s                                      | Normal laser/thrust; fixed local approaches                           |
| Fog geometry, save/rewind, stock transitions, MSAA presentation and context loss | 208/208 PASS, 177.377s                                   | Seven derived levels; controlled pixel/geometry checks                |
| Bundled installer                                                                | 15 protected files unchanged; installed APK hash matches | Owned emulator, not a physical phone                                  |
| Rendered GuideBot authored PMC exit                                              | Prior 4abb2b47: unsupported after 548 physics frames     | Companion reached exit; XL rejected activation; no progression credit |

Supplementary [Pumo/stock save isolation](evidence/pumo-stock-save-isolation.md)
passes 68 warm and 39 fresh-process steps on prior 4abb2b47/version 29080. Both mission saves
restore distinct player state; all 286 protected files match after the run.
The native load menu follows the last saved mission; it is not a combined list.

The previous [selector bundle](../../temp/pumo-selector-playtest-20261102/README.md)
and all older bundles remain historical baselines. This candidate retains
[repeatable selectors](evidence/pmc2-selectors.md),
[boss weak spots](evidence/pmc-boss-combat.md), [HXM binding](evidence/hxm-gameplay.md)
and [reactor/objective corrections](evidence/reactor-objectives.md).

## Historical29330 install instructions

Tap the new APK to upgrade the existing app. Use your existing retail Descent 2
data and complete original Pumo mission import. In the launcher, use Import Files
or the D2 optional XL addon row to select user-owned d2x-xl.hog and d2x.ham, either
as raw files or inside ZIP/7z archives. The supplied d2x-xl-2-1-8.7z contains the
camera HOG but no Vertigo d2x.ham. Supply the latter separately when required.
Use the same active file set as the mission. No command-line installer is needed.
Do not uninstall to resolve a signing mismatch; preserve pilots and saves.

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
from the current version 29180 phone fog candidate described above.

The [full-original earned-key finale](evidence/original-earned-finale.md) now
composes the original PMC gates, earned blue/gold keys, ordinary reactor combat,
portals and ending in453warm/24fresh-process steps on retained APK29471. This
uses full original mission-owned resources and preserves exact profile hashes.
It supersedes historical substituted-resource finale evidence for these bounded
checks, while leaving the other original14-level branch and connected traversal
unqualified. No new APK or phone qualification is implied.

The [original Multiverse branch](evidence/original-multiverse.md) adds78warm/
22fresh-process steps on the same retained29471: repeated normal hub entry,
earned red key/keyed door, reactor combat, all17boosts, the authored99-second
escape countdown, normal level3 entry and destination save/load. Original assets
and exact protected profile restoration are preserved; fixed approaches and the
remaining14-level scope stay explicit.

## Boundaries and feedback

- Fixed local approaches and bounded escape flights do not qualify full human
  navigation, surviving every boss encounter or earned campaign completion
- PMC3 opening, blue/gold-key and finale receipts supplement the bundle; the
  red-key route remains unqualified
- Physical ARM, touch ergonomics, controller acceptance and device performance
  need user feedback; historical route tests used an owned x86_64 host-GPU emulator.
  The version 29180 depth/color candidate uses an owned x86_64 SwiftShader emulator
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
