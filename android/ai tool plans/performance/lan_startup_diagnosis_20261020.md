# Initial LAN startup diagnosis

Verified October 20: origin/cmake is 66e5e27da9850a9f142ac5c13d9e65432d48f4e3.
PR #103 is OPEN, not merged, at 01401425aff549eefd8b5a9e9fd8a8458d01a9b8.
Investigation uses a separate worktree/branch based on that PR head, preserving it.
Existing measured engines are 2eb278f (before) and 04b6cf3 (after).

Plan:
1. Recover original startup diagnostics for successful runs 39375337680 and
   39380229137 before running new emulator work.
2. Build device-local critical-path timelines. Separate launcher preflight,
   Activity/library startup, native initialization, mission/player selection,
   level loading, network synchronization and briefing protocol creation.
3. Trace observed gaps into current source, distinguishing normal launcher
   behavior from test-only automation. Never sum overlapping peer stages.
4. Add focused logging/experiments only where existing evidence cannot identify
   the work or retry causing a delay. Use solo comparison if it isolates a gap.
5. Report quantified causes and remaining uncertainty; no speculative redesign,
   merge, history rewrite or push to cmake.

## Recovered evidence and current findings

Saved successful after run: [39380229137](https://github.com/larsonm-personal/dxx-redux/actions/runs/39380229137), engine `04b6cf39e2d12b15d1514fd1343da62778155760`.
Saved before: run 39375337680, engine `2eb278f486b62d603eb63b4a8cb94e48e83abb9d`.
Recovered startup excerpts through the supported check-annotation exporter; full
ZIP downloads still return HTTP 403 in this environment. Source/export hashes
are retained by `collect_lan_evidence.py`. Export 39436221786 succeeded for after,
39436228702 for before, and 39437796721 recovered additional lifecycle/init tags.
Two earlier expanded exports exceeded the helper's 128 KiB compressed limit;
no device work was rerun for those export attempts.

Host non-overlapping startup intervals in the successful after run, mean of
three samples, seconds (elapsed time, not CPU time):

| Stage | D1 | D2 |
| --- | ---: | ---: |
| launch command to Activity intent | 1.395 | 3.721 |
| Activity intent to library-loaded log | 1.964 | 1.618 |
| library-loaded to native LAN entry | 3.972 | 4.321 |
| native LAN entry to first level load | 6.746 | 5.493 |
| first level load, including pre-load | 2.256 | 2.825 |
| native synchronization | 1.307 | 2.068 |
| post-sync work to briefing protocol ready | 0.144 | 0.326 |
| **Total** | **17.783** | **20.372** |

The Activity/library interval is predominantly *before onCreate*. Across both
peers and games, onCreate to the library-loaded log is only 15-68 ms; that
interval includes `System.loadLibrary` plus Activity setup. Do not label the
whole 1.6-2.0 second interval as dynamic library loading.

D2 has finer existing native checkpoints. Host base-resource mounting takes
1.614 / 0.747 / 1.196 s; arch_init 0.163 / 0.326 / 0.104 s; gr_set_mode
0.370 / 0.362 / 0.516 s; gamedata_init 0.847 / 0.962 / 1.120 s; following
piggy_init_pigfile 0.492 / 0.596 / 0.358 s. SwiftShader is the observed renderer,
but these timings do not support attributing all native initialization to it.
`input_demo_assets_mount` calls `input_demo_assets_note_archive`, which hashes
the full HOG synchronously even when the recorder is inactive. Asset stream
fingerprints also hash PIG/HAM data synchronously. The existing checkpoints
bound this work but do not isolate hashing; new wall/thread-CPU probes target it.

The startup protocol serializes substantial loading. The client completes its
first LoadLevel before sending UPID_REQUEST. That request admits the client to
host player selection; only after selection closes does the host start its own
LoadLevel. The client remains in network_sync while the host loads. Therefore
client network_sync includes host CPU/I/O work and must not be described as
pure network latency or added to the host's overlapping time.

Host entry into level sync flushes packets, then wait_for_requests flushes
again and resets other players to CONNECT_WAITING. The client retries its
request every two seconds. A known-player request also needs an authenticated
challenge/proof exchange. Whether a queued readiness request is discarded and
adds a retry interval needs packet evidence, not inference from a two-second
elapsed duration. Diagnostic run 39437557537 at
`713a5b739f22d5e8c88614e99a1d1385ccfa18f5` adds sparse request, challenge/proof,
readiness acceptance, SYNC, and per-type flush-count markers with no protocol
behavior changes.

## Measurement boundaries

`chart_lan_startup.py` reads each device's `logcat -v monotonic` independently.
Profile boundaries are reconstructed from the logged local `level_init`
durations, with log timestamp precision and logging latency as practical limits.
No host timestamp is subtracted from a client timestamp. Stacked stage bars are
non-overlapping per device; the peers' bars are not summed.

Start is receipt of `MP_COMMAND: launch_game`, after launcher lobby setup and
ready selection. End is first phase 6 (`armed=1`) protocol-ready event, not the
user's later briefing skip and not a screenshot proving the first visible frame.
Launcher state publication and Activity `Displayed` logs are feedback proxies,
not measured screen visibility. Native `auto_net` is used by the normal Android
launcher, not solely a test bypass. This experiment sets max_players=2, so host
selection closes automatically when both players arrive; a normal non-full
lobby may instead wait for the host's Start Game input.

Sample 1 clears app data and stages shareware; samples 2-3 keep app/assets but
restart the game process. None is a guaranteed cold OS page cache. Host/client
launch commands are sent serially over adb. The 800 ms per-peer observation
sleep and later screenshot/automation staging affect controller bounds and
briefing dwell, not the device-local endpoints above. There is no slow-host
hold on this timing path. `timer_delay2(5)` is 5 FPS pacing, not a five-second
sleep. Existing logs show no orphan-process retirement in the 24 before/after
peer captures.

## Reproduction and validation

```sh
gh workflow run public-lan-smoke.yml --ref investigate/lan-startup-20261020 -f measure_transitions=true
# Recover an existing run without running emulators:
gh workflow run public-lan-smoke.yml --ref investigate/lan-startup-20261020 -f evidence_run_id=39380229137
python3 android/helpers/collect_lan_evidence.py EXPORT_RUN --output temp/startup-evidence
MPLCONFIGDIR=/tmp/matplotlib-startup python3 android/helpers/chart_lan_startup.py \
  temp/startup-evidence/first-play/lan-launch-preparation-public-shareware \
  --output temp/startup-chart
```

Use the artifact retention helper before creating a new output root. Exact run
revision is in `experiment.json`; do not assume the moving branch head matches
an earlier run. Existing measurement harness and pinned emulator configuration
are unchanged. Local virtualization is unavailable (`/dev/kvm` absent), so the
supported GitHub dual-emulator lane is used.

Scoped mixed code quality passed. Both desktop net_udp objects compile with the
new hooks. D1 and D2 each pass the existing reconnect-auth, initial-sync-retry,
and co-op-transition-policy CTests (3/3 per engine). Android execution status and
any candidate fix results are recorded below when available.
