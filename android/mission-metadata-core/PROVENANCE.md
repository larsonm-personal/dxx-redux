# Mission provenance

Native whole-mission reports and regression projections include `provenance`
The launcher metadata viewer opens the same evidence through its Provenance button

- `credits`: normalized field, original value, and source paths, merging only identical field/value pairs
- `date_estimate`: nullable year, basis, confidence, and optional archive file year range
- `file_dates`: original archive entry modification dates for the descriptor and containers/loose levels used by this mission
- `notes`: interpretation limits

Fields include author, nickname, real_name, email, website, version/revision, editor,
build_time, date, release_date, finished_date, and started_date. Values are declarations,
not verified identities or historical facts. Conflicting spellings and credits remain separate

The native collector reads the selected external descriptor, matching text file,
and AUT/descriptor files in the mission HOGs that actually supply levels. The external
descriptor is read independently because a HOG can shadow it after loading. The
shared bounded HOG catalog also reads embedded documents directly, including those
hidden by external files. Android staging retains the matching descriptor text file
Files larger than 64 KiB are skipped and distinct credit records are limited to 128
Free-form narrative, inferred identities, and web lookups are not parsed

Only an unambiguous four-digit year in an explicit release/completion declaration
is interpreted as a declared year (medium confidence). Otherwise, agreement among
relevant archive modification years yields a low-confidence vintage. Conflicting
years produce an unknown estimate, preserving the range. Dates before 1995 or
after the current UTC year are excluded, but retained as evidence. Short date strings
such as 04/05/98 and generic `date` declarations remain verbatim, without reinterpretation

Archive adapters supply original timestamps before staging. ZIP dates retain their
stored calendar date; 7z/RAR adapters use UTC when the archive API supplies an instant
`ArchiveEntryDates` supplies the same ZIP/7z date reader to Android archive adapters
and the host Kotlin CLI. PowerShell only orchestrates requests and selects staged paths;
it contains no timestamp parsing or date estimation. The native collector remains the
single implementation of credit parsing and vintage estimation
Unsupported date sources, loose imports, CD extraction, and nested archives without
an original entry-date mapping have no archive date evidence; extraction/download
filesystem times are never substituted. D1/D2 HOG members have no timestamps

The survey covered 106 top-level ZIPs: 87 had engine files within one calendar year,
while some spanned decades or contained pre-Descent dates. Representative checks:
Rogue (1998 modification evidence), Chasm (1997 release declaration), Vignettes
(2009 completion declaration), Chronolos (1999 HOG and 2016 external descriptor),
and Castaway Redux (2025/2026 modifications)

Validation: `android/tests/test_mission_provenance.ps1` exercises the native worker
against Rogue, including conflicting, invalid, declared, and absent dates in reused
worker requests. `MissionProvenanceTest` uses generated ZIP fixtures to check Android
staging, persisted extraction, regression projection, and viewer text. The reusable
`run_mission_zip_batch.ps1 -Pattern ROGUE.zip,chron10b.zip -MetadataOnly` runner covers
real Android import and analysis; the provenance objects match host-generated reports
