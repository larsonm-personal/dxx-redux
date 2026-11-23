# Pumo S21 loading and XL follow-up, November 22-23

## Objective and constraints

Use the connected SM-G996U (Android 15) through noon Pacific on November 23 to reduce ordinary Pumo loading time and advance an independently reproducible D2X-XL compatibility case. The existing PR #722 is the immediate parent and passed fork CI. Preserve the original archive and all phone saves. Physical testing uses the isolated `com.dxxredux.app.nsdtest` package; its automation coverage is distinct from a person steering the phone.

## Sequence

1. Build the current parent as the isolated physical-device variant. Record APK hash, source head, device and serial. Run the protected original Pumo import, hub-to-Crauphel, missile, save and reload route on the S21. Retain the runner receipt and raw timing log. Compare the same milestones with the clean emulator baseline; distinguish pre-briefing resource preparation, native level load and renderer texture caching.
2. Trace the expensive path. First try to reuse a portable, content-addressed result from mission extraction or publication without storing native pointers or GL state. Cache keys must cover archive bytes, mission identity, selected resource generation, level ordinal, preparation schema and renderer-dependent texture target where needed. Preserve fresh live admission and a safe miss path. Measure both cold import and repeat launch on the phone, including disk growth.
3. If the cache cannot be safely completed in the remaining window, deliver a smaller measured loading improvement rather than weakening source checks. Use the same S21 run and one independent original XL case from the remaining census to investigate the next compatibility blocker. Repeat the affected cluster and retain original and alternate distribution outcomes separately.
4. Run scoped formatting, host/native contracts, Android build and protected device tests. Commit coherent layers, push ready-for-review immediate-parent PRs to the personal fork, register the stack, inspect workflow admission and wait for passing applicable CI. Leave PRs open. Update phone directions, a dated report and the next kickoff with exact outcomes and the first unresolved action.

## Evidence and stop conditions

Use one local gameplay worker and read its receipts. Keep private archives, screenshots, profiles and saves in ignored `temp/` only. Do not change the Play or GitHub app packages on the phone. If the S21 disconnects, preserve the runner's last receipt, reconnect and resume only after confirming the prior process is terminal. Loading gains require matched build settings, original archive, level route and inputs; a shorter bar alone is not proof. Use hosted Linux CI for routine cross-platform validation.

## November 23 checkpoint

The first S21 launch exposed a real fog admission failure before gameplay:
D24 raster depth 0.5 sampled as 0.499755859. The admission adjustment alone
exposed missing fog in the controlled scene. The paired raster and volume-depth
precision fix passed 208/208 authored fog steps and the original Pumo
hub-to-Crauphel missile/save/reload route, 35/35 steps, on the same isolated
32140 APK. This is [fork PR #723](https://github.com/larsonm-personal/dxx-redux/pull/723),
based on #722 and registered in stack #720. It does not claim faster loading.

On the matched S21 route, pre-briefing checks took 9.129 and 11.470 seconds.
The quickload took about 32.5 seconds, including two complete preparations of
`mvlev1.rl2`: the first reconstructed the save's source identity and the
second prepared the level for native load. The next implementation retains
the first owner for one use inside the existing `mod_save_begin` /
`mod_save_finished` transaction only after source identity matches. It
requires the same mission publication generation, context, level bytes,
ordinal and unfiltered source objects; every miss follows normal preparation.
Measure a matched route on the phone before claiming a gain. A durable import
cache is deferred until source invalidation and resolution-dependent decoding
have a defensible key and a measured first-launch benefit.

The matched original archive, Rookie, 640 x 480 hub-to-Crauphel route measured
32.627 seconds for the baseline quickload and 21.953 seconds with one-use
reuse, a 10.674-second reduction. A repeat measured 21.906 seconds. Native
level initialization fell from 17.038 to 6.567 seconds in the first pair.
Both candidate runs passed 35/35 gameplay actions, including a missile before
and after quickload, and restored all protected profile hashes. The second
run also passed an explicit one-use log assertion. Pre-briefing checks stayed
near 9.1 and 11.5 seconds; they remain the next loading bottleneck.
