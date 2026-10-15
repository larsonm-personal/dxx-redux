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

## Additional standalone CTest owners

`cmake:fluidsynth-contracts` runs the existing `fluid_render_contracts` test. Its
public bundled SoundFont is provisioned by the existing hash-verifying
`get_soundfont.sh` helper, separately timed as setup. It does not run audition or
retail-audio experiments. `cmake:etc2-limits` builds the existing
`test_etc2_limits` target and runs its CTest registration. The assertions need no
assets; the unchanged full project configure still fetches its normal ETC2/KTX
build dependencies. Both profiles reuse original CMake flags and assertions and
produce actual CTest leaf receipts. Their setup/download failures remain retryable.

Further public runtime profiles bind the existing public-save-integrity recipes:
`public-save` and `public-pickup` build both `test_upstream_compat` fixtures;
`public-secret-baseline` builds both headless metadata executables and requires
actual assets; `shareware-data` requires the prepared data instead of accepting
missing-data skips. `public-ordered-input` and `public-all-weapons` build the
capture, full graphics, and headless replay targets for both engines. Compile
commands are exported so the existing all-weapons source/build/binary provenance
checks remain intact, including its exact clean-HEAD requirement. These visual
profiles use Xvfb and explicit x11 video; original timeout and assertion defaults
remain unchanged. Retail pickup modes and secret-area baselines remain excluded
from their explicitly public profiles.

The `host-profile:public-matcen` owner invokes the existing controlled native station-production wrapper with both fixture executables and explicit verified shareware directories. Its historical master `emulator` category does not describe its actual prerequisites: no ADB or Android device is used. This profile makes no normal-player-route claim.

## Private base-retail profiles

Four `retail-powershell` profiles reuse the same lazy native build adapter. Each
requires the eleven-file private stage at `game_data/retail-ci/{d1,d2}` and is
admitted by the nightly policy only with fixture `retail-base`. Before building,
the adapter inventories that exact stage against the existing standard-game-data
hashes and checks that the target and control checkout pins agree. Missing files,
different editions, or invalid stages are retryable BLOCKED setup with no
assertion credit. There is no fallback to shareware, unverified local media, or a
download source. Inventory time is setup, not assertion time.

- `host-profile:retail-secret-baseline` builds both headless metadata targets and
  invokes `test_secret_area_baseline.ps1` with `-DataEdition retail -Game both`,
  explicit data/executable paths, an attempt-local output directory, and
  `-RequireAssets`. It never updates a checked-in baseline.
- `host-profile:retail-counterstrike-route` invokes the existing Counterstrike
  route owner with the D2 headless metadata target and the verified D2 stage.
  Its native dump retains the owner's 300-second timeout and original route
  assertions; the adapter does not override them.
- `host-profile:retail-metadata-headers` builds both metadata workers and invokes
  the existing malformed-header/recovery owner with `-NoBuild`, retail data,
  both games, and explicit input/output paths.
- `host-profile:retail-guidebot-timeout` builds the D2 headless route target and
  runs the complete existing policy owner with `-NoBuild`, `-HeadlessExecutable`,
  and `-HogDir`. PASS includes its native one-second timeout status and 61-frame
  assertion. Exit 2 from the owner remains retryable BLOCKED, even if its earlier
  calculation-only checks passed.

Receipts are aggregate assertions for these exact selections only. They do not
claim custom mission, audio-disc, Android, or other retail runtime coverage.
Build failures remain retryable and do not earn assertion credit. These profiles
do not fetch assets or publish logs; the private nightly lane owns staging,
metadata-only public reporting, and cleanup of licensed inputs and derived files.
The portable `test_retail_ci_assets.py` and `test_nightly_retail.py` registrations
exercise synthetic/mock controls without licensed assets. Their passing results
do not establish that any retail owner has run successfully.
