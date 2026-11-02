# PMC1 switch gate and persistence

Original PMC1 (`pmlev1.rl2`) has a one-shot switch on wall 37, segment 367 side 5.
Trigger 5 opens the locked door pair 35/36 at segments 375/374. The probe enters
PMC1 through the ordinary hub route, verifies that shooting the locked door does
not open it, then fires the standard laser at the original switch. Introspection
must record player actor 0, shot 1, an operated trigger, and both doors fully open.

The warm phase saves and restores the opened state. A second app process loads
that save through the ordinary Load Game menu and flies across the gate. Fixed
poses position the ship for each bounded interaction; no keys, weapons, objective
completion, door state or exit activation are granted. The cold flight starts in
segment 1864 facing side 4, before x=-141, and must cross x=-135, beyond the authored
door plane at approximately x=-140.93. A transient segment assertion was unsuitable:
the ship passed the thin target segment before the automation sampled it.

Use `test_xl_pog_runtime.ps1 -PmcSwitchPlaytest -PmcPlaytest -OriginalHub
-MissionFlow` with the complete original resources. The existing owner verifies
backup/restoration of staged assets, pilots and settings, and records distinct
native process IDs for cold loading. Bind the disposable AVD home and recovery
name explicitly; do not run generic recovery against an unprotected user profile.

Direct evidence on published APK `888ded31795e2074c46566c688ac813902a60cce`:

- Normal switch shot: 30/30 steps, 72.600 seconds
- Locked-door negative control and warm restore: 38/38 steps, 96.338 seconds
- Direct cold traversal: 12/12 steps, 33.265 seconds
- Protected owner: 38/38 warm steps in 106.844 seconds and 13/13 cold steps in
  32.519 seconds, including an explicit check that the start pose precedes the gate

The protected run used native PIDs 8728 then 9487 and restored identical pilot,
settings and resource hash lists. Its `cold-render.png` was visually inspected.
Full protected receipts are in `temp/xl-original-pmc-switches`.

Receipts are under `temp/xl-traversal-20261102/pmc-switch5-*`. This is bounded
normal activation and persistence evidence, not human traversal, full PMC combat,
reactor/exit progression, hub return, or physical-device validation.

## Red-key gate extension

The next phase uses original red-key object 27 in segment 541. It first shoots
door 134 without a key and requires the door to remain closed. Fixed positioning
at the authored key allows normal collision to collect it. A checkpoint restore
must recover both the key and its saved position after the player moves away.
The player then returns to the original approach in segment 1553, fires normally,
and flies across the door. Trigger 13 must record player actor 0 and shot 0,
and open its linked doors 98/112. The final checkpoint is also checked by the
existing fresh-process load phase.

The direct pickup/door probe passed 40/40 in 77.087 seconds on the same APK.
The extended protected scenario passed 63/63 warm steps in 144.663 seconds and
13/13 cold steps in 34.567 seconds, with native PIDs 10872 then 11823. Resource,
pilot and settings hash lists matched after restoration, and the final frame was
visually inspected. Prior switch-only receipts are preserved in
`temp/xl-traversal-20261102/protected-switches-C`; the current protected output is
`temp/xl-original-pmc-switches`.
