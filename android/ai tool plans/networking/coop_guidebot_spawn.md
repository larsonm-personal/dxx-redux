# Deploy Guide-Bot in mines without a placed companion

- [x] Trace co-op-only rejection in escort_spawn_at_player
- [x] Add host-authorized creation and object mapping, preserving deployer ownership
- [x] Add paired regression coverage for a mine without a Guide-Bot, repeated deployment, and docking
- [x] Run scoped quality, host build/tests, and paired Android integration

Single-player already creates a missing companion. Co-op must request creation from
the host and publish one mapped robot plus its owner/generation to all peers
Use the existing reliable transport and gameplay world fence for the new message

The D2 protocol is bumped to 30069 on Android and 30019 on desktop
Creation has a separate generation from ownership so reordered owner/docking
state cannot suppress the robot's creation. D1's engine has no Guide-Bot; D1
missions played in D2 keep using the existing companion asset injection

Validation: scoped code quality, Windows D2 build, Android assembleDebug,
and native HUD, escort ownership, and gameplay-fence tests passed
Paired regression entry point: android/tests/test_lan.ps1 -GuidebotSpawn -InitialLevel 8 -AllowSecretWarps
Direct secret-level auto-hosting is rejected by the launcher contract, so the test
enters secret 2 from level 8 through the existing co-op travel automation

The paired run passed on both emulators: the secret mine starts with zero companions;
client deployment creates exactly one mapped robot on each peer with client ownership;
repeated deployment does not duplicate it or let the host steal ownership; recall and
redeployment retain the single companion and its client control slot
Evidence: temp/guidebot_spawn_lan.log, completed 2026-09-16 at 09:31 local time
