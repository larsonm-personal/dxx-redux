# Multiplayer Desync Log Analysis - 2026-06-16

## Context
- User saw intermittent multiplayer desync in a longer D2 playthrough.
- Symptoms: player B could see/pick up player A death spew that player A did not see/pick up, and a shootable switch dropped a forcefield on only one player's screen.
- The events should be covered by two logs in `game_data/logs_net_failure`.

## Plan
- [x] Inventory and summarize the two logs
- [x] Search for object/spew/pickup divergences
- [x] Search for wall/trigger/forcefield divergence clues
- [x] Cross-check suspicious log events against network/object code paths
- [x] Report likely causes and best instrumentation points
