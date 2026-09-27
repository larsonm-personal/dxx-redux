# Unattended regression coverage

From the repository root, run `pwsh -File android/run_all_tests.ps1` and choose
`1`, or pass `-SampleSeed 254` for an unattended, reproducible selection.

The default suite runs fixed integration owners, inexpensive host checks, the
complete headless input-demo corpus with graphics canaries, and rotating
scenario families. Physical routing has one owner with fixed canaries and one
additional case per mission family. The default seed advances daily in UTC.
Reports identify the seed and every omitted scenario; omissions are not passes.

Normal runs also select full graphics coverage (including the two-pass fallback
probe) on seeds divisible by 20, and the 90-second multiplayer soak on seeds
whose remainder is 10. Each has a 5% cadence across consecutive seeds; they do
not coincide. Explicit extended flags still force those variants. CD extraction
sampling uses the same suite seed, so an unchanged commit does not freeze it.
The seed advances daily, not per invocation: repeat runs with the same seed
deliberately select the same tests. Missing fixtures still prevent execution.

Of the 190 coverage-policy entries, 93 are fixed owners, 89 rotate (13 chosen
per normal run), one is the rare graphics probe, and seven are manual tests or
utilities. All automated policy entries are eligible during normal runs.
The 83 owned physical route cases separately select seven fixed canaries plus
15 rotating cases. Consecutive daily seeds cover all scenario families within
16 days and all physical cases within 12; infrequent runs can take longer.

- `-Filter 'test_name*'` runs matching tests directly, including owned route cases
- `-FullSuite` (menu `A`) runs every retained unattended scenario and route case
- `-FullRouteCorpus` expands only the physical route owner
- `-FullExtracts`, `-ExtendedGraphics`, and `-ExtendedMultiplayer` independently
  expand extraction variants, graphics replays, and the multiplayer soak
- `-Target45Minutes` retains the separate resumable runtime sampling profile

The fixed core has eight integration owners: full headless demos (with graphics
canaries), full JVM tests, full native tests, the sampled physical route owner,
and four emulator workflows (launch-to-automap, SDK lifecycle, save/load, and
matchmaking multiplayer). The other 85 fixed entries each took at most 30 seconds
in the reviewed run. Fixed coverage totaled about 26 minutes of recorded test
execution; builds, provisioning, rotating scenarios and rare variants add to it.
This is a historical estimate, not a newly measured end-to-end runtime.

Specialist parity, objective UI, audio/render preferences, mission ZIP importing,
and external disc/SAF paths rotate. One storage/import case runs per normal seed.

## Adding or maintaining coverage

Extend an existing integration owner when it already exercises the same flow.
Keep distinct behavior assertions, but share setup and avoid replaying the same
demo or rerunning a JVM subset already covered by the full unit-test owner.
Diagnostic probes and comparisons between two checked-in generated snapshots
do not belong in the unattended regression catalog.

Top-level tests need an explicit entry in
`../helpers/test_suite_coverage.ps1`. Physical route cases belong in
`guidebot_route_regression_cases.ps1` and declare their support owner. Catalog
validation checks ownership, classification, fixed coverage, and complete
rotation. New tests must not silently expand the everyday suite.

Device tests run serially. Finish tests and formatters before starting Gradle:
the repository retention guard deliberately rejects overlapping work. Native
route runs release successful cases' extracted payloads and copied binaries;
their logs, result JSON, manifests, and settings remain for review. Failed
cases retain their payloads as well.

## Read-only catalog and portability accounting

```powershell
$catalog = & ./android/run_all_tests.ps1 -ListTests | ConvertFrom-Json
$catalog.tests | Group-Object requires | Select-Object Name, Count
$catalog.support | Select-Object name, type, owner
```

`-ListTests` exports the actual master discovery catalog as JSON before execution
filters, sampling, fixture checks or infrastructure probes. It creates no report
directory, prunes no artifacts and starts no test/device/build. `-HostOnly`,
`-Filter` and manual-selection flags do not narrow this inventory. Add
`-ExtendedGraphics` to list full graphics replay variants instead of canaries.
`-FullSuite` selects exhaustive route-owner timeout policy; the support table
still lists all discovered owned scripts either way.

Each top-level entry records its checkout-relative path, declared infrastructure,
manual flag and effective timeout. `none` means no master-managed device/server
infrastructure; it does not promise absence of game assets, native tools, displays,
or other per-test requirements. The support table connects PS1/JSONC children to
their integration owner. Tests internal to Python, JVM or CTest owners are covered
by those owners' own reports, not expanded into master entries.

