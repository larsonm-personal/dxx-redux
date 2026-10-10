# Save cost experiments on S21

## Questions and plan

- Measure D1-in-D2 level 6 on physical SM-G996U, recording wall and game-thread CPU time
- Compare fresh-buffer memory capture, disk save, and thumbnail on/off with interleaved repeated samples
- Separate opening/header, thumbnail, live world, runtime extensions, coop recovery/metadata, dormant campaign validation/encoding/checksum, close, disk validation and publication
- Compare single player, solo coop host, and two-peer host/client where available; do not infer Wi-Fi latency from local save CPU time
- Inspect sizes/counts and retained campaign state; reproduce expensive input if the new-game control is fast
- Run an allocation reuse control and observational logging control if indicated by measurements
- Preserve installed production app and its private saves by using a separate debug package; use dedicated benchmark save paths
- Keep save semantics unchanged; instrumentation is enabled only by explicit automation commands
- Build both engines, run experiments, and record reproducible commands, raw evidence, limitations, and rework implications

## Results

Completed on physical Samsung SM-G996U (S21 family), serial R3CR40Q4XPK,
Android 15, with an isolated `com.dxxredux.app.nsdtest` Debug build, version 24570. The production `com.dxxrevival.app` installation and its private data
were not modified. Standard D1 v1.4a and D2 v1.2 assets were hash verified.
The connected peer used the same instrumented source built for x86_64 on
emulator-5586. No performance conclusions use emulator timings.

All main matrices use level 6, Trainee, 12 repetitions per variant, rotating
variant order each round and explicitly waiting 250 ms between saves. The
coop fixtures increase shields to survive idle collection. Saves happen on
the game thread in live gameplay. Both peers were verified connected before
the two-player matrix. A separate test uses eight memory captures five
seconds apart. Android reported thermal status 0 before/after testing.

Median elapsed times, milliseconds:

| Scenario                 | Memory, blank | Memory, reused buffer | Disk, blank | Disk, thumbnails |
| ------------------------ | ------------: | --------------------: | ----------: | ---------------: |
| Native D1 single player  |          6.99 |                  5.15 |       14.12 |            35.41 |
| D1-in-D2 single player   |         25.56 |                 26.03 |       40.84 |            63.94 |
| D1-in-D2 solo coop host  |         25.13 |                 20.93 |       42.40 |            61.58 |
| D1-in-D2 two-player host |         25.25 |                 26.23 |       37.44 |            65.81 |

Memory captures with thumbnails also cost substantially more: two-player
host median 54.58 ms versus 25.25 ms blank. Reusing allocation does not
consistently improve D1-in-D2 capture: it is not the primary cost.

The five-second two-player memory test measured 18.68, 25.64, 26.06, 23.62,
19.35, 29.11, 23.49, and 29.49 ms. Median elapsed/CPU time was 24.63/24.39 ms.
The two-player disk/thumbnail matrix reached 75.83 ms, median CPU 46.43 ms
versus elapsed 65.81 ms. These are save-call durations, not complete frame
intervals or displayed-frame timings.

### Attribution

Median stages from the two-player S21 host, milliseconds:

| Stage                                | Memory blank, elapsed | Disk thumbnails, elapsed | Disk thumbnails, CPU |
| ------------------------------------ | --------------------: | -----------------------: | -------------------: |
| World serialization                  |                  3.05 |                     2.88 |                 2.85 |
| Runtime before navigation metadata   |                  0.72 |                     0.85 |                 0.85 |
| Navigation metadata size calculation |                  9.29 |                    10.39 |                10.31 |
| Navigation metadata encoding/write   |                 11.60 |                    15.22 |                15.13 |
| Thumbnail work                       |                  0.01 |                    26.08 |                15.06 |
| File close/flush                     | included in tiny tail |                     8.58 |                 1.06 |
| File validation                      |                absent |                     0.26 |                 0.26 |

Medians of individual stages need not add exactly to the median total.
The stages are sequential laps, not overlapping parent/child totals.

The dominant memory cost is `level_metadata_save_runtime()` in
`android/app/src/main/cpp/shared/secretarea.c`. `escort_save_runtime()` first
calls it with `writing=2` to calculate a framed section length, then again
to write the section. The counting pass still traverses and encodes the
individual fields; only the eventual byte write is skipped. The schema in
`guidebot_metadata_save_fields.h` walks fixed-capacity route steps, planner
state, certificates, and workspaces, including unused array entries. The
much smaller `escort_route_save_runtime()` takes only about 0.03 ms per pass.
No Guide-Bot object was present in the measured coop mine.

Coop preparation, player metadata, checksum and trailer writing together
take well under 0.1 ms in these fixtures. They have zero recovery items and
zero dormant-world bytes. Campaign encoding/validation is correspondingly
tiny. This does not bound the cost of a large populated recovery ledger or
an archived secret/base mine, but such data is not required to reproduce
the dominant serializer cost.

D1-in-D2 saves are about 1.2 MB in memory; native D1 is about 0.31 MB. The
memory save skips the Android launcher metadata trailer, which disk saves
include. A rework cannot blindly publish a rewind blob as a complete launcher
save without also supplying the disk-specific metadata.

The disk close stall is consistent with storage waiting: PhysFS close flushes
the buffered writer and its POSIX backend calls `fsync`. The measured close
stage includes these operations; there is no separate syscall probe claiming
all of its time is inside `fsync`. Thumbnail work includes both the legacy
thumbnail and launcher thumbnail render/readback paths.

