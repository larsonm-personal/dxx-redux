# Ulterior Rebirth Variant Selection Audit

## Goal

Determine whether the launcher currently guides users toward the Rebirth
mission files in the Ulterior package, and identify the cleanest deterministic
selection and presentation behavior if it does not.

## Plan

1. [Complete] Inspect the Ulterior archive descriptors and generated metadata.
2. [Complete] Trace launcher classification, display, and mission-choice behavior.
3. [Complete] Check existing compatibility or preference rules for Rebirth variants.
4. [Complete] Report current behavior and recommend a scoped solution.

## Findings

- The archive contains TEST, D2X, DOS, and REBIRTH mission sets. All three
  Ulterior descriptors have identical titles and level lists, while their HOG
  payloads differ. The README calls REBIRTH definitive and recommends it when
  supported.
- `MissionZip` deliberately preserves all three sibling variants as independent
  mission sets. The native engine recursively enumerates every staged mission
  descriptor, so all variants are available at launch.
- No production preference rule selects REBIRTH. The first sorted mission is
  D2X and becomes the imported mission summary. The details UI shows only that
  first mission and labels constituent buttons with leaf names, so repeated
  `ULTERIOR.MN2` and `ULTERIOR.HOG` entries do not expose their parent variant.
- The README is available from the details dialog, but the user must open and
  interpret it. This is the only current guidance.
- The existing `missionSets` model and archive-relative mission paths provide
  enough identity to add a generic variant policy without hard-coding Ulterior.

## Recommendation

Detect sibling variant sets only when recognized variant directories contain
same-named, structurally equivalent mission descriptors with paired payloads.
For DXX Redux, select REBIRTH by default, stage only that variant for normal
game launch, and retain the complete archive plus all scanned sets for metadata
and inspection. Show `Selected variant: Rebirth (recommended for DXX Redux)`
and list the unselected D2X, DOS, and TEST sets in the details dialog. An
advanced override can be added later if cross-port multiplayer use requires it.

## Corpus Survey Plan

1. [Complete] Inventory all maintained ZIP and 7z mission archives and their
   mission descriptors.
2. [Complete] Group same-named descriptors across sibling directories and compare
   titles, level lists, paired payloads, and directory labels.
3. [Complete] Audit distinct multi-mission and TEST-directory packages for false
   matches under the proposed rule.
4. [Complete] Refine the launch-selection recommendation from corpus evidence.

## Corpus Survey Results

- Scanned 128 of 128 maintained archives: 106 ZIP and 22 7z files, totaling
  1.94 GiB. Inspection found 212 mission descriptors with no listing or decode
  failures.
- Thirteen archives intentionally contain multiple descriptors. Examples
  include Descent Maximum plus its anarchy mission, hard and normal Quotiency,
  Bahagad plus its easy version, and multi-mission collections. Their descriptor
  basenames or parsed mission identities differ and must remain independent.
- Only `ulterior_v1.0.6b.7z` contains the same descriptor basename in multiple
  sibling directories. Its D2X, DOS, and REBIRTH `ULTERIOR.MN2` descriptors have
  the same title and level list, each has a paired same-directory HOG, and their
  HOG payloads differ.
- Ulterior is also the only archive with a descriptor under a TEST directory.
  The test descriptor has a different basename, title, and level list, so it is
  not part of the sibling variant equivalence group.
- `ewithin-versions.zip` contains no top-level descriptors and has one nested
  `ewithin-rebirth.zip`. Existing production code already selects and imports
  that sole Rebirth child, so the sibling-descriptor policy does not affect it.
- A strict heuristic requiring duplicate descriptor basename, equivalent parsed
  mission identity, paired same-stem payloads, sibling recognized variant
  directories, and a REBIRTH plus D2X or DOS choice matched exactly one group:
  Ulterior's three main variants.

## Refined Safety Rules

