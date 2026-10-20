# Parallel loading evidence

Control: run 39442770798, revision 964ed12b747f615e379b6e708022d7eb36a85b72,
export 39446278295. Faster initial retries are disabled in both control and
parallel candidates. The control is stacked on the unmerged PR103, not cmake.

`control/stages.csv` contains contiguous device-local launch-to-protocol-ready
intervals; `network_waits.csv` isolates host flush/request/auth/SYNC intervals.
Source line references refer to recovered text excerpts. `provenance.json`
records the original artifact and recovered excerpt hashes.

Reproduction from the topic branch:

```sh
gh workflow run public-lan-smoke.yml --ref investigate/lan-parallel-load-20261020 -f measure_transitions=true
# After the measurement run completes, substitute its observed run ID:
gh workflow run public-lan-smoke.yml --ref investigate/lan-parallel-load-20261020 -f evidence_run_id=MEASUREMENT_RUN
python3 android/helpers/collect_lan_evidence.py EXPORT_RUN --output temp/parallel-evidence
python3 android/helpers/chart_lan_startup.py temp/parallel-evidence/first-play/lan-launch-preparation-public-shareware --output temp/parallel-chart --compare 'android/ai tool plans/performance/lan_parallel_load_20261020_results/control'
python3 android/helpers/chart_lan_parallel_load.py temp/parallel-evidence/first-play/lan-launch-preparation-public-shareware --output temp/parallel-overlap
```

Call `android/helpers/retain-recent-artifacts.ps1 -Artifacts OUTPUT` before each
new output root. The original CI artifact retains full logs and screenshots;
annotation recovery intentionally exports compact text only.

Each game uses three process-cold samples. Sample 1 clears app data; samples2/3
retain assets and pilots, but all samples reset configs/preferences. Level 2 in
the same session is process-warm. This is not ordinary retained-config startup.
Launcher commands and introspection polling are serialized controller operations;
chart stages use device-local timestamps. Protocol briefing readiness is the
phase 6 marker, not proof of the first visible frame. Script dwell and deliberate
fault holds are excluded from matched startup means.

The separate fault checks use 6s slow-host/client holds, a duplicate admission,
a 20s slow-client hold, dropped admission, client process cancellation, then a
fresh session. Cancellation must not produce loaded readiness; its bounded 60s
host loading allowance is checked separately. No claim of same-session reconnect
or host-loss recovery coverage is made.

Saved evidence can be reanalyzed without new emulator execution: use export
39455234856 for measured run39450006075, and export39454455784 for the final
guard smoke39451088229. `parallel/checks.json` records the62 complete native
receipts and12 fault-case passes; `parallel/verification.json` contains all18
peer-pair checks. `smoke/` retains the three explicit two-player session receipts.

The measured engine revision is a673dfd8e86a6315f9c557cb051340efe529c2da;
the observer-exclusion guard is7bc1593d1915be6d6986d8bd87a25d6f8910a207.
Later commits only adjust analysis, evidence export, documentation and cache
labeling. The experiment predates cmake84387ccdf (PR102); it does not measure
that later Mac-asset/import change.