### Additional checks and limits

- Single player and connected coop have similar D1-in-D2 costs when measured
  with the same spacing. The initial single-player burst was faster (17.52 ms
  memory median); it is retained as exploratory evidence, not used for the
  matched comparison. `post_delay_ms` does not delay the set-debug action, so
  the runner was corrected to issue explicit `wait_ms` steps
- The emulator client successfully performed 15 independent local saves,
  covering all five variants while remaining a non-master with two players.
  This proves the current writer accepts these local captures; it does not
  prove their future host-promotion/restore semantics or measure S21 client
  performance
- The fresh S21 fixture did not reproduce the previous SM-S931U capture's
  47-81 ms memory snapshots. Its 19-30 ms periodic captures are still a
  material frame cost. Different hardware, live mine state, rendering load
  and CPU scheduling remain possible contributors to the earlier magnitude
- The recollected 1-2 ms single-player save time was not reproduced by this
  Debug build. Native D1 was substantially faster than D1-in-D2, but a
  historical build or optimized release comparison has not been run
- The two independent 30-second host timers still exist in `game.c` and
  `multi.c`. They were inspected, not changed by this experiment
- No checkpoint scheduling, networking, save format or threading redesign
  was implemented

## Implications for a rework

1. Address navigation serialization first: avoid doing the full encoding work
   just to discover its size. Consider caching a schema length or backpatching
   the length after one serialization pass. Then measure the remaining write
   pass before choosing larger architectural changes
2. Examine fixed-capacity cached/planner state separately from the minimal
   state needed to resume deterministically. Do not drop it indiscriminately;
   rewind and Guide-Bot restore need their existing fidelity tests
3. Keep coherent reads/required normalization on the game thread. Background
   work must own independent captured data and the capture-time clock value;
   the current serializer reads and sometimes changes live globals
4. Offloading disk publication/flush can remove an additional stall, but
   cannot fix the navigation CPU work in memory-only rewind captures
5. Use blank/deferred/cached thumbnails for automatic captures if acceptable;
   the measured synchronous render/readback cost is large
6. Consolidate the two autosave schedulers and let peers keep independent
   checkpoints. Transfer the selected authoritative save at load time. Audit
   the old local-slot restore fallback and test promotion/restore from a
   former client's checkpoint as part of that behavioral change

## Reproduction and evidence

Instrumentation is disabled unless the debug automation action
`set_debug: save_probe` explicitly starts a sample. It collects monotonic
wall and thread-CPU clocks into bounded sequential laps. Report JSON writes
and diagnostic logging happen after the measured interval. `memory_blank`
uses a new buffer exactly like rewind; `memory_reuse` retains capacity;
disk variants use `state_android_save_to_path` including validation and
publication at the dedicated `Players/__save_probe__.sav` path. Finish
discards retained buffers and deletes that path. The runner requires the
isolated test package on physical devices.

Build (quote the ABI property in PowerShell):

```powershell
$env:JAVA_HOME = 'C:\local\jdk-21'
$env:Path = "$env:JAVA_HOME\bin;$env:Path"
# From android/
.\gradlew.bat :app:assembleDebug '-PnsdDiagnosticApp=true' '-Pandroid.injected.build.abi=arm64-v8a' --offline --console=plain
```

This AGP invocation published to `app/build/intermediates/apk/debug/`,
referenced by its IDE redirect metadata. The older `outputs/apk/debug/`
artifact was stale. Verify `output-metadata.json` before installing; the
ABI-injected APK requires `adb install -r -t`.

Start level 6 in the isolated app using the launcher/engine automation APIs,
then from the repository root:

```powershell
.\android\helpers\measure_save_cost.ps1 -Serial R3CR40Q4XPK -Rounds 12 -Output temp/save-cost.json
.\android\helpers\measure_save_cost.ps1 -Serial R3CR40Q4XPK -Rounds 8 -Modes memory_blank -IntervalMilliseconds 5000 -Output temp/save-periodic.json
```

The reusable runner creates and retains the exact automation script beside
each report. It targets an already-running game; it does not clear user data
or choose the scenario. It is an explicit performance experiment, not a
standalone regression test registered in the automatic test suite.

Raw reports, scripts, launch scripts, automation pass/fail records, setup and
game introspection, thermal readings and aggregate `summary.json` are in
`temp/save-experiments-20261009/`. Primary reports:

- `single-detailed-spaced.json`
- `solo-coop-detailed.json`
- `two-player-host.json`
- `two-player-five-second-memory.json`
- `native-d1-l6.json`
- `emulator-client-functional.json`

There were 368 successful S21 sample saves including exploratory runs and
15 successful emulator client sample saves. Setup failures (pilot menu
selection and an early non-game benchmark) are retained separately and were
corrected before collecting the reported matrices.

## Validation and cleanup

- Windows D1 and D2 builds passed, including the detailed instrumentation
- Android Debug arm64 and x86_64 D1/D2 libraries and APKs built successfully
- `test_upstream_compat`, `test_coop_save_format`, `test_coop_campaign`, and
  `test_android_rewind_policy` passed in both Windows build trees (8 tests)
- Scoped source/PowerShell/Markdown format checks and scoped diff whitespace
  checks passed; native warnings were existing state/net sites
- Diagnostic games were stopped and the S21's original stay-awake setting
  (0) restored. The isolated test app remains installed for reproduction
