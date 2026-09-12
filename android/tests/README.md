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
