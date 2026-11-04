# Original Metillis native progression

The opt-in `xl_pumo_metillis_player` gate follows `xl_pumo_majha_player` through a
CTest fixture. It requires the actual earned level4 checkpoint, original mission
resources and owned retail data. It runs 85 ordinary-player actions from that
checkpoint, then restores its earned reactor save in another process for 14
alternate-route actions. Both destinations exercise save/load after displacement.
A third fresh process runs15 Coldrom actions: normal Fusion pickup, the authored
reactor-chamber door effect, owned laser selection, rejection of an unowned
weapon and save/load of that earned inventory and selected weapon.

The main route checks absent blue/red/gold access controls, normal key pickups,
keyed-door contact/master activation, the closed reactor gates, normal primary
fire against guard203 and reactor204, and the gates opened by reactor destruction.
It then follows all17 speed boosts without intermediate repositioning, the master
and authored99-second countdown, and the ordinary exit to level5/spawn260.
The alternate restores the exact saved health, energy, kills and keys, crosses
the alternate reactor gate and master39 with message41/sound42 dependencies, and
normally enters level14/spawn228. Trigger metadata is not sound-playback evidence.

The developer probe can share read-only original resources through
`-pumo-resource-root`, while profile, saves and output remain in its separate
`-extra-dir`. This avoids another roughly1.6GiB resource copy for every route.
Without the option its existing staging behavior remains. Additional snapshots
record nearby robot health, kills, reactor guns, weapon properties, simulation
time and the engine's recorded death-source object for combat diagnosis.
The `select_primary` action uses `do_weapon_select`, the same ownership/upgrade
checks as ordinary keyboard selection. Snapshots record the selected weapon and
owned weapon flags. No production gameplay rules, original assets, inventory or
health are changed.

Run with the same prerequisites as `original-majha-native.md`: configure
`XL_PUMO_PLAYER_ROOT`, `XL_PREVIEW_LIFECYCLE` and `XL_RETAIL_DATA`, then run
`ctest -R xl_pumo_metillis_player --output-on-failure`. The fixture includes the
Majha prerequisite automatically. For a retained earned entry, invoke
`tests/pumo_metillis.cmake` with ENGINE, DATA, RESOURCES, ENTRY and OUTPUT.
RESOURCES is read-only input; OUTPUT must be an owned disposable native profile.

Precommit Windows evidence is retained under
`temp/xl-overnight-20261104/metillis-gate-precommit`: main85/alternate14 PASS. The extended three-process gate is retained in
`metillis-gate-selection`, main85/alternate14/Coldrom15 PASS.
The preceding diagnostic phases and exact executable are retained separately
under `native-metillis-player`, including unarmed corridor and point-blank
reactor deaths. A combined-run setup failure on a non-quad destination wall was
corrected with a valid original-segment pose before save restoration.

These are unrendered native simulations with real GL admission, fixed local
approaches, explicit earlier level3 setup and skipped presentation. They do not
prove connected human traversal, Android/phone behavior, UI import, briefing,
visual/audio playback or completion of the remaining campaign. No APK was built.
Coldrom14 progression is a separate follow-up; its Fusion pickup auto-selects a
charge-and-release weapon, so continuous held-fire probes are not reactor passes.
