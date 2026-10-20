# LAN transition latency experiment

Baseline: origin/cmake 66e5e27da9850a9f142ac5c13d9e65432d48f4e3,
committed 2026-10-19T22:08:01-07:00, fetched 2026-10-20. Clean original
`work` checkout preserved; experiments use a new branch.

Plan:
1. Verify unchanged public dual-emulator smoke in existing GitHub CI (local
   /dev/kvm and Android tools unavailable). Inspect PRs 99, 100 and 101.
2. Add opt-in measurements to the existing public asset owner, enable briefing
   automation, collect per-peer monotonic observation bounds plus native logs,
   repeat native D1/D2 cold-process and warm-process transitions.
3. Chart actual observations and audit fixed waits. Only change engine behavior
   when measurements justify it; repeat identical experiments and check sync.
4. Publish compact data/report and harness; keep bulky logs in CI artifacts.

Never subtract clocks between devices. Controller Stopwatch durations bound
state observations; native automation elapsed_ms is local to each script.
Observation latency includes ADB and polling and must not be called engine CPU
or network time. Deliberate skip/score input and process setup are separate.
`trigger_endlevel` is a controlled real-engine exit fixture, not a played mine;
reactor seed is a test setup operation, not measured player combat.

## Source audit (hypotheses, not measured results)

- `coop_briefing.c`: host STATE retransmit cadence 250 ms; client ACK cadence
  100 ms. Each acknowledged closing/loading/commit barrier can wait for the
  next periodic transmission. Keep retries and all participant/generation
  checks if accelerating changed state.
- `net_udp.c` in both engines: join game-info retry 1 s; connection timeout
  10 s; sync request retry 2 s; reliable packet retry about 333 ms; disconnect
  timeout 15 s. A retry is not an unconditional minimum duration.
- `LobbyService.kt`: initial discovery queries 500 ms, searching 2 s, idle
  populated discovery 6 s; JOIN retries 1 s (three tries); joined lobby refresh
  3 s; PREPARE/START sends immediately then twice more 200 ms apart.
- `SetupActivity.kt`: orphan process shutdown polls at 50 ms up to 2 s. Launch
  preflight records per-step durations and native level loading already emits
  `loadprof_v=1` breakdowns, including network synchronization.
- `gameseq.c`: a RELEASE-only loading-message hold calls `timer_delay(F1_0)`.
  Do not infer it is enabled in the debug APK without build/log evidence.
- Test infrastructure: Start-SetupActivity force-stops then sleeps 2 s before
  `am start`; Get-GameIntrospection sleeps 800 ms per peer. The experiment's
  launcher controller interval includes that configured 2 s harness hold.
  Native script post_delay_ms is explicitly zero. Screenshot/file collection
  and staging between briefing/scores and input are dwell/harness, not engine
  transition work. `timer_delay2(5)` is a 5 FPS limiter, not a 5 ms delay.

## Initial unchanged-baseline run

Run 39374851627 tested exact baseline 66e5e27 and failed before engine launch.
Host received JOIN at 05:37:31.214, 32.215, 33.216 UTC and logged sending its
responses. Client sent JOIN at 31.215, 32.217, 33.217, then reported no JOIN_ACK
at 34.218. Actual-send intervals were 1.002 s then 1.000 s; the chart uses the preceding
`joinLobby: attempt` events (1.001 s each). Total failed
attempt 3.003 s. Discovery start 05:37:27.748 to first ANNOUNCE 30.737 took
2.989 s on that client's clock. Host-only membership was therefore not proof
that the client had joined. The owner waited 20 s for readiness, then failed.
These are observed failure timings, not successful gameplay-transition data.
Recovery then reported unavailable emulator acceleration; this happened after
the test's handshake failure, not before the APK build or initial boot.

Direct artifact and reusable-job log downloads returned HTTP 403 from the
storage endpoint, including the permitted unrestricted execution attempt.
A separate opt-in workflow job downloaded the public artifact inside GitHub
and exported bounded text through the check annotations API. It does not
change security settings, publish repository content, or need a merge.
Source-file hashes and export hashes are retained by collect_lan_evidence.py;
exported native logs are explicitly filtered excerpts, not complete originals.

Reproduction:

