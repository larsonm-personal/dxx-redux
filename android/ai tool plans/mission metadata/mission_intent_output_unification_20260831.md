# Mission intent output unification plan

## Goal

Make every mission metadata regression-generation path emit the structured
`mission_intent` object, with one mission-wide classification and aggregate
counts, instead of allowing a reduced or level-counter-only representation to
replace it.

## Investigation

- [x] Inventory the native headless, JNI/emulator, host normalization, and
  checked-regression writers.
- [x] Identify the exact path that drops or flattens `mission_intent` and
  explain why alternating regeneration commands produce different output.
- [x] Check history and existing tests for any deliberate request to disable
  mission intent output.

## Implementation

- [x] Make the structured native result the source of truth in every writer.
- [x] Preserve deterministic field order and normalization for checked JSON.
- [x] Remove obsolete reduced mission-intent projection code where safe.
- [x] Add regression coverage that rejects scalar, missing, or level-only
  mission classification output.

## Validation

- [x] Run focused host generation and verify the full type 2 object.
- [x] Run the corresponding emulator/JNI serialization tests.
- [x] Run scoped code quality, native tests/builds, and Android tests as needed.
- [x] Record the root cause and validation evidence here.

## Result

Two independent projection bugs caused the oscillation:

1. `regenerate_all_mission_metadata_host.ps1` reduced the native structured
   object to its `classification` string and omitted the per-level start,
   powerup, and reactor evidence fields. Emulator regeneration copied the full
   object, so whichever command ran last selected the checked schema.
2. The extracted-mission target path in `MissionZipExtractionStore` omitted
   parsed descriptor mode flags. Android therefore used structural fallback
   rules while host generation used descriptor rules, even after both emitted
   structured objects.

The host projection now writes the complete ordered object and the same
per-level evidence fields as Android. Extracted targets retain descriptor mode
flags. Mission-metadata normalization rejects scalar or incomplete intent
objects, and a corpus test checks every checked mission.

The full host regeneration completed with 132 sources passed, one archive with
no descriptor skipped, and zero failures. All 384 checked mission entries now
have structured intent objects; none are scalar or missing. Focused Castaway
host and emulator output now match exactly, including `descriptor_campaign`,
the declaration flags, and all aggregate counts. PowerShell/Python schema tests,
metadata statistics/travel tests, focused Android unit tests, debug APK assembly,
and scoped code quality pass.
