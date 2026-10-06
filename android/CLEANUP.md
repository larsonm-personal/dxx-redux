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

Emulator data directories with `config.ini` may have custom names such as `avd`; they do not need an `.avd` suffix for stale `hardware-qemu.ini.lock` and `snapshot.lock.lock` cleanup. A held file lock or unknown directory lock protects its owning workspace, including when it contains Git-visible files. It does not preserve unrelated siblings or the entire enclosing scratch root

For interrupted formatting, preview this checkout's matching processes with
`pwsh android/helpers/stop-stale-formatters.ps1`; add `-Kill` to stop the matched
process trees. This is an explicit stop operation, not an age-based cleanup: it
can stop a currently running formatter. Linux relative invocations are resolved
against the process working directory. The formatter lock records repository,
PID and process start identity for Windows relative invocations. Cleanup rechecks
process identity before stopping, and excludes its own process and ancestors

`-Preview` and `-WhatIf` never delete; add `-Verbose` to list each candidate. `-AutoOnly` remains accepted by existing callers; cleanup is now always automatic. `-TemporaryOnly`, `-BuildsOnly` and `-PayloadsOnly` are mutually exclusive

Outside scratch roots, normal cleanup also removes old generated build/download artifacts (default `-ArtifactDays 30`) and superseded native/package generations. It preserves the newest generation of each recognized build family; `-KeepBuildGenerations` and `-BuildGraceHours` adjust that retention. Explicit temporary cleanup takes precedence for builds located inside a scratch root. Use `-BuildsOnly` to apply build retention without clearing scratch files

Normal and `-BuildsOnly` cleanup also discard `server/target/{debug,release}/incremental` without an age threshold. Cargo recreates this compiler state; this cache rule does not remove linked binaries or dependency artifacts. Normal age-based build cleanup still applies to those outputs. Scoped native producer cleanup does not touch Cargo caches, and active Cargo/rustc processes and held locks still prevent deletion

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
DOSBox-X packages participate using their configured `DOSBOX_DIR_NAME` (a
`dosbox-` prefixed directory), including the Windows extraction package cached
on Linux. Registration and retirement do not imply native Linux execution.
The native Linux bounded-extraction Python runtime also participates under its
configured `PYTHON_BOUNDED_LINUX_DIR_NAME`. Its ownership marker stays outside
the pinned `python/` tree, and runtime probes disable bytecode writes.
User-local `powershell-VERSION` installations participate using `POWERSHELL_VERSION`.
Linux `pwsh` and `pwsh-preview` command symlinks on PATH protect their targets,
even when the version directory itself is not on PATH. System deb/rpm PowerShell
installations remain owned by the package manager and are outside this registry.

The ownership registry and stable lock live in the dependency root. Configured
versions from every registered checkout, explicit environment/PATH overrides, active
processes, and retained CMake caches (including `buildd1-asan` and
`buildd2-asan`) are protected. Concurrent installers defer retirement; a
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

Linux JDK, NDK, CMake, SDK command-line tools, formatter binaries, cmakelang, ktlint, and DOSBox
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

Cmakelang creates its Python environment at the final path because console
launchers embed that path. Its transaction records when replacement starts and
commits only after package-version and executable validation. Recovery discards
an uncommitted partial environment and restores the previous installation, or
removes a failed first installation. A committed installation survives recovery,
which removes its old backup. Recovery can itself be interrupted and retried.
While this final-path transaction is pending, an atomic format marker makes older
archive-only installer scripts refuse recovery rather than discard the backup.
A current installer recovers it and restores the ordinary format marker, so
completed transactions do not prevent older checkouts from using cached tools.

