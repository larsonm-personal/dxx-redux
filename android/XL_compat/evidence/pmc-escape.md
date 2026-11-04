# Original PMC1 normal escape

The earlier seven/six-objective runs below used substituted Vertigo robot data.
Full-original Pumo's type76 objects are gunless props, so reactor101 is its only
PMC1 objective. The original reactor gate now proves ordinary laser destruction
and a90-second countdown; see [reactor accounting](reactor-objectives.md).
Historical exit semantics and contact traces below remain useful, but their
boss counts are not full-original mission evidence.

The original escape is reachable through the master-11 passage and blastable
wall. This probe opens that passage with a normal shot, blasts the wall with the
starter laser, then gives one forward input from a fixed approach in segment 585. Once the first speed boost engages, input is released. The engine carries
the player through the curved pipe and activates the authored exit by contact.
There is no exit dispatch, trigger dispatch, health grant or mission edit.

The preceding protected scenario enters PMC1 through the normal hub exit,
operates the local switch/key gates and destroys reactor 101 with laser fire.
Those checks use fixed local approaches. The escape flight itself has no pose
changes between its segment-585 approach and the completion screen. This is
local earned exit traversal, not a connected human run through the whole level.

## Authored conditions

Pinned XL source `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`main/trigger.cpp::CTrigger::DoExit`, accepts the local player when the exit is
enabled, stops the speed boost, selects the authored destination and begins the
end sequence. It has no universal reactor/boss-clear guard. Remaining objectives
are countdown accounting; six live bosses do not by themselves prohibit this
exit. Earlier notes saying those bosses are "required" refer to clearing all
countdown objectives, not an added exit prerequisite.

The route contacts boost triggers in order
`42, 16, 36, 37, 38, 39, 40, 41, 45, 43, 44, 35`, then exit 46 at segment 1702
side 0. Its fixed destination value 786432 selects level 12. The player starts
before opened wall 125 and blasted wall 122. Master 11 retains its authored
lock of doors 16/97 while opening 124/125; this is not a generic unlock-all probe.

## Historical substituted-resource exploratory evidence

Exact published APK `f1e73f5fd38a836515bf98ea369dbe1c2a788d7d`, version 28800:

- Cold-load approach and flight: 12/12 steps, 45.659 seconds including load
- All 12 boost contacts and exit 46 record actor 0, shot 0
- Completion endpoint: segment 1711, position
  `[-326.382568359375, -828.6124267578125, -100.59222412109375]`, alive,
  six remaining objectives, no countdown, speed boost unlocked
- Rendered frame shows the Level 11 Complete screen
- Ordinary Enter/briefing transition: 9/9 steps, 24.543 seconds, reaches PMC2
  level 12 segment 376 alive; saves a checkpoint

Receipts are `temp/xl-traversal-20261102/pmc-escape-flight-*` and
`pmc-escape-next-*`. The preflight owned profile archive is
`pre-escape-player.tar`, SHA-256
`1e621b4e5da59ab1cc2663eb3620f8cef857aa523cc0bc3548595a33c3fdf087`.
It contains 24 pilot/save/settings files. Only the owned disposable AVD was used.

## Historical substituted-resource protected scenario

Run `test_xl_pog_runtime.ps1 -PmcEscape -PmcReactorPlaytest
-PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow` with the historical Vertigo extension.
This is separate from the original PumoMods mode. The owner preserves the profile and verifies AVD identity,
with emulator recovery disabled. It records the completion trace and frame,
then cold-loads the preflight save in a fresh process and repeats the escape.
The normal completion menu then advances to PMC2, where save/load is checked.

Full mission navigation, surviving the fixture boss encounters, human touch/control
acceptance, physical-device execution and later PMC2 exits remain unqualified.
The rendered GuideBot companion still cannot activate this XL exit; the normal
player probe fills that specific coverage gap without relaxing actor semantics.

The fresh protected scenario passes 105/105 warm steps in 200.475 seconds and
24/24 cold-replay/destination-save steps in 94.078 seconds. Game PIDs 14984 and
16296 differ. Both profile/resource hash lists have SHA-256
`35982d56014782fa8fc80fd92beb4867971d9ddc3257f3eff71edbcd886f8477`.
The completion and restored PMC2 frames were inspected. Receipts are preserved
in `temp/xl-overnight-20261104/prior-escape`; the first run used the reactor output folder
before output precedence was corrected, so its complete relevant receipts were
copied to this dedicated folder. Previous immutable release bundles remain intact.

## Full-original Android escape, November 3 2026

Use `-PmcEscape -PmcReactorPlaytest -PmcInnerGates -PmcPlaytest -OriginalHub
-MissionFlow` with full original PumoMods and the original XL camera/effects
resources, without VertigoHam. The owner composes the earned original inner-gate
route, original reactor suffix and `test_xl_pumo_original_escape.jsonc`.
The passage is already opened/blasted by that route; the escape suffix asserts
those states, zero objectives, the earned red key and active countdown, saves
before the first boost and applies ordinary thrust. No pose changes occur during
the boost flight. All12 boost contacts plus exit46 record player actor0/shot0.
The completion menu retains the destroyed-reactor timer until level12 initializes;
its active telemetry is not an additional exit prerequisite.

On exact retained/installed APK29471 (source `ec7715ffa5cd34865b701aa2152ee42f2a51b018`,
com.dxxredux.app, SHA256
`da070ebaacf95ea9be2f95fbd630ef5db1ccea0e24cf9108bbb6822c96a0661d`):

- Warm117/117,105.540seconds,run `c5250686d778462a93d2c50fee6b61ac`
- Fresh-process24/24,37.130seconds,run `b6222560e2d74b93bb27af6077def41b`
- Game PIDs13663/14257 differ; cold menu load repeats the boost escape
- Warm completion:segment1711,(-327.9076,-823.4941,-100.2564),40.6434player shields,
  alive,zero objectives,boost unlocked,normal Level11 Complete screen
- Normal completion-menu/briefing transition reaches level12,segment376;
  ordinary quick-save/load preserves that PMC2 position,player100shields/100energy,
  zero objectives and countdown inactive
- Protected before/after manifests both SHA256
  `160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`

Receipts: `temp/xl-original-pmc-escape`, including inspected escape-complete.png
and cold-render.png. Original resource hashes/identities are the same as the
[inner-gate qualification](pmc-inner-gates.md). No APK was built or installed.
A terminal readiness failure before gameplay restored exactly and is preserved
in escape-readiness-failure under temp/xl-overnight-20261104. A subsequent run
reached the ordinary completion screen but failed a stale no-countdown assertion;
its trace/restoration receipts are in escape-countdown-assertion. The corrected
run above completes both warm and cold phases.

This uses owned Android14 software emulator DxxPhoneFog20261102, legacy protected
staging (mission_asset_owner=base), fixed approaches between local interactions
and ordinary activation. It does not qualify launcher UI import, connected human
navigation, PMC2/PMC3 progression, physical-phone controls or full audio/visual
fidelity. Historical Vertigo boss combat remains a separate fixture result.
