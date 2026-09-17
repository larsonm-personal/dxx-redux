# Guide-Bot cage releaser owns the companion

- [x] Trace initial ownership: passive release currently picks the first eligible slot
- [x] Retain the player responsible for destroying a cage wall, including network updates
- [x] Exercise client cage release and confirm both peers retain client ownership
- [x] Run scoped quality, Windows/Android builds, and focused native/LAN regressions

Keep the existing fallback for an already-open cage or an unavailable releaser
Use the normal wall damage path in automation so the test covers network propagation

D2 door-open messages carry the damaging player, since a peer can report a wall
destroyed by another player's replicated projectile. State-sync messages use -1
The D2 protocol is now 30070 on Android and 30020 on desktop

Regression entry points:
- android/tests/test_lan.ps1 -GuidebotClientRelease cage
- android/tests/test_lan.ps1 -GuidebotClientRelease deploy

Validation completed on 2026-09-16:
- Scoped code quality passed
- Windows D2 and Android debug builds passed
- Native escort_owner, coop_gameplay_fence, and hud_counts tests passed
- Two-emulator cage release passed; both peers retained client ownership
  (temp/guidebot_client_cage.log)
- Two-emulator Deploy release passed; both peers retained client ownership
  (temp/guidebot_client_deploy.log)
