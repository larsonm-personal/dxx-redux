# Fly-through exit metadata and guidebot plan

## Goal
Handle reactorless community levels that still have a reachable exit, using KCX F2 Remastered level 1 as the motivating case.

## Steps
- [x] Read repository guidance and create this plan.
- [x] Find the metadata/travel-time analysis path and how "missing reactor" notes are produced.
- [x] Inspect KCXF2RM level data enough to confirm the exit path representation.
- [x] Update metadata logic so reachable reactorless exits are labeled "no reactor, exit exists" and included in travel-time output.
- [x] Update guidebot objective selection so no-key/no-reactor levels can navigate directly to the exit.
- [x] Add or update focused tests/fixtures where practical.
- [x] Run scoped formatting/quality checks and relevant tests.

## Notes
- Existing local edits are present in gameplay and Android files. Do not revert or overwrite unrelated changes.