```sh
gh workflow run public-lan-smoke.yml --ref experiment/lan-transition-latency-20261020 -f measure_transitions=true
# If direct artifact download is unavailable, replace RUN with the completed measurement run:
gh workflow run public-lan-smoke.yml --ref experiment/lan-transition-latency-20261020 -f evidence_run_id=RUN
python3 android/helpers/collect_lan_evidence.py EXPORT_RUN --output temp/lan-evidence
python3 android/helpers/chart_lan_transitions.py temp/lan-evidence/first-play/lan-launch-preparation-public-shareware --output temp/lan-charts
```

The original smoke remains the default. Direct emulator invocation:
`pwsh android/tests/test_lan_launch_preparation.ps1 -PublicShareware -MeasureTransitions`.
The configured primary/secondary targets must be distinct disposable emulators.

## Instrumented baseline observed before optimization

Run 39375337680, target 2eb278f486b62d603eb63b4a8cb94e48e83abb9d, passed
all six samples (D1/D2 x three) and all 42 native automation executions.
The target adds diagnostics and an automation option for the already-supported
co-op briefing setting; it does not change engine synchronization behavior.
The failed original smoke above remains a separate result.

`lan_transition_latency_20261020_results/before/overview.svg` and `timings.csv`
were generated from this run. D1 host input-to-ready means were 1.976 s after
first briefing Skip, 2.849 s from controlled exit to scores, 2.003 s from score
advance to level number 2, and 2.453 s after next briefing Skip. D2 host means
were 2.433 s, 3.035 s and 2.671 s respectively; no level-two briefing appeared
in the D2 shareware samples. Client measurements are separate CSV rows.

Closing/loading/commit phase residence totals averaged 1.057 s on the D1 host
(n=9 operations) and 1.231 s on the D2 host (n=6). These use the timestamp from
`logcat -v monotonic` on each individual device. The `ms=` field in the native
phase diagnostic uses frame-cached timer_query and is NOT used as an exact
wall-clock timestamp. Native automation elapsed_ms uses SDL_GetTicks directly.

Decision: publish a changed generation/phase immediately instead of waiting
for the next 250 ms periodic STATE send. Keep periodic retransmission, packet
format, client ACK throttle, participant/generation validation, and every
closure/loading/commit acknowledgement barrier. Add regression coverage for
immediate changes, unchanged-state retry boundaries, and missing remote ACKs.

For the matched after run, keep the successful measurement owner/helper and
all automation actions/delays identical. A prospective reachability/failed-case
continuation refinement is retained only in ignored temp, not applied to the
comparison. Do not shorten timeouts to hide the separate failed JOIN sample.

## Measurement definitions and configuration

Both variants use GitHub `ubuntu-24.04`, two API 34 Google APIs x86_64
AVDs (`Nexus5X_Light_1`/`2`, serials 5554 host and 5556 client), 1536 MiB
RAM and two cores each, 1280x720 at 320 dpi, KVM, headless
`swiftshader_indirect`, and the existing shared-WiFi LAN setup. Emulator
37.2.12.0 (build 16428233) was recorded in the before artifact. These are
software-rendered virtual devices sharing one runner, not physical-phone
performance measurements. Public shareware assets come from the existing
owner's pinned download/verification path.

There are three sequential samples per game and peer per variant, not three
independent machines. Sample 1 clears application data before asset staging;
samples 2/3 restart the process while retaining assets/pilot caches and
resetting preferences/configuration. Level 2 is process-warm. OS filesystem
caches are not flushed: “cold” here does not mean cold disk or freshly booted
hardware. Individual samples remain in CSV; an average is not a significance
test, and run order is before then after, not randomized/interleaved.

