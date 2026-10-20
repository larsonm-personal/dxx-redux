# LAN co-op transition latency — 2026-10-20

Publishing changed co-op phases immediately removed an artificial wait behind
the host's 250 ms retransmission cadence. All ten game/peer/operation handshake
means decreased by 0.188–0.652 s in this experiment. Every after sample for the
initial briefing handshake was below every before sample in its corresponding
game/peer group. Required acknowledgments, generation/participant validation,
packet format, client ACK throttle, periodic retransmission and deadlines remain.

**This is not a universal loading speedup.** Next-level arrival was mixed, and
launch-to-briefing protocol readiness still took 14.514–23.147 s after the change.
Three sequential samples per game/peer on separate CI allocations are not a
statistical guarantee for physical phones.

## Observed results

**Before → after seconds, n=3 per cell per variant.** Host is emulator-5554;
client is emulator-5556. These are device-local native conditions/log timestamps,
not the controller's 800 ms polling bounds. No invalid attempt is pooled here.

| Interval                        | D1 host       | D1 client     | D2 host       | D2 client     |
| ------------------------------- | ------------- | ------------- | ------------- | ------------- |
| Briefing 1 Skip → ready         | 1.976 → 1.627 | 1.905 → 1.073 | 2.433 → 1.641 | 2.331 → 1.502 |
| Controlled exit → scores        | 2.849 → 2.636 | 2.554 → 2.349 | 3.035 → 2.372 | 2.705 → 2.154 |
| Score advance → level 2 arrival | 2.003 → 1.917 | 1.854 → 1.936 | 2.671 → 3.044 | 3.034 → 2.779 |
| Briefing 2 Skip → ready         | 2.453 → 1.726 | 2.307 → 1.508 | not present   | not present   |
| Briefing 1 closing → settled    | 0.944 → 0.533 | 0.862 → 0.539 | 1.217 → 0.564 | 1.168 → 0.537 |
| Exit closing → settled          | 1.170 → 0.805 | 0.809 → 0.621 | 1.245 → 0.592 | 0.858 → 0.609 |
| Briefing 2 closing → settled    | 1.057 → 0.425 | 1.018 → 0.436 | not present   | not present   |

D1 client level-two arrival increased 0.082 s and D2 host arrival increased
0.374 s. Individual samples also varied: cold D1 host exit was 3.716 s after
versus 2.795 s before. Native level-two network synchronization remained variable;
no improvement is claimed for all transitions or for next-level loading.
No level-two briefing appeared in the D2 shareware samples.

Both variants completed six samples and **42 native scripts each**: 12 completed
samples and 84 PASS receipts in total. Both peers were connected and unpaused at
level-one and level-two readiness. Actual CI-staged after scripts parse as strict
JSON and equal the before revision's complete native action data.

- [Comparison PNG](lan_transition_latency_20261020_results/after/comparison.png),
  [SVG](lan_transition_latency_20261020_results/after/comparison.svg),
  [summary CSV](lan_transition_latency_20261020_results/after/comparison.csv):
  means, medians, ranges, counts and individual values.
- [Before timing data](lan_transition_latency_20261020_results/before/timings.csv)
  and [after timing data](lan_transition_latency_20261020_results/after/timings.csv):
  **846 observed intervals each**, with sample, peer, revision, basis, local
  start/end and source filename. Per-variant manifests/provenance preserve
  configuration and original/exported source hashes. Missing phases are omitted.
- Per-variant `overview.svg`/`timings.svg` include startup and profiling details.
  `handshake_failure.csv`/`.svg` and `after_invalid_39378284578.json` retain failures.

## Configuration and revisions

