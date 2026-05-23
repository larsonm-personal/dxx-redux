# Plan: robot spawn state audit 2026-05-02

## Goal

Verify whether robot spawn-point/materialization runtime state is fully captured by save+checkpoint restore paths

## Steps

- [x] Map robot spawn state structs and globals used by fuelcen/matcen logic
- [x] Trace save and load serialization paths in state.c
- [x] Check post-restore hooks for spawn-state rebuild or clobbering
- [x] Audit additional robot respawn systems for unsaved runtime globals
- [x] Summarize saved coverage and concrete gaps with file evidence

## Notes

- Checkpoint demos rely on state_save_all_sub and state_restore_all_sub
- Main matcen state is serialized via RobotCenters and Station arrays
- Multiplayer respawn wave globals in multibot.c appear unsaved
