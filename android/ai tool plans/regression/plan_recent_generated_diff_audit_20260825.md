# Recent Generated Diff Audit Plan

## Goal

Determine why the current generated JSON and JSONC files appear in `git diff`,
whether their changes are semantic, and whether they are legitimate outputs of
recent regeneration commands.

## Plan

1. [Complete] Compare working-tree and `HEAD` encodings, byte sizes, and
   text diffs for each generated file.
2. [Complete] Compare parsed JSON values where the file format permits it.
3. [Complete] Trace each file to its writer and identify the command or encoding behavior
   responsible for the change.
4. [Complete] Report which changes are legitimate, which are accidental, and whether a
   code fix is needed to prevent recurrence.

## Findings

- `known_albums.jsonc` initially had the same Git blob hash as `HEAD`; its
  modified indication was stale. A real Windows PowerShell 5.1 regeneration
  then exposed runtime-dependent decimal rendering (`1.0000` versus `1`) in
  ambiguity comments. The publisher now formats numeric text invariantly,
  sorts album sources with explicit ordinal comparison, and skips exact output.
- `Descent - Levels of the World (USA)/extract_regression.jsonc` has the same
  191 expected filenames and test result as `HEAD`. Its generated timestamp,
  JSON formatting, and punctuation-sensitive filename order changed. The
  writer uses PowerShell-version-dependent `ConvertTo-Json` formatting and
  culture-sensitive `Sort-Object`, so this is generator churn rather than a
  regression-oracle update.
- `ulterior_v1.0.6b.json` is a real successful metadata expansion. The Android
  importer found `TEST/EXITD2V.MN2` plus D2X, DOS, and Rebirth variants of
  `ULTERIOR.MN2`; the prior host-produced file represented only two targets.
  The new file represents all four and passed the 11-step metadata run.
- The original multi-target mission output omitted each target's
  archive-relative `mission_path`, so the three Ulterior variants were not
  self-identifying. The fixed checked-in output now carries a stable path for
  every target.

## Verification

- Compared working-tree and `HEAD` Git blob hashes.
- Parsed the JSON/JSONC files and compared top-level values and inventories.
- Matched file timestamps to regression reports and mission batch artifacts.
- Confirmed the final Ulterior automation result passed 11 of 11 steps and
  published the checked-in metadata file.

## Fix Plan

1. [Complete] Make the album database publisher skip byte-identical output.
2. [Complete] Make extraction-spec ordering ordinal and JSON formatting independent of
   the PowerShell runtime.
3. [Complete] Add archive-relative mission identity to generated mission metadata.
4. [Complete] Extend focused integration tests for all three behaviors.
5. [Complete] Normalize the affected generated files, run focused tests and scoped code
   quality, and confirm repeated generation is clean.

## Fix Verification

- Regenerated the Levels of the World extraction spec and confirmed its SHA-256
  hash was unchanged and the file remained Git-clean.
- Regenerated `known_albums.jsonc` under PowerShell 7 and Windows PowerShell
  5.1, confirmed identical SHA-256 hashes, and confirmed the result matched
  `HEAD` byte-for-byte.
- Rebuilt and installed the APK, then regenerated Ulterior metadata on the
  emulator. The 11-step run passed and emitted four stable `mission_path`
  identities: TEST, D2X, DOS, and REBIRTH.
- Passed focused PowerShell regression tests, Python normalizer contracts,
  `LevelMetadataTargetsTest`, mission JSON normalization, and scoped code
  quality checks.
