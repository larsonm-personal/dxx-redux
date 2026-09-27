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

Discovery does not traverse SDKs or global caches outside the repository. Dependency/build trees outside scratch roots use their separate retention rules rather than generic recursive scratch discovery. Windows and Linux process checks protect deletion; other platforms support preview. Sizes reported are logical bytes; drive free-space change is the authoritative measure of reclaimed capacity

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
On Windows and Linux, Gradle build/assemble/bundle/native tasks trim `app/.cxx` at root-project
configuration, before AGP configures the next native hash. Native configurations
are separate families; fixed build directories are reused rather than rotated.
Producer native cleanup excludes its initiating process chain and Gradle clients
from the idle check, but still refuses cleanup during active native builds or
unrelated tests. It never kills processes. Other hosts retain their existing native
build behavior. Linux native trees may contain file symlinks whose resolved targets
remain inside the same CMake build candidate; cleanup removes these links without
following them. Directory links and links outside the candidate remain protected.

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
Before deleting a failed replay sandbox, the runner preserves native text logs,
configuration, any result and structured launch/exit details in a timestamped
failure directory beside `-ResultCopyPath`, or under the runner's `failures`
directory when no external result path was supplied. Copied executables and DLLs
are not archived. Producer retention keeps three prior failures per output; if
diagnostic archiving fails, the sandbox is retained instead. A nonzero engine
exit remains a failure even when the engine wrote a result
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

## Dependency installation workspaces

JDK, SDK command-line tools, NDK and CMake installers unpack into unique staging
on the destination filesystem, validate the replacement, and retain the previous
installation until publication succeeds. Small formatter installers use the same
publication helper and a shared workspace containing downloads and extraction.
Normal success, failure and handled termination remove these temporary files.
No invalid or partially downloaded tool is published as the installed version.

Bash installers require a default 4 GiB free reserve plus a tool-specific estimate
for download/extraction. `DXX_DEPENDENCY_MIN_FREE_GB` adjusts the reserve using a
positive whole GiB value. PowerShell output checks select the actual filesystem
mount, including separately mounted temporary/output directories on Linux.

Versioned dependency installations are registered after a successful bootstrap or
dependency update. `android/get_deps/clean-dependencies.ps1` previews retention;
`-Apply` removes superseded managed versions. The bootstrap and updater invoke
this automatically. `-RegisterRepository` records another checkout sharing the
same dependency root; `-RegisterCurrent` claims the configured installed tool
directories after their installers have succeeded. Register every sharing checkout
before retiring versions. Once a checkout is retired, remove its reference with
`-UnregisterRepository /absolute/path/to/old/checkout`; missing checkouts otherwise
protect their dependencies until explicitly unregistered. Linux checkout paths
remain case-sensitive. These managed installation directories contain only
replaceable tool files; keep personal files elsewhere.

The ownership registry and stable lock live in the dependency root. Configured
versions from every registered checkout, explicit environment/PATH overrides, active
processes, and retained CMake caches are protected. Concurrent installers defer retirement; a
configured replacement must be installed and registered before the previous
working version can be removed. Missing checkout configuration,
unreadable tool processes, nested repositories, changed identities, and unsafe
links prevent deletion. Unix file links inside a managed installation, such as a
venv interpreter pointing to system Python, are unlinked without following their
targets; directory links remain protected. Unregistered older installations are reported as unmanaged
and are not adopted based solely on their names. SDK package/image retention and
unowned legacy versions remain separate from this versioned-tool policy.

Retirement records are written before moving a superseded version to a deterministic
quarantine directory. If the cleaner is killed during deletion, the next apply run
resumes from that journal, including when the installation marker was already
removed. It does not accumulate new quarantine copies on each retry. Preview does
not delete files or change ownership records; a stable lock file may be created.

Linux JDK, NDK, CMake, SDK command-line tools, formatter binaries, ktlint, and DOSBox
shell installers serialize through `flock` before inspecting their cached install.
The default wait is 60 seconds; `DXX_DEPENDENCY_LOCK_TIMEOUT_SECONDS` accepts 0..3600.
Their fixed `.dxx-install-state/work` directory contains the download, extraction,
and replacement backup. The next participating installer in the same parent
directory recovers an interrupted replacement and removes abandoned staging, even
when its requested tool is already cached. If publication did not finish, recovery
restores the previous install. If publication finished, it removes the old backup.
Surviving download/extraction children retain the lock after the Bash parent dies.
The small control directory and lock inode remain stable; never delete the lock
while installers are running. Linked or unrecognized recovery state is rejected.

This recovery is separate from version retirement and repository scratch cleanup.
Windows shell installers retain ordinary failure rollback but do not yet use this
Linux lock/recovery path. The in-place cmakelang installer, single-file soundfont
download, bootstrap PowerShell download, and unowned legacy staging directories
still require separate recovery handling.
