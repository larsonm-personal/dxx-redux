# Plan: Add awareness source instrumentation for replay desync (2026-05-02)

## Goal
- Tag each create_awareness_event caller so replay logs can identify the exact path that consumes the first shifted RNG call

## Steps
- [x] Add a tiny source-tag API in ai.c/ai.h and include source fields in awareness entry/result logs
- [x] Annotate key call sites (collide/laser/game/ai2) with source tags before create_awareness_event
- [x] Rebuild d2 host target and verify no compile errors
- [x] Run replay RNG compare for demo 223549 and extract first awareness entry around frame 425 with source tag
- [x] Summarize root cause evidence and next fix direction

## Findings
- First frame-425 awareness call is tagged source=collide_weapon_wall
- That entry is: frame=425 gt=2314404 type=2 obj=48 source_obj=48 aux_obj=46 state=4062229960
- The immediate gate draw advances state 4062229960 -> 1879082593, which exactly matches the first RNG mismatch call payload at call_count 24872
- This confirms the earliest ordering fork is a weapon-wall awareness event happening before the path random_xlate draw in replay

## Root Cause Direction
- The replay run introduces (or schedules earlier) a player weapon wall-impact awareness event at frame 425
- That consumes the first diverging RNG draw in create_awareness_event and shifts subsequent path/AI random consumers
- Next fix should target event ordering parity for player weapon wall collision handling versus path generation in that frame window
