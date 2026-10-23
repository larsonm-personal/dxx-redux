# Pull request checks

`Engine smoke` runs on every PR and can also be dispatched manually. Two
independent Ubuntu 24.04 jobs build D1 and D2 in parallel. Each job has a
45-minute ceiling, including up to 15 minutes for dependency installation. Superseded runs cancel
automatically. There is no second copy triggered by pushing the PR branch

The normal CMake `all` target compiles and links the desktop game, headless
tools, metadata workers and native tests. OpenGL, SDL_mixer and UDP stay enabled.
Debug builds keep assertions active and use reduced debug information to limit
memory and disk use. No warm cache is required

Every registered CTest runs with dummy SDL audio/video drivers. This includes
asset-free engine integration tests (AI, physics, weapons, triggers and save
state), input-demo recording/replay component tests and D2's synthetic classic
trigger demo writer/reader round trip. Tests run serially within each game
because some fixtures share filenames. Each test has a default 120-second
timeout, the combined configure/build/test step has a 17-minute limit, and zero
discovered tests is an error. CTest logs/JUnit and configuration diagnostics are retained for three
days, including on failures. Compiler output stays in the Actions job log

This is Linux native coverage. It does not compile Android Kotlin/JNI-only
paths, exercise a GPU/display, run an emulator, package an APK, or validate other
desktop compilers. Keep the manual packaging workflows for those build paths

## Tooling checks

Keep the existing Linux and Windows tooling suites, including the Windows
PowerShell 5.1 release tests. They exercise download verification, extraction,
dependency installation, cleanup/process lifetime, release operations, replay
comparison and test-catalog/evidence handling. Catalog validation is only part
of their coverage; it does not claim to execute the catalog's game tests

The workflow now runs on PRs that change scripts, automation fixtures,
dependency/extraction infrastructure, workflows, or the asset-loader contracts
those tests inspect. Ordinary engine and launcher source edits no longer
trigger it. Manual dispatch remains available; duplicate push runs are removed

For branch protection, require the two `Linux d1/d2 build and tests` checks.
Keep path-filtered tooling checks optional, since an unrelated PR will not
start that workflow. This change does not modify repository protection settings

## Full demo replays

The `.dximdemo` corpus and the required retail game files are not tracked in
Git. A clean hosted checkout therefore cannot run the existing full-game
regression runner. The smoke job executes the available asset-free demo checks
and does not silently skip a promised gameplay replay

To add a full-world PR replay later, commit a small validated recording and
provide the matching, hash-verified redistributable media fixture. Another
option is a separate trusted/manual job with privately provisioned retail
media. Keep asset access out of ordinary fork PRs and retain the existing
recorded-state comparison; repeatability alone is not recording fidelity

## Local reproduction on Ubuntu 24.04

Install the packages listed in `engine-smoke.yml`, then run from the repo root:

```sh
./run-linux-build.sh --test
# One CI matrix job, using the same four parallel build jobs
./run-linux-build.sh --test --target d1 --jobs 4 --generator Ninja
```

CI and local tests share this runner and the registered CTest list. Test mode
uses `buildd1-test` and `buildd2-test`, `BUILD_TESTING=ON`, Debug with `-O0 -g1`,
and all normal native targets and features. It rejects non-Debug configurations
to preserve assertions. CMake builds and CTest receive the same configuration,
including for multi-configuration generators. CTest is resolved beside the
selected CMake first, then from PATH

Test builds do not overwrite normal host replay freshness stamps. Add
`--sanitizer address` for independent `buildd1-test-asan` and `buildd2-test-asan`
trees, or `--clean` to recreate only the selected trees. Build-only invocation
without `--test` retains its existing defaults. Reports are written to
`ctest-results.xml` inside each test build tree, alongside `Testing/Temporary/`
and the CMake diagnostics retained by CI

The workflow first runs the isolated CLI regression suite:

```sh
python3 android/tests/test_linux_build_cli.py
```

It executes the real shell runner against recording tool shims in temporary
fixture repositories, then cleans them up. It checks argument validation,
tool discovery, routing, configuration/build/test failures, diagnostics options,
and separation from ordinary builds without compiling games or downloading assets

For interrupted coding tasks or GitHub access failures, see the
[task recovery runbook](TASK_RECOVERY.md).