This recovery is separate from version retirement and repository scratch cleanup.
Windows shell installers retain ordinary failure rollback but do not yet use this
Linux lock/recovery path. The soundfont downloader also uses the Linux journal,
located beside the assets directory so recovery files are never packaged in the
APK. Its same-filesystem workspace preserves the existing asset until a verified
replacement is published. Recovery removes abandoned downloads before checking
the cached asset, including after a kill immediately following publication.
Windows soundfont downloads retain their existing temporary-file failure cleanup.
PowerShell bootstrap downloads use the same locked workspace under the dependency
base, including deb/rpm payloads. A retry clears interrupted downloads before
checking an already-installed PowerShell. User-local tarballs are staged and
version-checked before publication; interrupted replacement restores the previous
tree or keeps the committed replacement. A validated local tree can repair its
command link without another download. Package database recovery remains owned by
the OS package manager; the helper does not automatically repair interrupted
system package transactions. Unowned legacy staging still needs separate handling.

Native GuideBot simulation batches discover hash-verified retail D1/D2 assets
through the shared game-data helper. D1-in-D2 runs copy the required files into a
lowercase `base-data` directory under their retained output generation; disk
preflight includes the copy size. Explicit caller-supplied data directories remain
caller-owned. Automatic base-data copies are removed on both success and failure,
and partially copied stages are removed if validation fails. A producer lock
protects each active batch and route-test owner from retention cleanup. After a
forced producer termination, abandoned data remains bounded by generation retention.
Result logs/JSON and mission staging needed by downstream tests remain available.
Route-test children use the shared supervised process pool for timeout and parent
termination cleanup.

SDK platform finalization, emulator/image provisioning and shell AVD creation
share the Linux command-line installer lock with `get_sdk.sh`. This keeps a
command-line tool replacement from racing a running SDK operation and recovers
an interrupted replacement before locating its executable. Package provisioning
checks the disk reserve before downloads. Emulator/image cache admission requires
package metadata and native payload files; incomplete directories are retried.
These checks do not yet retire obsolete SDK packages or clean sdkmanager-owned
partial downloads. Package retirement must account for registered checkout pins
and AVD image references before removing anything.

To inspect SDK package references without changing the SDK, run:

```powershell
./android/get_deps/inspect-sdk-retention.ps1 | ConvertTo-Json -Depth 8
```

The inventory reads package metadata, all registered checkout pins sharing the
dependency base, and AVD configs under the default and environment-selected roots.
Moved AVDs referenced by `.ini` descriptors are included. `-AvdRoots` can supply
explicit diagnostic roots; omitting a root does not establish that its images are
unused. Missing checkout/AVD configs or linked reference paths stop discovery.
Malformed or inconsistent package metadata is reported as `Unknown`.

