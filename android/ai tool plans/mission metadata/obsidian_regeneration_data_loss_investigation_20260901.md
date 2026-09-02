# Obsidian regeneration data-loss investigation

## Plan

- [x] Characterize the `Obsidian.json` diff and identify every affected field family
- [x] Compare current Windows analyzer output with the prior checked-in representation
- [x] Trace MIDI metadata and key-mask values through native output, Kotlin projection, and PowerShell orchestration
- [x] Establish the root cause and determine whether other regenerated missions are affected
- [x] Report findings and propose the smallest corrective change without rewriting regression data

## Findings

- The Windows worker's shared `jni_level_metadata.cpp` serializer emits route
  steps but omits `route_required_key_mask` and
  `route_completing_key_mask_set`.
- The Kotlin projection treats missing integer properties as zero, producing
  the widespread mask churn in checked-in output.
- The former headless metadata serializer explicitly emitted both fields.
- The current diff contains 1,308 nonzero-to-zero mask changes across 126
  mission JSON files, including 18 levels in `Obsidian.json`.
- Obsidian MIDI metadata was not deleted. Its 14 projected `track_names`
  entries are identical to `HEAD`, including their normalized SHA-256 digest.
- The targeted Windows runner wiring test does not validate a key-bearing
  native result, so it could not detect the omitted native properties.

## Proposed correction

Emit both mask fields from the shared JNI/native level serializer, require the
properties in the Kotlin projection instead of silently defaulting them, and
add a Windows regeneration fixture with nonzero masks. Then regenerate the
affected corpus from the corrected worker and review real routing changes
separately from serializer churn.

## Implementation

- [x] Emit both route-mask fields for successful and failed native level rows
- [x] Make the Kotlin projection reject missing route-mask fields
- [x] Add regression coverage for native serialization and projection validation
- [x] Rebuild the Windows metadata workers and Kotlin CLI
- [x] Regenerate Obsidian in isolation and verify all expected masks and MIDI tracks
- [x] Run scoped quality checks and relevant tests

The corrected full Windows corpus run completed with 127 passes, one expected
no-descriptor skip, and zero failures. All native level rows contained both
mask properties. The suspicious nonzero-to-zero corpus changes fell from
1,308 to 75; the remaining values were explicitly produced by the current
planner rather than introduced by missing serialization fields.
