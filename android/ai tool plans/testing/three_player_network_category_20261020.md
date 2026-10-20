# Three-player network coverage category

Base: b60f6c5ab7887e2379b1bbe1f03c09eb052092dc, the parallel-startup
recovery hardening branch. Keep this category change separate from engine
changes, packet-fault experiments and actual mixed-version qualification.

## Plan

1. Register `test_lan_three_player_startup` as the independent
   `three_player_network` category, requiring three disposable emulators and a
   900-second master timeout. Use the normal master entrypoint and evidence.
2. Run precisely four cases: native D1 and native D2, each with a successful
   uneven-load start and a selected loading player lost before synchronization.
   Reuse pinned public asset staging once per game, reset mutable state per case,
   and retain one result per case plus a complete-owner summary.
3. Admit the category to the scheduled extended policy, sharing the existing
   Android dual-device lane's APK/toolchain/public-fixture setup. Provision the
   third emulator only for a selected three-player owner. Keep ordinary PR smoke
   at two players; provide explicit normal-entrypoint qualification separately.
4. Verify catalog, selection, failure/partial-result handling and host controls,
   then exercise all four real cases through the same CI owner path.

## Coverage and bounds

Healthy starts hold one client for 20 seconds and the other for 6 seconds, then
require all three named peers in the cooperative briefing and running level.
The loss case force-stops the held client, verifies process disappearance, and
requires both survivors to reach the expected error or disconnected main menu
without synchronization. All peers must show the intended native engine.

The category does not repeat D1-in-D2, packet faults, old/new APK builds, timing
samples or the wider dual-peer recovery matrix. A failed case remains a failed
receipt even if later cases pass. Missing or skipped cases cannot certify the
owner. Loss of emulator health stops further work rather than fabricating cases.

## Validation status

The four underlying experimental cases passed in run 39476033731 before this
change. That is not normal-entrypoint qualification of the new category.
Local PowerShell/Android execution is unavailable in the isolated recovery
workspace; no credentials or security settings were changed. Local control command `python3 -m unittest discover -s android/tests -p
test_nightly_tooling_sweep.py -q` passed all 72 tests. PowerShell catalog,
controller-negative tests, scoped formatting and real emulator entrypoint remain
pending hosted validation. The three changed workflow files parse as YAML, and
all 46 embedded Bash scripts pass `bash -n`. No merge is performed.
