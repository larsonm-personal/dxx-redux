# September 18 suite failures

- [x] Diagnose and fix D2 graphics/headless replay divergence in the two level 9 recordings
- [x] Diagnose and fix missing objective labels in the D2 launch-to-automap test
- [x] Run focused regressions and scoped formatting; leave the full suite to the user

Preserve the user's existing changes in outstanding_bugs.md

## Findings and changes

- The graphical D2 target enabled `DXX_GUIDEBOT_ROUTE_PLANNER`, but the replay headless target did not. The first traced divergence was Guide-Bot path generation at frame 134/135, where the legacy headless door rules allowed a closed wall that desktop rejected. Enable the same planner definition for headless replay
- Both affected level 9 recordings now produce matching graphical/headless final results. The local, gitignored `d2_descent2_level9_20260511_215831.dximdemo` final powerup expectation was refreshed from 107 to 106 after that agreement was verified; recorded inputs were preserved
- The automap assertion ran while current-level route metadata was still calculating at 98%, with no objective labels available yet. Wait for complete route readiness through an introspection expectation before checking label and connector geometry

## Validation

- Windows D2 build passed
- Both affected recordings passed graphical playback against fresh headless results
- D2 launch-to-automap passed all 95 steps
- Scoped formatting/lint passed
- Both affected headless recordings passed their final fixture checks
- Launch-to-automap also passed all 96 steps in a scratch copy that explicitly cleared the route cache first. The new readiness wait took 9.69 seconds, then rendered 12 label candidates and a connector
- Full suite deliberately not run

Logs and diagnostic traces: `temp/suite_failures_20260918/`