## Extended three-player network category

The scheduled tooling sweep admits `test_lan_three_player_startup` in the
`three_player_network` category. It reuses the Android multi-device lane's APK
and public fixtures and provisions a third disposable emulator only when selected.
Its 900-second owner budget covers exactly four separate results: native D1/D2
healthy uneven-load starts and selected-loader process loss. Both surviving peers
must recover without silently starting; missing or skipped results never pass.
The wider packet-fault and actual mixed-version experiments remain opt-in.

Ordinary PR checks retain the two-player public smoke. To qualify just the new
category through the normal catalog, master runner and durable receipts, dispatch
`public-lan-smoke.yml` with campaign `lan-three-player`, or apply the explicit
`test-three-player-network` PR label. Reapply the label after a later commit to
request another focused run; ordinary synchronization events remain two-player.

For local reproduction, provision three disposable emulators on the same shared
network, with the third running as `emulator-5558` (or set
`DXX_THIRD_EMULATOR_SERIAL`). Prepare the verified public D1 and D2 fixtures, then:

```powershell
pwsh -NoProfile -File android/run_all_tests.ps1 -Filter test_lan_three_player_startup
```

The normal master preflights/provisions the pair and validates the third before
mutating any target. It does not stop a caller-owned third emulator. Evidence is
in `android/temp/lan-three-player-startup/`, including the four case receipts
and `three-player-summary.json`. CI retains it separately under `three-player/`.

## Setup failures and recovery

Before Android Gradle work, run `bash android/get_deps/check_build_prereqs.sh`.
It reports the dependency base, Java/SDK/NDK paths, pinned command-line build
marker and missing package metadata without creating directories or installing
anything. `dependency_base.txt` takes precedence over `DXX_DEPENDENCY_BASE`;
existing Java/Android environment overrides are reported and checked. A missing
SDK in a restored workspace is an environment prerequisite failure, not a Gradle
code failure. Run the named helpers, then repeat the check and the original build

CMake dependency fetches retain the manifest SHA-256 cache keys, lock, verified
cache admission and atomic rename. Transient connection/partial/timeout errors
get three attempts of at most 120 seconds, with 2/4 second backoff and a 400 second
lock wait. HTTP errors, TLS failures and checksum mismatches fail immediately.
Never disable TLS or change a pin to get past a failed download. Remove only the
reported corrupt cache entry after investigating it, then rerun configuration

Bootstrap file downloads use curl's transient HTTP/timeout retries (two retries,
30 second connection and 300 second attempt deadlines; at most 910 seconds).
The wget fallback requires GNU `timeout` and has the same outer 910 second limit.
Metadata requests have a 60 second deadline (65 including forced termination for
wget) and do not retry streamed stdout, avoiding concatenated partial responses.
SDK ZIP integrity is checked before extraction; a corrupt archive fails without
replacing the existing installation. Rerun the same installer to use its existing
staging/recovery protocol. ZIP CRC checks do not establish publisher identity;
the SDK command-line archive still relies on the pinned build URL over TLS

Apt uses native acquisition retries and 30 second HTTP/HTTPS stall timeouts.
Index refreshes fail on acquisition errors instead of silently reusing stale lists.
Native provisioning has a 15-minute step budget: a progressing 50 MB transfer at
70 KB/s can exceed 12 minutes without ever hitting an inactivity timeout. Job
budgets include this setup allowance; build and test step limits are unchanged.
Workflow step/job deadlines remain the overall provisioning limit; a package or
build error must still fail the job. Nightly SDK caches now include command-line
and platform tools, with a new cache generation; installer/package checks still
run on cache hits. Native dependency caches remain content-addressed and verified

Run `python3 android/tests/test_verified_dependency_download.py` for disposable
loopback HTTP tests of partial transfer recovery, bounded exhaustion, permanent
HTTP errors, hash rejection, cache reuse and curl transient HTTP recovery. Existing
installer fixtures cover corrupt SDK ZIP rollback and clean resumability

A GitHub run that fails at startup with zero jobs and an unexpected service error
has not executed repository setup. Record its run ID separately and rerun only
after checking platform status. Preserve branch/SHA, PR and diagnostic links at
working checkpoints; use `.github/TASK_RECOVERY.md` to restore a clean workspace
rather than repeatedly repairing an inaccessible one