| Interval/basis | Start | End and limitation |
| --- | --- | --- |
| Launcher controller | Before `Start-SetupActivity` | Helper returns; includes its deliberate 2 s hold and ADB |
| Launch feedback | Device-local `MP_COMMAND: launch_game` | `Launcher game launch requested` state publication; not a first-pixel observation |
| Window displayed | Same launch command | Android `Displayed ... MainActivity`; window-draw proxy, not proof of meaningful progress |
| Briefing protocol ready | Same launch command | First armed phase 6 log; native protocol milestone |
| Start-to-briefing controller | Host `lan_start_game` request | Each peer's fresh introspection observes presenting phase 6; includes ADB/800 ms per-peer polling |
| Skip action-to-ready | Native script step 2 starts Skip | Step 3 observes in-game, briefing inactive, unpaused |
| Exit action-to-scores | Native script step 2 starts `trigger_endlevel` | Step 5 observes postlevel active after normal advance permission/request |
| Scores-to-level-2 arrival | Native score advance starts | Level number becomes 2; explicitly **not** readiness |
| Level-2 controller ready | After arrival/optional briefing Skip | Level 2, in-game, unpaused, both peers connected; D1 also briefing inactive |
| Closing-to-settled handshake | Phase 7 log | Phase 0 log through phases 8 and 9, same level/generation/device |
| Native loading profile | Existing loadprof section entry | Existing microsecond section duration, e.g. textures or network sync |
| Dwell/harness | Briefing/scores observed | Subsequent automation request; captures screenshots, copying and deliberate input delay separately |

Script durations use local `SDL_GetTicks`, phase/startup durations use each
device's `logcat -v monotonic` prefix, and controller intervals use Stopwatch.
No host timestamp is subtracted from a client timestamp. Native condition
timestamps include engine scheduling and the other peer's response; they
are not pure wire latency. ADB broadcasts to the peers are sequential.
The comparison chart excludes controller observation bounds entirely.
All script `post_delay_ms` values are zero. Scores and briefings are not
automatically charged their deliberate viewing/staging dwell.

The two-player native player-selection flow auto-starts when both players
arrive (`max_players=2`); manual selection dwell was not exercised. Its
connection work is included in launch-to-briefing and existing native
network-sync profiles, but there is no separate measured first-pixel marker
for that screen. Screenshots were captured at briefing, gameplay and scores
in the original CI artifact; they were not visually inspected through the
blocked local artifact endpoint. No claim of exact time-to-visible-content
is made. Reactor destruction is seeded through the existing real-engine
co-op countdown fixture, followed by controlled exit, not a complete played
combat level or a measurement of player traversal.

Before startup state publication took 1–5 ms, window display 4.345–9.909 s,
and armed briefing phase 6 took 16.379–24.492 s. This is the largest observed
delay and is not solved by the small handshake change. First-level native
network-sync durations on the D1 client were 4.630, 4.555 and 4.506 s:
an unusually repeatable wait worth investigating alongside the 2 s sync
retry and 5 FPS connection polling. The source alone does not establish
which wait caused those observations. Texture loading also took roughly
1–2 s on these virtual devices. No speculative timeout, loading-message
hold, or asset-cache change is included.

## Validation and evidence access

- Before runtime run: [39375337680](https://github.com/larsonm-personal/dxx-redux/actions/runs/39375337680),
  `nightly-results-android-dual` artifact **12244470393**, 61,509,455 bytes.
- Initial failed original-baseline smoke:
  [39374851627](https://github.com/larsonm-personal/dxx-redux/actions/runs/39374851627).
- Complete before text export:
  [39377769444](https://github.com/larsonm-personal/dxx-redux/actions/runs/39377769444).
  Earlier incomplete annotation-export attempts were rejected, not used as data.
- Ubuntu and Windows tooling/catalog checks:
  [39375580457](https://github.com/larsonm-personal/dxx-redux/actions/runs/39375580457), both passed.
- Local D1/D2 `coop_transition_policy` CTest passed with the optimization.
  Coverage checks immediate changed phase/generation, unchanged-state 250 ms
  loss recovery, and that remote acknowledgment remains necessary.
- Scoped mixed-language format/lint and automation catalog validation passed.
  The comparison parser was checked against the observed before data using
  identity comparison (all deltas zero, three samples in each comparable
  group); that check creates no fabricated benchmark data.

Full local desktop builds were attempted for both engines and failed in
unchanged `newdemo.c` calls to `classic_demo_backward_link_target`:
`PHYSFS_sint64 *` is incompatible with the helper's `int64_t *` on this
Linux toolchain. Those three source files are unchanged from baseline.
The targeted transition regression builds pass; Android runtime builds are
separately checked in the dual-emulator runs. No unrelated demo fix is included.
Local full catalog supervision could not read `/proc/self/task/.../children`;
the same catalog checks passed in GitHub Linux and Windows tooling CI.
