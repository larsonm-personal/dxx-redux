# Unattended regression coverage

From the repository root, run `pwsh -File android/run_all_tests.ps1` and choose
`1`, or pass `-SampleSeed 254` for an unattended, reproducible selection.

The default suite runs fixed integration owners, inexpensive host checks, the
complete headless input-demo corpus with graphics canaries, and rotating
scenario families. Physical routing has one owner with fixed canaries and one
additional case per mission family. The default seed advances daily in UTC.
Reports identify the seed and every omitted scenario; omissions are not passes.

- `-Filter 'test_name*'` runs matching tests directly, including owned route cases
- `-FullSuite` (menu `A`) runs every retained unattended scenario and route case
- `-FullRouteCorpus` expands only the physical route owner
- `-FullExtracts`, `-ExtendedGraphics`, and `-ExtendedMultiplayer` independently
  expand extraction variants, graphics replays, and the multiplayer soak
- `-Target45Minutes` retains the separate resumable runtime sampling profile

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
