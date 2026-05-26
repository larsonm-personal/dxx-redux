# Guidebot Coop Resume Desync Investigation - 2026-05-26

## Goal
- Investigate why a cooperative network game resumed from a save can desync the guidebot
- Identify a likely cause if visible from code inspection
- If no clear cause is found, propose targeted logging to reproduce from the same save file

## Plan
- [x] Read save/restore flow for network games and guidebot/buddy state
- [x] Trace guidebot AI state synchronization in cooperative multiplayer
- [x] Look for saved state, random state, object ordering, or ownership gaps specific to restored games
- [x] Summarize likely causes and practical logging probes
