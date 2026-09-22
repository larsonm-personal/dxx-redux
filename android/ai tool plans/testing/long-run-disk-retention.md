# Long-run disk retention

Date: 2026-09-21

## Findings

- Producer retention recognizes second-resolution timestamps but replay output
  names include milliseconds. The leftover milliseconds split one producer into
  separate families, defeating rotation
- Arbitrarily named replay experiments are stable workspaces to the generic
  retention helper, so calls before those runs do not remove earlier experiments
- Generation count alone does not bound bytes. Detailed object traces and copied
  executables/assets make a few generations expensive
- Replay capture has no free-space reserve during execution. Post-capture
  compression temporarily needs both the original and compressed files
- Workspace cleanup inventories processes and invokes Git for every deletion,
  including hundreds of tiny loose files. Tree scans and deletion have only
  terminal progress UI, with little visible output in redirected sessions

The workspace currently has about 244 GiB free after manual cleanup; historical
large traces are gone. Current footprint measurements are not evidence of their
former sizes

## Implementation and validation

1. Fix millisecond timestamp grouping; allow producers to explicitly own a
   directory prefix and cap retained bytes as well as generation count. Protect
   Git-visible work, links, nested repositories, active leases and current output
2. Use bounded retention for paired D1 replay experiments, including descriptive
   names in the owned prefix. Keep captures compressed and retain failure evidence
3. Add a shared replay free-space reserve before staging/launch and during replay;
   stop the owned engine and report incomplete evidence before exhausting space
4. Improve cleaner scan/deletion progress and reduce repeated expensive work
   without removing the last-moment Git/process/path checks
5. Exercise synthetic retention, safety and low-space failures, run scoped quality
   checks, and resume a native/imported replay comparison with the new controls

Do not globally clear temp while tests are running. Recorded demos and source
assets are never retention candidates. Long-lived evidence belongs outside temp

## Results

- Fixed millisecond grouping; paired runs explicitly own `d1_replay_parity_`,
  retaining two prior runs with an 8 GiB history ceiling. Held producer leases,
  Git-visible work and third-party workspaces override reclamation
- Every shared retention caller now checks a 4 GiB output reserve after trimming;
  replay checks it during execution as well. Compression verifies its lossless
  output before unlinking the original. Exceptions now retire the replay sandbox
  unless explicitly retained with `-KeepSandbox`
- Producer retention avoids unrelated collection scans and quadratic Git-path
  comparisons. General cleanup retains every per-candidate safety check and emits
  elapsed-time progress in redirected output
- Measured five full CIM inventories at 1.07 seconds and five scoped Git checks
  at 0.24 seconds. With hundreds of tiny files, these checks explain minutes of
  tail latency. A real full cleanup preview completed in about 26 seconds with
  671 candidates totaling only 330 KiB after earlier manual cleanup
- Synthetic cleanup, byte/count/millisecond retention, active lease, protected
  Git work, disk reserve and lossless-compression tests pass. No game code changes
  are required for these storage protections
- A real one-second replay timeout stopped its engine and removed the sandbox.
  A retention preview with a one-byte budget protected the live Python producer's
  lease. The first complete level-14 paired run used 439.6 MiB of compressed
  evidence, with all three captures successful and native repeatability exact
- Default `-TraceState` and determinism-matrix traces now also write gzip directly.
  Comparison phases print progress; future manifests archive the storage helpers
  along with their calling harness. Unix producers take an explicit file lock;
  runtime validation in this phase is on Windows
