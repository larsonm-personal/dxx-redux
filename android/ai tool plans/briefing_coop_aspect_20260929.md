# Co-op briefing aspect follow-up

Status: fix and regression verified on two Android emulators

Reported case: First Strike, D1-in-D2, level 3 co-op briefing fills the entire wide screen on multiple phones

## Diagnosis

- The previous fix trusted the saved renderer pixel aspect
- Reproduced with a saved 4:3 configuration: Android rendered 640x480 onto a 1920x1080 SurfaceView, while the briefing used the full 640x480 canvas
- Fresh native-resolution configurations concealed the failure
- Added sparse initialization logging and resolution introspection for the front canvas to distinguish render-buffer dimensions from displayed dimensions

## Completed changes

- Derive Android briefing pixel correction from the live SurfaceView size and render-buffer size; preserve desktop aspect behavior
- Apply the same correction during native D1 and imported D1 model-robot rendering, then restore the gameplay aspect
- Extend the LAN test with `-BriefingAspect`, seeding the stale configuration and checking the physical 4:3 ratio and centered canvas on both peers
- Capture both briefing frames and remove the seeded configuration after the test

## Validation

- Before fix: `test_lan.ps1 -Game d2 -MissionFile descent -InitialLevel 3 -BriefingAspect -SkipBuild` failed with a 640x480 canvas displayed at 1920x1080
- After fix: the same test passed on both peers and completed through playable co-op; canvas 480x480 at (80,0) maps to 1440x1080 centered on the display
- Visually checked the captured client frame: side borders and round Earth
- Android x86_64 debug APK built and installed on both emulators
- Regular Android assembleDebug build passed, including phone architectures
- Both Windows `test_upstream_compat` targets built
- Desktop briefing regression passed: 102 native frames match imported D1, including aspect variants
- Scoped mixed-language formatting and lint passed
- Physical phones have not been tested locally

Artifacts: `temp/briefing-aspect-repro.log`, `temp/briefing-aspect-after.log`, `temp/briefing-aspect-emulator-5556.png`, `temp/briefing-aspect-desktop-regression.log`
