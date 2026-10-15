# Allowlisted Linux host-suite adapters

`run_host_test_suites.py` is a control-checkout adapter. It executes only suites listed in its
adjacent `host_test_suites.json`, against a separately selected target checkout. Plans select
an ID, never a shell command. It does not replace `run_all_tests.ps1` or its PS1/JSONC ownership.

```sh
python3 android/tests/run_host_test_suites.py --list-json
python3 android/tests/run_host_test_suites.py \
  --suite python:test_inno_capability_docs.py --repo /path/to/target \
  --build-root /tmp/host-build-cache --output /tmp/run-123/inno.json
```

Use a distinct output filename/directory for every attempt. Logs and case results live beside
that output. Each attempt supplies its own TEMP/TMPDIR/TMP directory so cached-build fixtures
cannot collide when process IDs are reused. Build directories can be reused for the same target checkout. A selected commit
must be checked out and verified by the caller; this adapter never checks out or modifies Git
history. Full checkouts are required for source-contract tests that inspect other directories.

## Supported owners

- `python:<filename>` runs one allowlisted existing unittest module, or one explicitly listed
  no-argument assertion script. Python tests retain their original assertions. No arbitrary
  repository-wide discovery or script guessing is performed.
- `extractor:full` configures the original standalone CMake entry point and builds/runs its
  complete CTest inventory. Its hash-verified dependency downloads require network or a cache.
  This includes the existing PCM/fingerprint, audio metadata, MIDI/synthesis, and archive tests.
- `extractor:light` is optional and requires the target commit's dependency-light option. It
  never substitutes for full extraction/audio coverage. Unsupported commits are explicitly blocked.
- `engine:ctest-d1` and `engine:ctest-d2` lazily configure/build the existing native entry points
  with the assertion-preserving Debug flags used by `run-linux-build.sh --test`. Native CTests
  run serially with dummy SDL audio/video because fixtures share filenames.
- `audio:strict-fingerprint` builds the existing `fingerprint_match` target and exercises the
  existing strict-admission assertions through the portable `--matcher`/`--known-discs` CLI.
  Its fixture is the maintained fingerprint in `known_albums.jsonc`, not retail audio. Target
  commits predating the portable CLI are blocked rather than silently invoking a Windows path.

Host packages match `engine-smoke.yml`: build-essential, CMake, Ninja, PhysFS, SDL1.2, SDL_mixer1.2,
PNG, GLEW, zlib and Mesa/Xvfb packages. Selected compiler tests additionally need Clang/Clang++.
Python spectrum tests need numpy, scipy and imageio-ffmpeg. Several existing Python tests spawn
PowerShell; those entries declare `pwsh` and must only run where that execution is allowed.
The adapter installs nothing globally. Cached full extractor dependencies are beneath
`<build-root>/extractor-full/build/dxx_dependency_cache`; native trees are `engine-d1`/`engine-d2`.

## Results and finite sweeps

Schema 1 results contain `suite`, `status`, `reason`, `exit_code`, `elapsed_seconds`,
`setup_seconds`, `elapsed_wall_seconds`, `coverage_scope`, `coverage_complete`, `cases`,
`exclusions`, `phase`, `attempted_tests`, `retryable`, `blocker_kind`, and `log`. Exit codes are 0 (PASS), 1 (FAIL), and 2 (BLOCKED).

**Aggregate PASS means the owner executed without failures; it does not erase excluded leaves.**
Only case records with `status: PASS` earn leaf coverage. Explicit CTest skips, skipped unittest
cases, and known internally skipped real-media checks remain in `exclusions` and make
`coverage_complete` false. An owner with public passes and missing licensed media can therefore
finish its attempt without endlessly rerunning those public leaves. The scheduler should retry
excluded leaves only after their prerequisites change, and report the uncovered media ceiling.
A suite with no executed tests is BLOCKED, never a pass. Setup failures carry
`attempted_tests: false` and cannot earn assertion coverage. Missing tools/download failures,
setup timeouts and cancellation are retryable; immutable-target capability omissions are not.
The scheduler must not retire a retryable BLOCKED attempt as a completed test.

HFS/STI2 internal media skips are conservatively recorded as incomplete CTest registrations;
their synthetic subchecks are not fabricated as separately identified leaf records. The light
profile normally has 22 explicit registration skips plus these two partially covered registrations
(eight internal media checks). Missing GOG/SOW/StuffIt fixtures are likewise visible. Passing
public fixtures never proves retail compatibility.

Configure/build time is recorded separately from test time for a 45-minute **test** budget.
The adapter's wall-clock timeout still bounds both, and timeout/cancellation terminates its
process group so compiler/test descendants cannot continue unobserved. Cached CTest results
are never reused as new execution evidence.

`owned_elsewhere` records children already covered by native CTest owners (for example HMP
playback needs a compiled exporter and must not be run by bare unittest discovery).
`unsupported_entrypoints` retains concrete constraints for remaining standalone scripts.
Android instrumentation, physical hardware checks, Windows GUI checks, device audio probes,
interactive tools, and exact licensed mission media remain distinct from this Linux owner.

## Public native and scoped PowerShell profiles

Six additional `host-profile:` selections invoke existing assertion owners with
reviewed, explicit arguments. Plans cannot inject executable paths or commands.
`metadata-d1`, `metadata-d2`, and `metadata-imported-d1` build the corresponding
native metadata worker; the imported profile also checks retirement/recovery on
D1/D2 content-profile changes. `metadata-headers` runs malformed-header recovery
for both engines. `public-flight` builds both full games and executes their
existing public-flight replay smoke under `xvfb-run` (Mesa/Xvfb/xauth required).
All use verified public shareware prepared by the existing setup script; its
pinned dependency requirements and downloads are setup, separately timed from
assertions. Default retail data is never substituted. Build failures and public
fixture preparation failures remain retryable and earn no test coverage.

`benchmark-policy` runs only the existing `-PolicySelfTest` branch. It does not
measure native performance, validate corpus digests, or update history. Those
unexecuted portions are explicit exclusions. Public native receipts similarly
exclude retail/custom-asset variants. These owners currently provide aggregate
assertion receipts, not invented internal leaf counts. Optional variants are not
silently included. Local qualification covers adapter argument/phase contracts;
actual PowerShell/native profile execution requires the hosted lane.
