# Workspace cleanup

Run from the repository root. Cleanup is unattended: eligible artifacts are removed without confirmation

```powershell
.\android\clean-workspace.ps1 -Preview
.\android\clean-workspace.ps1
# Fast emergency cleanup: all temporary workspace files, without build scans
.\android\clean-workspace.ps1 -TemporaryOnly
```

The default removes every ignored file type inside scratch directories, including fresh files. This includes custom-named runs, copied engines, extracted assets, partial/failed runs, reports, JSON, logs, scripts, source backups and hidden files. Scratch roots are discovered at arbitrary repository depths: `temp`, `temp_*`, `temp-*`, `tmp`, `tmp_*`, `tmp-*`, `.tmp`, `.temp`, `test-results`, `test-reports`, `regression-results` and `regression-output`. There is no timestamp naming requirement or fixed runner allowlist. Ignored loose `.tmp`, `.temp` and `.log` files outside these roots are also eligible

Scratch files are disposable. Put evidence or work that must survive cleanup in a non-temporary, Git-visible location. `-TempDays N` explicitly retains temporary files newer than N days, using both creation and modification times; the default is zero. Reports in temp are not retained by the default cleanup

The cleaner protects tracked files (even forcibly added ignored files), untracked non-ignored work, original `game_data`, emulator source assets, recorded regression demos, fixtures, nested repositories, links/junctions and held locks. Protected children do not strand disposable siblings. It never follows links or deletes outside the resolved Git workspace. An ignored filename alone does not make arbitrary source/assets disposable

Active build/test/formatter/emulator jobs block deletion. The script waits up to 60 seconds for them automatically (`-BusyWaitSeconds` controls this), then exits if they remain active. It does not terminate jobs or ask for permission. Known stale emulator marker directories are removable only through this idle-checked temporary cleanup; held file locks remain protected. It rechecks process activity, Git state, containment, links, modification/creation times, file count and bytes before deletion. Newly changed candidates are preserved. Locked/unreadable artifacts are reported and other eligible artifacts can still be removed. Run again once jobs finish or locks are released

`-Preview` and `-WhatIf` never delete; add `-Verbose` to list each candidate. `-AutoOnly` remains accepted by existing callers; cleanup is now always automatic. `-TemporaryOnly`, `-BuildsOnly` and `-PayloadsOnly` are mutually exclusive

Outside scratch roots, normal cleanup also removes old generated build/download artifacts (default `-ArtifactDays 30`) and superseded native/package generations. It preserves the newest generation of each recognized build family; `-KeepBuildGenerations` and `-BuildGraceHours` adjust that retention. Explicit temporary cleanup takes precedence for builds located inside a scratch root. Use `-BuildsOnly` to apply build retention without clearing scratch files

```powershell
.\android\clean-workspace.ps1 -BuildsOnly
.\android\clean-workspace.ps1 -Preview -TempDays 7 -KeepBuildGenerations 2
```

`-PayloadsOnly` retains the older narrow report-preserving policy for timestamped `mission_zip_host_metadata` and `guidebot_simulation_regression` outputs. It removes reproducible `raw`/`stages` mission payloads with a configurable one-hour `-PayloadGraceHours` grace. Use `-TemporaryOnly` or the default for comprehensive reclamation across arbitrary run names

Discovery does not traverse SDKs or global caches outside the repository. Dependency/build trees outside scratch roots use their separate retention rules rather than generic recursive scratch discovery. Windows process checks are currently required for deletion; other platforms support preview. Sizes reported are logical bytes; drive free-space change is the authoritative measure of reclaimed capacity

Validate safely against a synthetic Git repository:

```powershell
.\android\tests\test_clean_workspace.ps1
```

## Producer startup retention

Producers trim their own output family to the newest three prior generations
before creating the next output, leaving at most four after a new generation is
created. No age threshold applies. Reusing an existing output does not create a
fourth generation. The current/planned output is excluded from prior history,
including when a wrapper and child runner share it.

All callers of `retain-recent-artifacts.ps1` also check a 4 GiB output-volume
reserve after retention, before producing new artifacts. This includes planned
directories that do not exist yet. `-MinimumFreeSpaceGB` adjusts that reserve

The shared `helpers/retain-recent-artifacts.ps1` accepts planned output paths.
Hooks cover metadata and guidebot batches, regression/test reports, demo matrix
outputs, warning logs, guidebot browser runs, and timestamped/deployment AAB copies.
On Windows, Gradle build/assemble/bundle/native tasks trim `app/.cxx` at root-project
configuration, before AGP configures the next native hash. Native configurations
are separate families; fixed build directories are reused rather than rotated.
Producer native cleanup excludes its initiating process chain and Gradle clients
from the idle check, but still refuses cleanup during active native builds or
unrelated tests. It never kills processes. Other hosts retain their existing native
build behavior; workspace native deletion currently requires Windows safety checks.

Manual cleanup still defaults to one newest generation. Producers keep three prior
outputs so running them offers more history without requiring cleanup flags.

Millisecond timestamps (`yyyyMMdd_HHmmss_fff`) now belong to the same family as
second-resolution timestamps. Producers can explicitly group descriptive directory
names with `-DirectoryPrefix` and impose `-MaxFamilyBytes` as well as `-Keep`.
These limits apply to prior history; current/planned output is excluded. Protected
files, nested repositories, build boundaries and held `.lock` files are preserved,
even when this prevents meeting the budget. Retention reports those exceptions

Paired D1 replay captures use the owned `d1_replay_parity_` prefix: at most two prior
runs and 8 GiB of prior history, including descriptively named experiments under
that prefix. `-RetainedHistoryGB` adjusts the byte budget. Custom paths outside the
prefix retain the generic timestamp policy. A held `producer.lock` protects a run
until both capture and comparison finish. Fixed-name files and unrelated scratch
families are not removed by this producer

Replay runners require 4 GiB free on output volumes before launch and check again
once per second during execution (`-MinimumFreeSpaceGB`). A reserve failure stops
the owned engine through the usual exception cleanup and reports an incomplete
capture; it never blesses truncated traces or starts global workspace cleanup.
Replay sandboxes are removed on exceptions and timeouts as well as success;
`-KeepSandbox` explicitly retains them for debugging. External requested result
and trace paths remain available, including incomplete captures
The reserve is a guardrail, not reserved disk allocation: other processes and a
single very large write can consume it between checks. Paired, default `-TraceState`
and determinism-matrix state traces are written directly as gzip. Explicit trace
paths keep their requested format. Post-capture RNG compression verifies the decoded bytes
before replacing the source, keeping the original if compression fails

Cleanup emits elapsed-time scan and deletion progress even when output is
redirected. Its final small-file phase can be slower than large-file removal:
every candidate still gets fresh process, Git, path and tree checks. Producer
retention avoids scanning unrelated collections and uses a set of protected paths
instead of comparing every candidate against every Git-visible file