Use this inventory alongside execution reports when accounting for Windows/Linux
parity. A catalog entry is not a pass, and a support owner passing a sampled run
is not proof that every child ran. Record missing media, unsupported capabilities
and unexecuted cases explicitly. `test_run_all_tests_catalog.ps1` checks discovery
completeness, owners, replay variants and read-only report behavior in tooling CI.

Host scheduling includes replay path/build-guard tests, repository artifact policy,
mission ZIP publication/recovery policy, Windows Job Object lifetime and Windows
PowerShell 5.1 compatibility. These checks do not require an emulator. On Linux,
the two Windows-specific contracts return explicit SKIP/exit 2; the PowerShell
compatibility test first exercises the portable helper assertions on the current
runtime. Linux descendant lifetime is exercised by `test_headless_process_pool`.

The `test_fingerprint_*` family is host-side. Its native enumeration and threshold
checks use `android/tests/build`; publication tests use local synthetic executables
and archives without AcoustID requests. CD, mission-ZIP and music-pack fingerprint
commands resolve Windows/Linux executable names in that shared build tree and
reuse its cached CMake. Native enumeration/matcher checks have 30-second child
limits; publication, enumeration and threshold fixture runs have retention and
producer locks, with temporary data removed in `finally`.

Extraction planning and policy tests also run without a device: CD workflow stage
selection, CD/GOG batch checks, spec generation, provenance, publication, regression
workflow contracts and mocked launcher preflight. Their isolated fixture directories
use retained `run_` generations and producer locks, with cleanup in `finally`.
These tests do not replace actual on-device extraction or proprietary-media runs.

`test_code_quality_files.ps1` also exercises the real formatting entry point in an
isolated fixture with recording tool stubs. It verifies that space-separated paths
all reach the tools, hidden files remain in scope, missing/empty explicit scopes
stop before any tool runs, and intentional unscoped invocation remains available.
The test runs in tooling smoke without requiring formatter installations.

D2X-XL sound contracts and dependency verification run as host tests on Windows
and Linux. `test_d2xxl_tga_pixels` exercises the production TGA decoder on both
hosts, including pixel depth, image origin, alpha and supertransparency masks.
`test_d2xxl_tga_layout` additionally checks System.Drawing bitmaps and archive
publication on Windows; it reports an explicit skip on Linux. Full texture-pack
conversion remains Windows-dependent. Both texture fixtures and dependency
verification use retained, locked run directories and remove payloads on exit

## Compact host execution evidence

The master runner updates `execution_evidence_<os>_<architecture>.json` in its
report directory before and after each test. Filtered runs preserve the latest
observations for other tests even after their logs and Markdown reports age out.
A separate summary on each host records commit, dirty-worktree flag, test source
hash, arguments, runtime, declared infrastructure requirement, outcome and skip
reason. Diagnostic paths may refer to files already removed by retention

`RUNNING` means no completion was recorded, including after an interruption; it
is not a process-liveness check. `SKIP` and `NOT_RUN` are not passes. A newer skip
replaces an older pass, and a late completion from an older invocation cannot
replace a newer attempt. Owner PASS covers that owner's reported contract only;
consult its case-level report for omitted fixtures, sampled cases or child skips.
Commit/dirty/source fields do not fingerprint all dependencies or prove that the
current checkout matches the tested code

Each host summary keeps at most 1,024 latest test observations and 4 MiB, evicting
oldest observations with an explicit count. Writers serialize, publish atomically,
and clean interrupted publication scratch after a successful update. Corrupt
summaries are preserved and reported as errors; evidence write failure makes the
suite exit nonzero after infrastructure cleanup. `-ListTests` does not create or
update this evidence. These are local observations, not an automatic Windows/Linux
parity verdict; absent entries mean execution evidence is missing

`test_dependency_install` owns Linux shell transaction fixtures that require
`flock`, including Windows archive/wrapper simulations executed on Linux. It
reports a capability skip on other hosts; those simulations are not actual
Windows installer validation. Portable dependency/platform/ownership checks remain
separate tests on Windows and Linux. SDK fixtures exercise standalone retention
handoff, failed or partial installs, full-bootstrap deferral and Windows shell
re-entry. `test_sdk_writer_lock` runs on both hosts and checks writer exclusion,
contention, exit-code propagation, timeouts and PowerShell AVD argument forwarding
