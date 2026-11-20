# Sphere level 2 ordinary respawn, 2026-11-20

The ordinary Android x86_64 owner loaded the earned red-key checkpoint in segment 2500 with three lives and score 46850. Enemy combat killed the pilot. The input script waited for the native explosion and death-screen readiness, then pressed Space. The new ship started in segment 0 with two lives, both keys, score 46850, 100 shields and 100 energy. The weapon inventory reset to the base laser and seven concussion missiles.

Android wrote a quick slot, stopped the process and cold-loaded that slot. Windows independently restored the same DGSS bytes, checked the pilot and inventory, and exported a native continuation. The resulting save SHA-256 is linked to the incoming Android digest in the [owner receipts](../XL_compat/evidence/sphere-ordinary-respawn-20261120.json).

A subsequent Android recording exercised ordinary controls on the new ship. Clean Windows and Linux replay each matched all 25 frames and object comparisons, 29 world records, 190 SIM RNG events and the terminal result. The recording had zero FX draws. The separate Android startup trace still has 1500 additional starfield draws. Physical ARM and a full campaign remain open.

## Reproduction

Use the original Sphere 1.51 descriptor and HOG and the authenticated retail runtime identified by the receipts. Stage the Android export of the earned red pickup as the ordinary save for a disposable pilot named spresp. The source owner records the expected SHA-256 and authentic mission assets. The entrance must have two keys, three lives and score 46850 in segment 2500.

Run the [respawn script](../XL_compat/tests/sphere_earned_respawn_android.jsonc) with the ordinary D2 APK through android/helpers/run_test.ps1 on the staged emulator using the retail edition and PreserveState. Retain the Android-written quick slot, verify its DGSS prefix and compare its digest with the on-device file. Force-stop the application and use the [cold-load script](../XL_compat/tests/sphere_earned_respawn_android_cold.jsonc) with the same profile. Both scripts were checked against the executed plans after scoped formatting. The original route to the red key used portal steering and visible-target aiming.
