# Modern mission exits and revisits

Branch 94 implements authored positive exit destinations for modern XL levels,
visited/destroyed mission state, intact-level disk caches, reactor-death exit
selection, and campaign completion. Classic progression remains on its existing
path. This qualifies controlled transitions, not earned traversal or the entire
Pumo Mines campaign.

## Source contract

Pinned D2X-XL revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
The existing trigger reference target extracts `CMissionManager::AdvanceLevel`,
`CTrigger::DoExit`, and `FindNextLevel` from hash-checked source files.
Windows and Linux passed 10,080 destination cases, 331,776 activation/flags/
side-effect sequences, and 10,368 reactor-death destination cases.

Exit dispatch retains source ordering: stop audio and boost, select destination,
reject destroyed destinations, apply exit flags, then enter the normal exit
sequence. A completed transition stops further trigger-chain processing.
Reactor death prefers available unvisited destinations, then the lowest ordinal.
Destinations above the mission's final level enter normal campaign completion.

Pinned `ReenterLevel` and the active save loader restore the cached player and
world, preserving current weapon selection and the current campaign timeline.
This includes cached lives; negative cache mode is not secret-return player
carry mode. Reentry resets player position and flight controls. Destroyed levels
are marked unavailable and their departing cache is removed. A new game resets
the visit table so prior cache files cannot supply a visited world.

The XL save trailer is schema 27. Ordinary full saves restore mission progress;
level-cache restores retain the current timeline. Mission identity, state count,
state values and destination bounds are validated before applying the trailer.

## Runtime evidence

Native lifecycle coverage uses derived Neptune geometry and real exit dispatch
for 1 -> 3 -> 1 -> 3. It checks two distinct disk worlds and player states,
selected weapons, visited state, full checkpoint restore, new-game isolation,
destroyed-destination rejection and runtime death destination selection. Existing
automatic base/secret saves and repeated entry/return controls also pass.

Android tests use `android/tests/test_xl_pog_runtime.ps1`:

- `-OriginalHub -MissionFlow`: 32/32 steps, original Pumo route
  1 -> 2 -> 3 -> 4 -> 14, full hub checkpoint restore and original POG GPU checks.
  Triggers are dispatched directly; score/intro presentation is suppressed.
- `-OriginalHub -CampaignEnd`: 16/16 steps, unchanged original hub exit 5 with
  destination 29, normal score screens, original ending briefing and main menu.
- `-RevisitFixture`: 31/31 steps, derived disk revisits, ordinary save/restore,
  then `DoPlayerDead` with a destroyed reactor. The destroyed cache is removed;
  cached lives, post-death ship reset, world state, controls and GPU checks pass.
  This uses the production terminal entry, not the full countdown timer.
- `-OriginalHub -FlyoutFixture -D1Pig <retail D1 PIG>`: 19/19 steps, 85.259 seconds.
  Original level-2 tunnel geometry receives derived scenery resources and a
  fixed approach pose. The actual cinematic reaches its exterior phase and
  advances through the normal score screen to level 3. A derived return trigger
  restores cached level 2 with its particle world, energy, mission timeline,
  interior spawn, flight controls, player camera and GPU resources.

The flyout test found that exit-model loading replaces mission model definitions
before next-level admission. Committed XL travel now invokes the existing
mission-definition reload after saving the departing level and before preflight.
Registry validation is unchanged. Both native lifecycle and Android flyout
coverage passed after this production fix.

The Android owner protects and restores staged assets and player files, including
the temporary D1 PIG; before/after hashes match. Original Pumo package files are
unchanged. The flyout fixture appends generated scenery to a separate HOG copy.

## Receipts and limits

Build/reference/owner logs are under `temp/xl-skybox-20261031`, including
`mission-flow-windows.log`, `mission-flow-linux.log`,
`android-original-flow.log`, `android-original-end.log`,
`android-revisit-death.log`, `android-flyout-cache-reset.log`, and
`android-flyout-reset-build.log`. Native details are in
`native-runtime/controls.log`. Android artifact directories are
`temp/xl-original-flow-android`, `temp/xl-original-end-android`,
`temp/xl-revisit-android`, and `temp/xl-flyout-cache-android`.

Scoped formatting, both automation catalogs and diff whitespace checks passed.
Policy reference results are reused after the reload-order fix because the
policy did not change. Catalog results are reused because test registration did
not change. No autonomous 3D navigation was used. Six original Pumo levels still
have unsupported requirements; this work does not qualify those levels or a
complete human playthrough.
