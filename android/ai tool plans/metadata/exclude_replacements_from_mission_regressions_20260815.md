# Exclude replacement details from mission regression data

Date: 2026-08-15
Status: implemented and validated

## Request

- Omit `replacements` and `replacement_groups` from checked-in mission metadata
  JSON because they add substantial size without useful regression coverage

## Plan

- [x] Locate the shared normalization boundary for Android and host regeneration
- [x] Exclude both fields without removing app-facing replacement metadata
- [x] Add regression coverage for the compact checked-in schema
- [x] Regenerate Obsidian and validate the mission corpus and generators

## Results

- Mission-metadata normalization removes `replacements` and
  `replacement_groups` from both Android and host regression output
- The host checked-in projection no longer constructs the excluded fields
- Obsidian shrank from 916,636 bytes to 120,472 bytes while retaining its route
  data, including the corrected level 4 route
- No checked-in mission metadata JSON contains either excluded field
- Normalization, regeneration-runner, scoped quality, and the 1,561-route corpus
  tests pass

## Constraints

- Preserve replacement metadata used by launcher UI and robot preview features
- Keep Android and host-generated regression JSON normalized and equivalent
- Preserve unrelated working-tree changes