1. Preserve every parsed mission set in scan and metadata results.
2. Create a launch variant group only when sibling sets share the descriptor
   basename, game, mission type, title, normal level list, secret level list,
   and each has a paired same-directory mission payload.
3. Require recognized sibling labels and a REBIRTH member plus at least one D2X
   or DOS member. Do not infer variants from title similarity alone.
4. Prefer REBIRTH for DXX Redux. Stage shared files and the selected set only;
   keep other sets visible as unselected package contents.
5. Do not globally suppress TEST directories. Suppress Ulterior's auxiliary
   TEST set from normal launch only as part of an accepted variant bundle, and
   retain it for details and metadata analysis.
6. Leave the existing nested-Rebirth archive importer unchanged.

## Implementation Plan

1. [Complete] Add a strict generic sibling-variant selection model while
   preserving the complete scanned mission-set inventory.
2. [Complete] Limit native launch exposure to the selected Rebirth set and shared
   package content without affecting unrelated multi-mission archives.
3. [Complete] Present the selected and excluded variants in launcher details with
   archive-relative paths.
4. [Complete] Add focused scanner and staging regression tests. The details UI
   uses the same tested selection model and shows archive-relative paths.
5. [Complete] Run scoped formatting, unit tests, build verification, and a focused
   production scan of the maintained Ulterior archive.

## Verification

- Scoped code quality checks and direct ktlint checks passed.
- Focused scanner and extraction tests passed, including equivalent variants,
  differing variants, unknown sibling labels, auxiliary TEST content, and an
  unrelated mission alongside a selected variant bundle.
- The production scanner inspected the maintained 71.8 MB Ulterior 7z and kept
  all four sets while selecting only `REBIRTH/ULTERIOR.MN2` for launch.
- The complete Android debug unit suite passed 939 tests with no failures, and
  the debug APK assembled successfully.
- Windows D1 and D2 CMake builds completed. D2 passed all 43 registered CTest
  tests; the D1 build has no registered CTest tests in this configuration.

## Variant Precedence Follow-up

1. [Complete] Centralize the recognized variant mask order as one precedence
   array.
2. [Complete] Generalize selection so the highest available variant masks every
   lower-precedence equivalent sibling, including DOS masking D2X when Rebirth
   is absent.
3. [Complete] Add focused precedence and fallback tests.
4. [Complete] Run scoped formatting, Android unit tests, and build verification.

The follow-up preserves the original strict equivalence and fail-closed checks,
but now selects the first available entry from `MISSION_VARIANT_PRECEDENCE`.
Focused precedence tests and the complete 939-test Android suite passed, and the
debug APK assembled successfully.

## Nested Archive Precedence Follow-up

1. [Complete] Trace the nested archive behavior used by
   `ewithin-versions.zip` and inspect both maintained child archives.
2. [Complete] Move directory and nested-archive aliases, display labels,
   support status, and mask order into one shared policy array.
3. [Complete] Replace the hard-coded nested Rebirth selector and import-candidate
   check with shared policy selection.
4. [Complete] Add nested DOS/D2X precedence, ambiguity, unsupported XL-only, and
   maintained Enemy Within coverage.
5. [Complete] Run scoped formatting, unit tests, actual-archive verification, and
   Android build verification.

The shared `MISSION_VARIANT_MASK_PRECEDENCE` array now defines Rebirth, DOS,
D2X, and D2X-XL aliases in mask order. D2X-XL is recognized as unsupported, so
the maintained Enemy Within package selects its Rebirth child while an XL-only
wrapper remains non-importable. Candidate detection and import use the same
fail-closed selector. The actual archive contains exactly
`ewithin-rebirth.zip` and `ewithin-xl.zip`; focused tests passed, the complete
944-test Android suite passed, and the debug APK assembled successfully.

## Contained-Mission Metadata Masking Follow-up

1. [Complete] Trace contained-mission regression serialization and launcher
   metadata-browser consumption.
2. [Complete] Apply the shared effective mission-set masking view to generated
   contained-mission metadata.
