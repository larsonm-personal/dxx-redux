# Original hub activation and restore

The normal-player probe complements the rendered GuideBot pilot. The companion
can fly the PMC corridor, but the XL exit rejects its actor type. A player must
activate the authored route normally to establish progression.

Run `test_xl_pog_runtime.ps1` with `-HubActivation -PmcPlaytest -OriginalHub
-MissionFlow` and the original Pumo/XL resource arguments used by the PMC owner.
The owner backs up and restores its staged resources, pilots and settings. Use
an explicitly owned disposable device and bind `ANDROID_AVD_HOME` plus
`DXX_TEST_SECONDARY_AVD` when using a nondefault emulator on the secondary serial.
Generic recovery otherwise may fall back to a different profile if the original
process disappears. Do not run this against an unprotected user profile.

The scenario fixes the player pose in hub segment 65 facing side2, saves before
activation, fires the standard laser and applies ordinary forward thrust. It
requires arrival in PMC1 (level 11), restores the original hub save, verifies the
unused boost/exit triggers, and earns the transition again without repositioning.
No direct trigger/exit dispatch, inventory grant or mission edit is used.
Segment70 is too shallow for a useful firing pose: face_view clamps the offset
to about 1.25 units, inside the player's radius. Use the approach in segment 65.

This proves a bounded normal door/boost/exit and save/restore path. It does not
prove human navigation, PMC combat/completion, hub return after objective
completion, every hub branch, or physical-device behavior.

The direct first-transition probe passed 22/22 on published APK
888ded31795e2074c46566c688ac813902a60cce in 73.766 seconds. Receipts are in
`temp/xl-traversal-20261102/owned-direct-approach-*`. The direct restore scenario
passed 36/36 in 134.681 seconds. The protected owner passed 36/36 in 144.742
seconds, with identical before/after resource, pilot and settings hash lists.
Its state, trace and visually inspected frame are retained in
`temp/xl-original-hub-activation`.

The generic runner previously ignored `-PreserveState` for game prelaunch and
reset settings anyway. It now honors the switch; its callback regression checks
both preserved and ordinary startup. This change does not make recovery safe
for arbitrary user devices or replace the owner's restoration checks.
