# Lunar Series metadata failure JSON plan

## Goal
Investigate `game_data/mission_files/Lunar Series Revamped.json`, determine why it contains only a failure record, and fix or regenerate the metadata path so Lunar no longer leaves a failing mission JSON.

## Work phases
1. [ ] Identify the producer of `game_data/mission_files/*.json` metadata files.
2. [ ] Reproduce or inspect the Lunar metadata generation path after the native level-name fix.
3. [ ] Update code or generated output so failed analysis is not left as mission metadata.
4. [ ] Run focused verification.
