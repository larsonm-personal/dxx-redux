# LAN engine selection and launcher choices

- Keep discovery independent of the single-player engine selection and use the advertised engine for lobby and in-game joins, including IP discovery
- Check the advertised engine's data before joining and show a specific warning when unavailable, without changing the saved single-player selection
- Remove the D1-in-D2 launcher choice; retain D1 mission selection inside D2 and the internal startup profile used by standalone engine regression scripts
- Update readiness and LAN regression coverage, run scoped code quality, and build/test the Android changes

Initial inspection: discovery already publishes both engines and multiplayer launches use GameLaunchInfo.game. The missing-data failure currently arrives at launch preflight; move this feedback to join time and consolidate the in-game card/IP paths

## Implementation and validation

- Launcher choices are D1 and D2 only; saved single-player selection is not changed by LAN joins
- Multiplayer browsing remains accessible without installed game data
- Discovered lobby cards and IP probes share advertised-engine join routing; missing or unsupported engine data produces a diagnostic before joining
- The authoritative join acknowledgement also checks engine readiness, and in-game joins retain the advertised mission requirement
- Extended the two-emulator runner with `-HostGame d1|d2 -JoinCoverage` and engine reporting
- Scoped mixed-language code quality passes
- Debug x86_64 CMake/Gradle build and 17 targeted JVM tests pass (SetupLaunchReadinessTest, LobbyMissionRefreshTest, LanMissionStatusDisplayTest)
- Device integration was attempted but not completed: APK installation failed because the primary classes.dex was absent in incremental packaging output. Concurrent builds/installations are operating on the same output and devices; a package-task rerun was attempted, but output was again being replaced. No LAN runtime pass is claimed