| Variant                                                 | Exact revision                             | CI run                                                                                                 |
| ------------------------------------------------------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------------ |
| Latest fetched cmake baseline                           | `66e5e27da9850a9f142ac5c13d9e65432d48f4e3` | [39374851627](https://github.com/larsonm-personal/dxx-redux/actions/runs/39374851627), failed JOIN     |
| Instrumented before; synchronization behavior unchanged | `2eb278f486b62d603eb63b4a8cb94e48e83abb9d` | [39375337680](https://github.com/larsonm-personal/dxx-redux/actions/runs/39375337680), PASS            |
| Immediate changed-phase publication                     | `9268919164a114b7b97130afbf35bc801479fc1f` | [39378284578](https://github.com/larsonm-personal/dxx-redux/actions/runs/39378284578), invalid scripts |
| After, with corrected untimed script staging            | `04b6cf39e2d12b15d1514fd1343da62778155760` | [39380229137](https://github.com/larsonm-personal/dxx-redux/actions/runs/39380229137), PASS            |

Baseline was fetched October 20 and includes PRs
[#99](https://github.com/larsonm-personal/dxx-redux/pull/99) (public LAN smoke,
launcher/lobby options and missing-level checks),
[#100](https://github.com/larsonm-personal/dxx-redux/pull/100) (on-device fixes,
background route-metadata scheduling), and #101 multi-device fuzz work.
The existing public-shareware owner/reusable dual-emulator lane was extended.
Original `work` checkout was clean and preserved; work is on a new topic branch.
Nothing was merged or pushed to `cmake`.

Both runs: GitHub `ubuntu-24.04`, API 34 Google APIs x86_64 AVDs
`Nexus5X_Light_1`/`2`, 1536 MiB RAM and two configured cores each,
1280x720 at 320 dpi, KVM, headless `swiftshader_indirect`, shared Wi-Fi LAN.
Recorded emulator version matches: **37.2.12.0 build 16428233**. Debug x86_64 APKs
use NDK r30, CMake 3.31.6, compile SDK/build tools 37 and Java 21. Physical CPU
equivalence between runner allocations was not established. Public D1/D2 assets
use the pinned verification path in `android/helpers/shareware_test_data.ps1`.

Sample 1 clears app data before asset staging. Samples 2/3 restart the process,
retain asset/pilot caches and reset configuration/preferences. Level 2 is
process-warm. OS filesystem caches are not flushed: “cold” is not cold disk or
fresh physical hardware. Individual samples remain in the CSVs.

## Events and timing interpretation

Never subtract host and client clocks. Native script elapsed_ms uses local
SDL_GetTicks; phase/startup intervals use each device's `logcat -v monotonic`
prefix. The diagnostic message's `ms=` uses frame-cached timer_query and is
**not** an exact wall-clock timestamp. Controller Stopwatch intervals include
ADB/polling and are upper bounds, not engine CPU or wire latency.

| Interval                     | Start → end                                                                                                                       |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| Launcher controller          | Before Start-SetupActivity → helper returns; includes its configured 2 s hold and ADB                                             |
| Feedback publication         | Device launch_game command → preparation state published, then logged “Launcher game launch requested”; not first pixels          |
| Window displayed             | Same command → Android Displayed MainActivity; window-draw proxy, not meaningful-content visibility                               |
| Player selection complete    | Same command → host “closing select_players” after both players arrive                                                            |
| Briefing protocol ready      | Same command → first armed phase 6 log; not first pixels                                                                          |
| Start-to-briefing controller | Host lan_start_game request → each peer observed presenting phase 6; 800 ms per-peer polling plus ADB included                    |
| Skip → ready                 | Native step 2 Skip starts → step 3 observes in-game, briefing inactive, unpaused                                                  |
| Controlled exit → scores     | Native trigger_endlevel starts → postlevel_active after normal advance permission/request                                         |
| Scores → level 2 arrival     | Native score advance starts → level number becomes 2; **not readiness**                                                           |
| Level 2 controller ready     | Next advance or optional briefing Skip → level 2, in-game, unpaused, both peers connected                                         |
| Handshake                    | Phase 7 log → phase 0 through phases 8/9, same device/level/generation                                                            |
| Loading profiles             | Existing loadprof section durations in microseconds; nested sections must not all be summed                                       |
| Dwell/harness                | Briefing/scores observation → subsequent script request; screenshots, staging and deliberate input delay excluded from comparison |

All script post_delay_ms values are zero. Script staging precedes timed commands;
broadcasts to the two peers are sequential. Native action durations include engine
scheduling and the other peer's response, not just network transport. Pre-action
wait_for permission durations are retained separately in the timing CSV.

Player selection auto-starts at max_players=2; manual selection dwell was not
exercised. Before host selection completed 11.968–16.375 s after launch in D1
and 14.997–18.561 s in D2. Native auto-host start → selection completion was
4.698–8.227 s / 5.110–6.063 s, including connection setup and waiting for the peer.
There is no separate first-pixel timestamp for that menu. Screenshots were captured
at briefing, gameplay and scores, but not visually inspected locally because of
the transfer failure below. No exact time-to-visible-content claim is made.

Reactor destruction uses the existing real-engine co-op countdown seed fixture,
followed by trigger_endlevel and normal barriers. This is controlled exit, not
human combat, traversal, or a measured full reactor countdown.

## Fixed waits and remaining bottlenecks

- Host STATE retry: 250 ms; client ACK throttle: 100 ms. The change sends a new
  phase/generation immediately on the next network poll, while unchanged STATE
  retries still wait 250 ms. Every closure/loading/commit acknowledgment remains.
- Before preparation-state publication took 1–5 ms, window display 4.345–9.909 s,
  and armed briefing phase 6 took 16.379–24.492 s. After: 1–3 ms,
  3.961–8.933 s and 14.514–23.147 s. Startup differences between allocations
  cannot be attributed to the small barrier change; the large delay remains.
- D1 client first-level network-sync profiles were 4.630, 4.555, 4.506 s before:
  unusually repeatable. After they were 4.505, 2.476, 4.306 s, so these data do
  not establish an unconditional 4.5 s hold. Texture loading also takes roughly
  1–2 s. Investigate actual sync request/reply timestamps before another change.
- Source audit, not measured unconditional delays: game-info retry 1 s/timeout
  10 s; sync retry 2 s; reliable retry about 333 ms; disconnect timeout 15 s;
  connection timer_delay2(5) means 5 FPS, not 5 ms. Launcher discovery uses
  500 ms/2 s/6 s cadences, JOIN 1 s x three attempts, PREPARE/START immediate
  plus two 200 ms retries. Orphan shutdown polls 50 ms up to 2 s.
- RELEASE-only loading-message timer_delay(F1_0) is not established as enabled
  in this debug APK. No speculative timeout, loading hold or cache change is made.

## Failures and evidence access

1. Original unchanged smoke 39374851627 failed before native launch. Client
   JOIN attempts were 1.001 s apart; no JOIN_ACK after 3.003 s, despite host
   response logs. Discovery to first ANNOUNCE took 2.989 s on the client clock.
   Host membership alone did not prove client join; readiness timed out at 20 s.
   Later recovery's acceleration error occurred after this handshake failure.
2. First after attempt 39378284578 built successfully but D1 sample 1 scripts
   failed JSON parsing: line 7 column 9, unexpected closing brace. Formatting had
   added trailing commas; the helper copied raw JSONC to the strict native loader.
   Both Skip receipts show FAIL, zero steps. Roughly 116 s later the two-minute
   briefing deadline allowed entry; scores subsequently timed out. Its 467/368 ms
   timeout-driven closing handshake is **not a matched speed sample**. No sample
   completed. Artifact **12244907274** and compact invalid-attempt JSON are retained.

Repair: use existing ConvertFrom-JsoncText during untimed staging, validate strict
JSON and require native PASS receipts. All three recovered staged payloads equal
the successful before action data. The chart tool refuses failed/incomplete runs.
A JSONC semantic comparison alone had missed the initial runtime incompatibility.

Before artifact **12244470393** is 61,509,455 bytes; after artifact **12245932105**
is 61,399,122 bytes, both named nightly-results-android-dual. Complete bounded-text
exports are [39379623350](https://github.com/larsonm-personal/dxx-redux/actions/runs/39379623350)
and [39382396706](https://github.com/larsonm-personal/dxx-redux/actions/runs/39382396706).
Native logs in these exports are filtered excerpts; original/export hashes preserve
provenance. Full logs/screenshots remain in the CI artifacts, not the repository.

Followed `.github/TASK_RECOVERY.md` from PR #98: resumed the existing executor,
verified topic ref/identity, inspected existing runs before retrying. CLI auth and
GitHub connector decoded job logs worked. Connector ZIP retrieval returned a file,
but local transfer returned HTTP 403, including supported materialization. This
remains a **local file-transfer/visual-inspection limitation**, not a CI execution
blocker; bounded CI text export supplied all timing evidence. No credential or
security-setting changes, permission bypass, or duplicate competing experiment.
Local /dev/kvm was absent; existing GitHub CI supplied accelerated emulators.

## Validation and reproduction

Targeted D1/D2 coop_transition_policy CTest passed (CMake 3.31.6/GCC 14.2.0):
immediate changed phase/generation, unchanged-state retry boundaries and required
remote ACKs. Corrected Android experiment passed six/42; before passed six/42.
[Tooling checks 39375580457](https://github.com/larsonm-personal/dxx-redux/actions/runs/39375580457)
passed Ubuntu and Windows catalog/workflow checks. Final scoped C, Kotlin,
PowerShell, Python, YAML and script formatting/lint checks pass. Native JSON
conversion, failed-receipt rejection, failed-chart refusal and comparison
identity/missing-edge handling were exercised; no fixture timings enter results.

Full local desktop builds were attempted and failed in unchanged newdemo.c calls:
PHYSFS_sint64* versus int64_t* in classic_demo_backward_link_target under GCC 14.2.
Those files match baseline; no unrelated demo fix is included. Local full catalog
supervision could not read /proc/self/task/.../children; corresponding GitHub
Linux/Windows tooling checks passed. These are explicit validation limitations.

```sh
# Current after experiment; before engine revision is recorded above.
gh workflow run public-lan-smoke.yml --ref experiment/lan-transition-latency-20261020 -f measure_transitions=true
# Direct invocation needs two distinct verified disposable emulators:
pwsh android/tests/test_lan_launch_preparation.ps1 -PublicShareware -MeasureTransitions
# If direct artifact transfer is unavailable (this runs no emulator):
gh workflow run public-lan-smoke.yml --ref experiment/lan-transition-latency-20261020 -f evidence_run_id=RUN
python3 android/helpers/collect_lan_evidence.py EXPORT_RUN --output EVIDENCE
python3 android/helpers/chart_lan_transitions.py BEFORE/first-play/lan-launch-preparation-public-shareware --output results/before
python3 android/helpers/chart_lan_transitions.py AFTER/first-play/lan-launch-preparation-public-shareware --output results/after --compare results/before/timings.csv --png
cmake --build buildd1-test --target test_coop_transition_policy -j2
cmake --build buildd2-test --target test_coop_transition_policy -j2
ctest --test-dir buildd1-test -R coop_transition_policy --output-on-failure
ctest --test-dir buildd2-test -R coop_transition_policy --output-on-failure
```

SVG/CSV generation uses the standard library; optional PNG uses matplotlib.
Controller bounds and dwell remain in underlying data but are excluded from the
comparison chart. The default public smoke remains unchanged unless measurement
is explicitly enabled. Before reproduction uses the recorded instrumented before
revision on a separate topic checkout with the same direct emulator command.
