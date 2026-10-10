# Detached navigation snapshot experiments

## Plan

- Measure preallocated native metadata capture on the attached S21 in D1-in-D2 level 6
- Use one metadata member list for the existing serializer and an experiment-only native snapshot, preserving field order and encoding
- Compare a capture-frame normal encoding with delayed worker encoding while gameplay advances
- Validate detached decode/re-encode byte equality without applying data to the live game
- Record native/encoded sizes, copy wall/CPU time, worker encoding time, and game advancement
- Rotate preallocated capture slots to avoid measuring only a single hot destination
- Keep diagnostics explicitly invoked, production save scheduling and behavior unchanged
- Build Android D1/D2 and Windows, run the physical-device experiment, document results and limits

## Results

Completed initial matrices on the attached SM-G996U, Android 15, ARM64 Debug
build 24570 in the isolated `com.dxxredux.app.nsdtest` package. The production
application and its private data were not changed.

The probe uses 12 preallocated, pre-touched native snapshot slots. Each capture
copies the 33 top-level metadata values/structures on the game thread. It then
encodes the live state as an independent reference, starts a worker with a 50 ms
delay, and returns to gameplay. The worker encodes the detached snapshot and
performs a detached decode/re-encode round trip. Verification compares exact
bytes, checks game time advancement, and compares the current live encoding
with the capture reference using the original epoch.

All 33 original serializer operations were checked against the pre-change
source for exact order and arguments. The live and detached serializers share
one member list; the underlying field encodings and save version are unchanged.

## Measurements

All measurements are from the physical S21. The emulator is only a connected
peer, running the prior diagnostic build with the same save schema. The two-peer
host had two players in every sample and two connected players in fresh
introspection during and after collection. Both coop fixtures use the existing
shield/gear seed to survive idle measurement; no Guide-Bot was deployed.

Elapsed milliseconds:

| Scenario                           | Samples | Native capture median | Capture maximum | Direct encoding median | Worker encoding median |
| ---------------------------------- | ------: | --------------------: | --------------: | ---------------------: | ---------------------: |
| Single player, about 1 s spacing   |      24 |                 0.054 |           0.171 |                  7.820 |                 10.958 |
| Solo coop host, about 1 s spacing  |      24 |                 0.052 |           0.101 |                  8.000 |                 12.247 |
| Two-player host, about 5 s spacing |      12 |                 0.115 |           0.147 |                 16.637 |                 20.868 |

The direct encoder column is one metadata write pass, excluding the existing
separate size-calculation pass. Two-player capture median CPU time was 0.105 ms;
direct encoding was 16.445 ms CPU and worker encoding was 19.034 ms CPU. The
two-player run changed both spacing and gameplay load, so its longer encoding
times cannot be attributed specifically to networking. Thermal status was 0.

All 60 captures matched the capture-frame reference byte for byte. All 60
detached decode/re-encode checks passed, and gameplay advanced in every sample.
The live metadata changed between capture and verification in all 24 single-
player samples; it remained unchanged in the 36 coop samples. Thus byte equality
also held under demonstrated source-state changes, not just idle identical data.

Each native metadata snapshot is 470,352 bytes (459.33 KiB), versus 868,160 bytes
(847.81 KiB) encoded. Twelve native snapshots consume 5,644,224 bytes (5.38 MiB),
compared with 9.94 MiB for twelve encoded metadata sections. These are metadata
sizes, not complete game snapshots. The diagnostic also allocates reference,
round-trip and worker output buffers, which are not included in the pool size.

## Interpretation and boundaries

- A straightforward bulk capture is already cheap enough that persistent or
  copy-on-write data structures are unnecessary for this metadata's frame cost
- Snapshot encoding reads only the captured values and writes worker-owned
  memory. It needs no shared-game-state mutex
- The completion atomic reports lock-free on this device. This experiment uses
  one outstanding job and per-sample pthread creation/join; it does not implement
  or benchmark a production lock-free queue
- A production path should prestart the worker, preallocate slots, publish owned
  jobs through a bounded single-producer/single-consumer queue, and defer/coalesce
  periodic work when full instead of waiting on the game thread
- Capture timings include only the copies and clock overhead. Allocation, page
  touching, direct reference encoding, thread creation, verification, and JSON
  report I/O are excluded. The diagnostic itself intentionally does substantial
  reference/verification work on the game thread; this is not an end-to-end
  stutter reduction measurement
- Worker elapsed time can exceed its CPU time under concurrent gameplay. Moving
  the work changes frame-thread cost, not the total CPU required to encode it
- The existing size-calculation pass repeats field encoding. A future detached
  path can determine framing without performing that work on the game thread
- Native snapshots are process-local data, not a portable disk/network format
- These checks validate metadata capture, encoding and detached round trips.
  They do not prove full-world restore, host migration, complete save capture in
  1-2 ms, or production queue/backpressure/reclamation behavior
- Production checkpoint scheduling, restore behavior and save formats are
  unchanged; diagnostics run only when explicitly requested by automation

## Reproduction and evidence

Reusable runner for an already-running D1-in-D2 level 6 fixture:

```powershell
.\android\helpers\measure_metadata_snapshot.ps1 -Serial R3CR40Q4XPK -Rounds 24 -Output temp/snapshot-single.json
.\android\helpers\measure_metadata_snapshot.ps1 -Serial R3CR40Q4XPK -Rounds 12 -IntervalMilliseconds 2500 -Output temp/snapshot-spaced.json
```

Each sample issues capture, a wait, verification, and another wait. Default
500 ms waits make captures approximately one second apart; 2500 ms waits make
captures approximately five seconds apart. Exact generated automation, raw
samples, durable PASS records, and logs are retained beside each output.
The scripts are explicitly invoked experiments, not automatic suite tests.

Evidence is under `temp/snapshot-experiments-20261009/`: `single-l6.json`,
`coop-l6.json`, `two-player-spaced.json`, associated scripts/PASS records/logs,
`summary.json`, fresh connected-peer introspection and thermal readings.
Device setup used the
existing standard assets. The coop launcher command requires mission basename
`descent`, not display title `First Strike`. The successful connected fixture
admits the emulator peer before the host starts the game. Initial setup failures
were corrected before collecting measurement matrices.

## Validation

- Android ARM64 D1/D2 APK built successfully; no new compiler warnings
- Windows D1/D2 builds passed
- D1: upstream compatibility, coop save format and rewind policy tests passed
- D2: those tests plus Guide-Bot route decision and certifier tests passed
- Scoped C/C++, PowerShell and Markdown formatting passed
- All 60 S21 capture/worker/round-trip comparisons passed
- Both isolated diagnostic games were stopped after collection; the S21 was
  returned to Home and its original stay-awake setting (0) restored
- Temporary device launch scripts were removed; isolated test apps and local
  evidence remain available for reproduction