`Unreferenced` means absent from those two reference sources only. It is not
eligibility for deletion: package ownership, running processes, retained build
caches, other users' AVDs and a fresh check before removal remain necessary. The
report deliberately sets `RetirementReady=false`; this command never deletes or
registers packages. AVD environment locations follow the
[Android tools documentation](https://developer.android.com/tools/variables);
eventual removal should use the pinned
[sdkmanager uninstall interface](https://developer.android.com/tools/sdkmanager).

Managed SDK package cleanup is available separately:

```powershell
./android/get_deps/clean-sdk-packages.ps1 -RegisterCurrent
./android/get_deps/clean-sdk-packages.ps1
./android/get_deps/clean-sdk-packages.ps1 -Apply
```

Bootstrap registers current packages after successful provisioning and runs apply.
Only versioned build-tools, platforms, CMake, NDK and system-image packages with
matching ownership markers and metadata receipts can be retired. Unregistered
legacy packages remain protected. Current checkout pins, registered AVD roots,
active SDK tools, unreadable tool processes, linked package content and retained
CMake caches prevent retirement; the configured replacement must be installed
and registered. Registration persists visible AVD roots so later environment
changes do not discard those references. Sharing users must register their
checkouts and AVD roots before versions are retired.

The cleaner holds the command-line installer lock and rechecks references before
calling `sdkmanager --uninstall` through the supervised process pool. A fixed
ownership journal records removal intent, native directory identity and a bounded
SHA-256 manifest before uninstall begins. Already-removed packages are reconciled
on the next apply. Partial removals can recover even after package.xml or the
ownership marker disappears: every surviving entry must be an unchanged original
file or directory. Added files, changed bytes, replaced directories, links and
renewed references prevent deletion. References are checked again after hashing.

Verified partial payloads move into a deterministic quarantine for deletion.
Recovery survives termination after the move or partway through deleting it,
without creating additional copies. A temporary schema version makes older
cleaners reject pending quarantine work. Successful recovery restores the normal
schema and removes its manifest. Preview validates recovery evidence but does not
move or remove payloads. Journals predating manifests remain protected if ownership
evidence is missing. Windows shell provisioning
still needs the same lock discipline as Linux; external SDK managers do not
participate in this repository's lock.

### DOS MIDI parity

`tests/test_dos_midi_parity.ps1` reuses `android/tests/build` and writes small
MIDI/JSON diagnostics and process logs under `android/temp/dos_midi_parity/run_<id>`.
It retains three prior repository runs, holds `producer.lock` while active,
and checks the output/build volumes for a 4 GiB reserve. All child stages have
supervised timeouts. `-OutputDirectory` changes the parent of the unique runs;
external output directories remain outside repository cleanup ownership.

### Native GuideBot navigation

The Original and live navigation runners discover pinned D2 data through the shared
host helpers. `-HogDir` can select an explicit source directory; it must match the
same pinned data. Each run stages lowercase copies under its retained `run_` directory
and removes them, extracted missions and player data in `finally`, including native
failure and timeout. Source game data is never used as writable scratch storage.
Each native invocation has a 90-second timeout (`-ProcessTimeoutSeconds` overrides
it for diagnostics), with process-tree supervision and retained output logs.
The existing producer lock and three-prior-run policy also bound interrupted runs.

The Windows Job Object lifetime fixture now uses locked, retained
`android/temp/process_lifetime/run_<id>` directories. Normal and failed completion
remove that run's fixture data after its owned processes are stopped. Interrupted
runs remain subject to the three-prior-generation retention policy.

Master test execution evidence is stored separately from retained logs in
`temp/test_reports/execution_evidence_<os>_<architecture>.json` (or the selected
report directory). Each summary is bounded to 1,024 latest observations and 4 MiB;
oldest entries are evicted with a recorded count. It survives normal timestamped
report retention, but explicit removal of its containing workspace removes it.
The adjacent `.lock` is a reusable writer lock, not a live-process marker. A
successful publication also removes matching temporary/backup files left by a
killed evidence writer. See `tests/README.md` for interpretation and limitations

Standalone Linux `finalize.sh` and `get_emulator.sh` now run managed SDK package
retention after successful provisioning, including a fully cached invocation.
The producer finishes recovery and releases its installer lock before the cleaner
acquires that same lock. Missing requested package metadata, provisioning failure
or retention failure makes the command fail. Full bootstrap defers this operation
until all producers finish. Windows shell SDK producers now re-enter through
`get_deps/helpers/invoke_sdk_writer.ps1`, which holds the same exclusive lock,
supervises child lifetime and runs standalone package retention after release.
The PowerShell AVD creator uses this wrapper on both hosts. Writer waits and
execution are bounded; failed or timed-out producers never trigger retirement.
Linux shell installers retain their inherited flock and transaction recovery.
Actual Windows execution of the new wrapper remains pending validation

SDK command-line tool transactions use the same recovery journal under either the
Linux shell lock or the shared SDK writer wrapper. A supervised shell operation
must target that wrapper's exact `cmdline-tools/latest` destination. On the next
invocation, an interrupted replacement restores its previous tools when needed
and removes the abandoned transaction workspace. The PowerShell AVD producer runs
this recovery before resolving `avdmanager`. This does not add recovery for
sdkmanager's internal partial downloads or for other Windows dependency installers