3. [Complete] Apply the same view to the launcher metadata browser without
   changing unrelated multi-mission packages.
4. [Complete] Add focused serialization and browser-model regression coverage.
5. [Complete] Regenerate the maintained Ulterior metadata and run scoped quality,
   Android tests, and build verification.

`effectiveMissionSets` is now the single Android view used for native staging,
contained-mission analysis, automation serialization, and metadata-browser
targets. The host regenerator mirrors the centralized directory precedence and
uses strict fail-closed matching before flattening archives, so it selects the
same Rebirth descriptor and HOG rather than whichever duplicate basename is
enumerated first. Its `mission_path` output now matches the emulator schema.

The maintained Ulterior JSON contains one 26-level record for
`REBIRTH/ULTERIOR.MN2`. Emulator regeneration passed, PowerShell 7 and Windows
PowerShell 5.1 host regenerations produced byte-identical output, scoped quality
checks passed, the complete 945-test Android suite passed, and the debug APK
assembled successfully. Mission metadata floating-point normalization now uses
14 significant digits to collapse the one-ULP serialization differences found
between PowerShell 5.1 and 7.

## Floating-Point Churn Audit

1. [Complete] Compare the checked Ulterior metadata with the pre-rounding
   emulator artifact and classify every changed field.
2. [Complete] Trace the exact formatter and regeneration step that introduced the
   edits.
3. [Complete] Report the cause and a lower-churn correction without changing the
   regression file during this diagnostic pass.

The churn came from the newly added 14-significant-digit normalization, not
from mission analysis. It changed 608 otherwise stable emulator values: 152
route distances, 380 route-position coordinates, 25 mine volumes, 26 normalized
mine volumes, and 25 travel distances. Before that rounding, emulator and
PowerShell 7 metadata were byte-identical. Windows PowerShell 5.1's
`ConvertFrom-Json` parsed only 15 values to the adjacent IEEE-754 double, which
the broad rounding workaround expanded into hundreds of edits. The lower-churn
fix is to remove the broad rounding and preserve raw analyzer numeric lexemes
through the PowerShell 5.1 host conversion path.

## Gameplay-Scale Precision Audit

1. [Complete] Establish player-ship dimensions in native game units.
2. [Complete] Measure the numeric range of checked-in mission metadata fields.
3. [Complete] Recommend the shortest stable precision that stays comfortably
   below one tenth of a ship length.

The D2 ship collision radius is 310325 in 16.16 fixed-point units, or about
4.735 game units, making the engine-relevant collision diameter about 9.47
units and one tenth about 0.947 units. Across 39,640 checked-in linear metadata
values, five significant digits had a worst absolute error of 0.499 units;
four significant digits failed the threshold for 168 values. Six significant
digits reduced the worst error to 0.0499 units. Because the tolerance is
absolute rather than relative, one decimal place is the clearer policy for
positions and distances: its maximum rounding error is 0.05 units. Volumes and
dimensionless normalized volumes should use a separate relative rule, such as
six significant digits.

## Gameplay-Scale Precision Implementation

1. [Complete] Replace blanket significant-digit rounding with field-aware
   linear and volume precision rules.
2. [Complete] Extend normalization tests for precision, signed zero, idempotence,
   and PowerShell-version convergence.
3. [Complete] Regenerate Ulterior and compare emulator, PowerShell 7, and Windows
   PowerShell 5.1 results.
4. [Complete] Run scoped quality checks and relevant regression tests.

Linear fields now round to one decimal place and canonicalize signed zero;
mine volume and normalized mine volume round to six significant digits. The
Ulterior comparison measured a maximum linear error of 0.0499726 game units
and a maximum volume relative error of 0.00000471. PowerShell 5.1, PowerShell 7,
and normalized emulator output were byte-identical. The regenerated single
Rebirth record is 113609 bytes. Scoped quality, Python contract tests, and the
normalization test under both PowerShell versions passed.
